///////////////////////////////////////
/// 640x480 version! 16-bit color
/// This code will segfault the original
/// DE1 computer
/// compile with
/// gcc graphics_video_16bit.c -o gr -O2 -lm
///
///////////////////////////////////////
#include <bits/pthreadtypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/mman.h>
#include <sys/time.h> 
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
//#include "address_map_arm_brl4.h"

// video display
#define SDRAM_BASE            0xC0000000
#define SDRAM_END             0xC3FFFFFF
#define SDRAM_SPAN			  0x04000000
// characters
#define FPGA_CHAR_BASE        0xC9000000 
#define FPGA_CHAR_END         0xC9001FFF
#define FPGA_CHAR_SPAN        0x00002000
/* Cyclone V FPGA devices */
#define HW_REGS_BASE          0xff200000
//#define HW_REGS_SPAN        0x00200000 
#define HW_REGS_SPAN          0x00005000 

// PIO ports
#define FPGA_AXI_BASE 	0xC4000000
#define FPGA_AXI_SPAN   0x00001000
#define FPGA_PIO_X      0x00000000
#define FPGA_PIO_Y		0x00000010
#define FPGA_PIO_Z		0x00000020
#define FPGA_PIO_RESET  0x00000030
#define FPGA_PIO_CLK    0x00000040
#define FPGA_PIO_X_INIT 0x00000050
#define FPGA_PIO_Y_INIT 0x00000060
#define FPGA_PIO_Z_INIT 0x00000070
#define FPGA_PIO_SIGMA  0x00000080
#define FPGA_PIO_BETA   0x00000090
#define FPGA_PIO_RHO    0x000000A0


// graphics primitives
void VGA_text (int, int, char *);
void VGA_text_clear();
void VGA_box (int, int, int, int, short);
void VGA_rect (int, int, int, int, short);
void VGA_line(int, int, int, int, short) ;
void VGA_Vline(int, int, int, short) ;
void VGA_Hline(int, int, int, short) ;
void VGA_disc (int, int, int, short);
void VGA_circle (int, int, int, int);


// threads 
void* frame_update(void* arg);
void* console_input(void* arg);

// 16-bit primary colors
#define red  (0+(0<<5)+(31<<11))
#define dark_red (0+(0<<5)+(15<<11))
#define green (0+(63<<5)+(0<<11))
#define dark_green (0+(31<<5)+(0<<11))
#define blue (31+(0<<5)+(0<<11))
#define dark_blue (15+(0<<5)+(0<<11))
#define yellow (0+(63<<5)+(31<<11))
#define cyan (31+(63<<5)+(0<<11))
#define magenta (31+(0<<5)+(31<<11))
#define black (0x0000)
#define gray (15+(31<<5)+(51<<11))
#define white (0xffff)
int colors[] = {red, dark_red, green, dark_green, blue, dark_blue, 
		yellow, cyan, magenta, gray, black, white};

// pixel macro
#define VGA_PIXEL(x,y,color) do{\
	int  *pixel_ptr ;\
	pixel_ptr = (int*)((char *)vga_pixel_ptr + (((y)*640+(x))<<1)) ; \
	*(short *)pixel_ptr = (color);\
} while(0)

// axi bus base
void *h2p_virtual_base;
volatile signed int * axi_pio_X_ptr = NULL ;
volatile signed int * axi_pio_Y_ptr = NULL ;
volatile signed int * axi_pio_Z_ptr = NULL ;
volatile unsigned int * axi_pio_clk_out_ptr = NULL   ;
volatile unsigned int * axi_pio_reset_out_ptr = NULL ;
volatile signed int * axi_pio_x_init_ptr = NULL ;
volatile signed int * axi_pio_y_init_ptr = NULL ;
volatile signed int * axi_pio_z_init_ptr = NULL ;
volatile signed int * axi_pio_sigma_ptr = NULL ;
volatile signed int * axi_pio_beta_ptr = NULL ;
volatile signed int * axi_pio_rho_ptr = NULL  ;




// the light weight bus base
void *h2p_lw_virtual_base;

// pixel buffer
volatile unsigned int * vga_pixel_ptr = NULL ;
void *vga_pixel_virtual_base;

// character buffer
volatile unsigned int * vga_char_ptr = NULL ;
void *vga_char_virtual_base;

// /dev/mem file id
int fd;

// measure time
struct timeval t1, t2;
double elapsedTime;

// convert 7.20 but in 32 bit with sign 5 0's then 7 decimal bits and 20 fractional. Now its doing fixed to int * 4
#define fixed_to_int(x) ((int)x >> 18)


// convert float to 7.20 fixed point with 5 padded 0's for the LSB so its 32 bit
#define float_to_fixed(x) ((signed int)(x * 1048576.0f)) << 5

bool running = true;

int main(void)
{
  	
	// === need to mmap: =======================
	// FPGA_CHAR_BASE
	// FPGA_ONCHIP_BASE      
	// HW_REGS_BASE        
  
	// === get FPGA addresses ==================
    // Open /dev/mem
	if( ( fd = open( "/dev/mem", ( O_RDWR | O_SYNC ) ) ) == -1 ) 	{
		printf( "ERROR: could not open \"/dev/mem\"...\n" );
		return( 1 );
	}
    
	// get virtual address for AXI bus  
	h2p_virtual_base = mmap( NULL, FPGA_AXI_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, FPGA_AXI_BASE); 	
	if( h2p_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap3() failed...\n" );
		close( fd );
		return(1);
	}

    // Get the addresses that map to the two parallel ports on the AXI bus
	axi_pio_X_ptr =(signed int *)(h2p_virtual_base + FPGA_PIO_X);
	axi_pio_Y_ptr =(signed int *)(h2p_virtual_base + FPGA_PIO_Y);
	axi_pio_Z_ptr =(signed int *)(h2p_virtual_base + FPGA_PIO_Z);
	axi_pio_clk_out_ptr =(unsigned int *)(h2p_virtual_base + FPGA_PIO_CLK);
	axi_pio_reset_out_ptr =(unsigned int *)(h2p_virtual_base + FPGA_PIO_RESET);

	axi_pio_x_init_ptr =(signed int *)(h2p_virtual_base + FPGA_PIO_X_INIT);
	axi_pio_y_init_ptr =(signed int *)(h2p_virtual_base + FPGA_PIO_Y_INIT);
	axi_pio_z_init_ptr =(signed int *)(h2p_virtual_base + FPGA_PIO_Z_INIT);
	axi_pio_sigma_ptr =(signed int *)(h2p_virtual_base + FPGA_PIO_SIGMA);
	axi_pio_beta_ptr =(signed int *)(h2p_virtual_base + FPGA_PIO_BETA);
	axi_pio_rho_ptr =(signed int *)(h2p_virtual_base + FPGA_PIO_RHO);



    // get virtual addr that maps to physical
	h2p_lw_virtual_base = mmap( NULL, HW_REGS_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, HW_REGS_BASE );	
	if( h2p_lw_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap1() failed...\n" );
		close( fd );
		return(1);
	}
    

	// === get VGA char addr =====================
	// get virtual addr that maps to physical
	vga_char_virtual_base = mmap( NULL, FPGA_CHAR_SPAN, ( 	PROT_READ | PROT_WRITE ), MAP_SHARED, fd, FPGA_CHAR_BASE );	
	if( vga_char_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap2() failed...\n" );
		close( fd );
		return(1);
	}
    
    // Get the address that maps to the FPGA LED control 
	vga_char_ptr =(unsigned int *)(vga_char_virtual_base);

	// === get VGA pixel addr ====================
	// get virtual addr that maps to physical
	vga_pixel_virtual_base = mmap( NULL, SDRAM_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, SDRAM_BASE);	
	if( vga_pixel_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap3() failed...\n" );
		close( fd );
		return(1);
	}
    
    // Get the address that maps to the FPGA pixel buffer
	vga_pixel_ptr =(unsigned int *)(vga_pixel_virtual_base);

	// ===========================================

	/* create a message to be displayed on the VGA 
          and LCD displays */
	char text_top_row[40] = "DE1-SoC ARM/FPGA\0";
	char text_bottom_row[40] = "Cornell ece5760\0";
	char text_next[40] = "Graphics primitives\0";
	char num_string[20], time_string[20] ;
	char color_index = 0 ;
	int color_counter = 0 ;
	
	// position of disk primitive
	int disc_x = 0;
	// position of circle primitive
	int circle_x = 0 ;
	// position of box primitive
	int box_x = 5 ;
	// position of vertical line primitive
	int Vline_x = 350;
	// position of horizontal line primitive
	int Hline_y = 250;

	//VGA_text (34, 1, text_top_row);
	//VGA_text (34, 2, text_bottom_row);
	// clear the screen
	VGA_box (0, 0, 639, 479, 0x0000);
	// clear the text
	VGA_text_clear();
	// write text
	VGA_text (10, 1, text_top_row);
	VGA_text (10, 2, text_bottom_row);
	VGA_text (10, 3, text_next);
	
	// R bits 11-15 mask 0xf800
	// G bits 5-10  mask 0x07e0
	// B bits 0-4   mask 0x001f
	// so color = B+(G<<5)+(R<<11);
	* axi_pio_reset_out_ptr = 1;
	* axi_pio_clk_out_ptr = 0;
	usleep(10);
	* axi_pio_clk_out_ptr = 1;
	usleep(10);
	* axi_pio_reset_out_ptr = 0;
	usleep(10);
	while(1) 
	{
		pthread_t console_thread, frame_thread;
		pthread_create(&frame_thread, NULL, frame_update, NULL);
		pthread_create(&console_thread, NULL, console_input, NULL);

		pthread_join(frame_thread, NULL);
		pthread_join(console_thread, NULL);
	} // end while(1)
} // end main

//
// pthread function to read from the PIO and plot on the VGA
//

float speed = 1; 

void* frame_update(void* arg) {
	while(running) 
	{
		* axi_pio_clk_out_ptr = 0;
		usleep(10 * 1/speed);
		// Get pixel values
		int x = fixed_to_int(*(axi_pio_X_ptr));
		int y = fixed_to_int(*(axi_pio_Y_ptr));
		int z = fixed_to_int(*(axi_pio_Z_ptr));
		
		// DEBUG PRINTING -------------------------
		printf("x: %d\n", x);
		printf("y: %d\n", y);
		printf("z: %d\n", z);
		printf("-----\n");

		printf("RAW FXP VALUES: \n");
		printf("x: %d\n", *axi_pio_X_ptr);
		printf("y: %d\n", *axi_pio_Y_ptr);
		printf("z: %d\n", *axi_pio_Z_ptr);
		printf("-----\n");
		// ---------------------------------------

				// Calculate pixel coordinates
		int xy_x = (int)(x+100);
		int xy_y = (int)(150 - y);
		int xz_x = (int)(x+450);
		int xz_y = (int)(225 - z);
		int yz_x = (int)(y+100);
		int yz_y = (int)(450 - z);
		// Plot XY image in top left quadrant (with bounds checking)
		if (xy_x >= 0 && xy_x < 640 && xy_y >= 0 && xy_y < 480)
			VGA_PIXEL(xy_x, xy_y, colors[1]);		
		
		// Plot XZ image in bottom left quadrant (with bounds checking)
		if (xz_x >= 0 && xz_x < 640 && xz_y >= 0 && xz_y < 480)
			VGA_PIXEL(xz_x, xz_y, colors[3]);

		// Plot YZ image in bottom right quadrant (with bounds checking)
		if (yz_x >= 0 && yz_x < 640 && yz_y >= 0 && yz_y < 480)
			VGA_PIXEL(yz_x, yz_y, colors[5]);
		* axi_pio_clk_out_ptr = 1;
		usleep(10 * 1/speed);

	} 
	return NULL;
}

//
// pthread function to read the console to control speed, init conditions, params
//
void* console_input(void* arg) {
	char input[20];
	float x_init = 0.0;
	float y_init = 0.0;
	float z_init = 0.0;
	float sig = 0.0;
	float beta = 0.0;
	float rho = 0.0;
	while(running) {
		printf("Commands:\n");
		printf(" p - play / pause\n");
		printf(" s - increase speed\n");
		printf(" l - decrease speed\n");
		printf(" c - clear screen\n");
		printf(" g - set sigma\n");
		printf(" b - set beta\n");
		printf(" r - set rho\n");
		printf(" i - set initial conditions\n");
		printf("Enter command: ");
		// get one character input
		fgets(input, 20, stdin);
		switch(input[0]) {

			// play / pause button
			case 'p':
				running = !running;
				break;

			// increase speed 
			case 's':
				speed *= 2;
				break;

			 // decrease speed
			case 'l':
			 	speed /= 2;
				break;
			
			// clear screen
			case 'c':
				VGA_box (0, 0, 639, 479, 0x0000);
				VGA_text_clear();
				break;
			// get value for sigma
			case 'g':
				sig  = scanf("%f", &sig);
				*axi_pio_sigma_ptr = float_to_fixed(sig);
				break;
			
			// get value for beta
			case 'b':
				beta  = scanf("%f", &beta);
				*axi_pio_beta_ptr = float_to_fixed(beta);
				break;
			
			// get value for rho
			case 'r':
				rho  = scanf("%f", &rho);
				*axi_pio_rho_ptr = float_to_fixed(rho);
				break;
			
			// get initial conditions and reset the system
			case 'i':
				printf("Enter initial x, y, z: ");
				running = false;
				scanf("%f %f %f", &x_init, &y_init, &z_init);
				*axi_pio_x_init_ptr = float_to_fixed(x_init);
				*axi_pio_y_init_ptr = float_to_fixed(y_init);
				*axi_pio_z_init_ptr = float_to_fixed(z_init);
				*axi_pio_reset_out_ptr = 1;
				running = true;
				break;
			
			
			// add more cases for different commands
			default:
				printf("Unknown command\n");
		}
	}

	return NULL;
}



/****************************************************************************************
 * Subroutine to send a string of text to the VGA monitor 
****************************************************************************************/
void VGA_text(int x, int y, char * text_ptr)
{
  	volatile char * character_buffer = (char *) vga_char_ptr ;	// VGA character buffer
	int offset;
	/* assume that the text string fits on one line */
	offset = (y << 7) + x;
	while ( *(text_ptr) )
	{
		// write to the character buffer
		*(character_buffer + offset) = *(text_ptr);	
		++text_ptr;
		++offset;
	}
}

/****************************************************************************************
 * Subroutine to clear text to the VGA monitor 
****************************************************************************************/
void VGA_text_clear()
{
  	volatile char * character_buffer = (char *) vga_char_ptr ;	// VGA character buffer
	int offset, x, y;
	for (x=0; x<79; x++){
		for (y=0; y<59; y++){
	/* assume that the text string fits on one line */
			offset = (y << 7) + x;
			// write to the character buffer
			*(character_buffer + offset) = ' ';		
		}
	}
}

/****************************************************************************************
 * Draw a filled rectangle on the VGA monitor 
****************************************************************************************/
#define SWAP(X,Y) do{int temp=X; X=Y; Y=temp;}while(0) 

void VGA_box(int x1, int y1, int x2, int y2, short pixel_color)
{
	char  *pixel_ptr ; 
	int row, col;

	/* check and fix box coordinates to be valid */
	if (x1>639) x1 = 639;
	if (y1>479) y1 = 479;
	if (x2>639) x2 = 639;
	if (y2>479) y2 = 479;
	if (x1<0) x1 = 0;
	if (y1<0) y1 = 0;
	if (x2<0) x2 = 0;
	if (y2<0) y2 = 0;
	if (x1>x2) SWAP(x1,x2);
	if (y1>y2) SWAP(y1,y2);
	for (row = y1; row <= y2; row++)
		for (col = x1; col <= x2; ++col)
		{
			//640x480
			//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
			// set pixel color
			//*(char *)pixel_ptr = pixel_color;	
			VGA_PIXEL(col,row,pixel_color);	
		}
}

/****************************************************************************************
 * Draw a outline rectangle on the VGA monitor 
****************************************************************************************/
#define SWAP(X,Y) do{int temp=X; X=Y; Y=temp;}while(0) 

void VGA_rect(int x1, int y1, int x2, int y2, short pixel_color)
{
	char  *pixel_ptr ; 
	int row, col;

	/* check and fix box coordinates to be valid */
	if (x1>639) x1 = 639;
	if (y1>479) y1 = 479;
	if (x2>639) x2 = 639;
	if (y2>479) y2 = 479;
	if (x1<0) x1 = 0;
	if (y1<0) y1 = 0;
	if (x2<0) x2 = 0;
	if (y2<0) y2 = 0;
	if (x1>x2) SWAP(x1,x2);
	if (y1>y2) SWAP(y1,y2);
	// left edge
	col = x1;
	for (row = y1; row <= y2; row++){
		//640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;	
		VGA_PIXEL(col,row,pixel_color);		
	}
		
	// right edge
	col = x2;
	for (row = y1; row <= y2; row++){
		//640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;	
		VGA_PIXEL(col,row,pixel_color);		
	}
	
	// top edge
	row = y1;
	for (col = x1; col <= x2; ++col){
		//640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;	
		VGA_PIXEL(col,row,pixel_color);
	}
	
	// bottom edge
	row = y2;
	for (col = x1; col <= x2; ++col){
		//640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;
		VGA_PIXEL(col,row,pixel_color);
	}
}

/****************************************************************************************
 * Draw a horixontal line on the VGA monitor 
****************************************************************************************/
#define SWAP(X,Y) do{int temp=X; X=Y; Y=temp;}while(0) 

void VGA_Hline(int x1, int y1, int x2, short pixel_color)
{
	char  *pixel_ptr ; 
	int row, col;

	/* check and fix box coordinates to be valid */
	if (x1>639) x1 = 639;
	if (y1>479) y1 = 479;
	if (x2>639) x2 = 639;
	if (x1<0) x1 = 0;
	if (y1<0) y1 = 0;
	if (x2<0) x2 = 0;
	if (x1>x2) SWAP(x1,x2);
	// line
	row = y1;
	for (col = x1; col <= x2; ++col){
		//640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;	
		VGA_PIXEL(col,row,pixel_color);		
	}
}

/****************************************************************************************
 * Draw a vertical line on the VGA monitor 
****************************************************************************************/
#define SWAP(X,Y) do{int temp=X; X=Y; Y=temp;}while(0) 

void VGA_Vline(int x1, int y1, int y2, short pixel_color)
{
	char  *pixel_ptr ; 
	int row, col;

	/* check and fix box coordinates to be valid */
	if (x1>639) x1 = 639;
	if (y1>479) y1 = 479;
	if (y2>479) y2 = 479;
	if (x1<0) x1 = 0;
	if (y1<0) y1 = 0;
	if (y2<0) y2 = 0;
	if (y1>y2) SWAP(y1,y2);
	// line
	col = x1;
	for (row = y1; row <= y2; row++){
		//640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;	
		VGA_PIXEL(col,row,pixel_color);			
	}
}


/****************************************************************************************
 * Draw a filled circle on the VGA monitor 
****************************************************************************************/

void VGA_disc(int x, int y, int r, short pixel_color)
{
	char  *pixel_ptr ; 
	int row, col, rsqr, xc, yc;
	
	rsqr = r*r;
	
	for (yc = -r; yc <= r; yc++)
		for (xc = -r; xc <= r; xc++)
		{
			col = xc;
			row = yc;
			// add the r to make the edge smoother
			if(col*col+row*row <= rsqr+r){
				col += x; // add the center point
				row += y; // add the center point
				//check for valid 640x480
				if (col>639) col = 639;
				if (row>479) row = 479;
				if (col<0) col = 0;
				if (row<0) row = 0;
				//pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
				// set pixel color
				//*(char *)pixel_ptr = pixel_color;
				VGA_PIXEL(col,row,pixel_color);	
			}
					
		}
}

/****************************************************************************************
 * Draw a  circle on the VGA monitor 
****************************************************************************************/

void VGA_circle(int x, int y, int r, int pixel_color)
{
	char  *pixel_ptr ; 
	int row, col, rsqr, xc, yc;
	int col1, row1;
	rsqr = r*r;
	
	for (yc = -r; yc <= r; yc++){
		//row = yc;
		col1 = (int)sqrt((float)(rsqr + r - yc*yc));
		// right edge
		col = col1 + x; // add the center point
		row = yc + y; // add the center point
		//check for valid 640x480
		if (col>639) col = 639;
		if (row>479) row = 479;
		if (col<0) col = 0;
		if (row<0) row = 0;
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;
		VGA_PIXEL(col,row,pixel_color);	
		// left edge
		col = -col1 + x; // add the center point
		//check for valid 640x480
		if (col>639) col = 639;
		if (row>479) row = 479;
		if (col<0) col = 0;
		if (row<0) row = 0;
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;
		VGA_PIXEL(col,row,pixel_color);	
	}
	for (xc = -r; xc <= r; xc++){
		//row = yc;
		row1 = (int)sqrt((float)(rsqr + r - xc*xc));
		// right edge
		col = xc + x; // add the center point
		row = row1 + y; // add the center point
		//check for valid 640x480
		if (col>639) col = 639;
		if (row>479) row = 479;
		if (col<0) col = 0;
		if (row<0) row = 0;
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;
		VGA_PIXEL(col,row,pixel_color);	
		// left edge
		row = -row1 + y; // add the center point
		//check for valid 640x480
		if (col>639) col = 639;
		if (row>479) row = 479;
		if (col<0) col = 0;
		if (row<0) row = 0;
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;
		VGA_PIXEL(col,row,pixel_color);	
	}
}

// =============================================
// === Draw a line
// =============================================
//plot a line 
//at x1,y1 to x2,y2 with color 
//Code is from David Rodgers,
//"Procedural Elements of Computer Graphics",1985
void VGA_line(int x1, int y1, int x2, int y2, short c) {
	int e;
	signed int dx,dy,j, temp;
	signed int s1,s2, xchange;
     signed int x,y;
	char *pixel_ptr ;
	
	/* check and fix line coordinates to be valid */
	if (x1>639) x1 = 639;
	if (y1>479) y1 = 479;
	if (x2>639) x2 = 639;
	if (y2>479) y2 = 479;
	if (x1<0) x1 = 0;
	if (y1<0) y1 = 0;
	if (x2<0) x2 = 0;
	if (y2<0) y2 = 0;
        
	x = x1;
	y = y1;
	
	//take absolute value
	if (x2 < x1) {
		dx = x1 - x2;
		s1 = -1;
	}

	else if (x2 == x1) {
		dx = 0;
		s1 = 0;
	}

	else {
		dx = x2 - x1;
		s1 = 1;
	}

	if (y2 < y1) {
		dy = y1 - y2;
		s2 = -1;
	}

	else if (y2 == y1) {
		dy = 0;
		s2 = 0;
	}

	else {
		dy = y2 - y1;
		s2 = 1;
	}

	xchange = 0;   

	if (dy>dx) {
		temp = dx;
		dx = dy;
		dy = temp;
		xchange = 1;
	} 

	e = ((int)dy<<1) - dx;  
	 
	for (j=0; j<=dx; j++) {
		//video_pt(x,y,c); //640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (y<<10)+ x; 
		// set pixel color
		//*(char *)pixel_ptr = c;
		VGA_PIXEL(x,y,c);			
		 
		if (e>=0) {
			if (xchange==1) x = x + s1;
			else y = y + s2;
			e = e - ((int)dx<<1);
		}

		if (xchange==1) y = y + s2;
		else x = x + s1;

		e = e + ((int)dy<<1);
	}
}