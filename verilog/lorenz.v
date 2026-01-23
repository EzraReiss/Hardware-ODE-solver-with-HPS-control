/////////////////////////////////////////////////
//// integrator /////////////////////////////////
/////////////////////////////////////////////////

module integrator(out, funct, InitialOut, clk, reset);
	output signed [26:0] out; 		//the state variable V
	input signed [26:0] funct;      //the dV/dt function
	input clk, reset;
	input signed [26:0] InitialOut;  //the initial state variable V
	
	wire signed	[26:0] out, v1new ;
	reg signed	[26:0] v1 ;
	
	always @ (posedge clk) 
	begin
		if (reset==1) //reset	
			v1 <= InitialOut ; // 
		else 
			v1 <= v1new ;	
	end
	assign v1new = v1 + funct ;
	assign out = v1 ;
endmodule

//////////////////////////////////////////////////
//// signed mult of 7.20 format 2'comp////////////
//////////////////////////////////////////////////

module signed_mult (out, a, b);
	output 	signed  [26:0]	out;
	input 	signed	[26:0] 	a;
	input 	signed	[26:0] 	b;
	// intermediate full bit length
	wire 	signed	[53:0]	mult_out;
	assign mult_out = a * b;
	// select bits for 7.20 fixed point
	assign out = {mult_out[53], mult_out[45:20]};
endmodule
//////////////////////////////////////////////////


module lorenz_comb
(
    input signed  [26:0] xk,
    input signed  [26:0] yk,
    input signed  [26:0] zk,
    input signed  [26:0] sigma,
    input signed  [26:0] rho,
    input signed  [26:0] beta,
    input signed  [26:0] dt,
    output signed [26:0] x_comb,
    output signed [26:0] y_comb,
    output signed [26:0] z_comb
);
    
    
    signed wire [26:0] x_dt;
    signed wire [26:0] y_dt;
    signed wire [26:0] z_dt;
    signed wire [26:0] rho_dt;
    signed wire [26:0] beta_dt;
    signed wire [26:0] sigma_dt;

    signed_mult dt_x (
        .out(x_dt),
        .a(dt),
        .b(xk)
    );

    signed_mult dt_y (
        .out(y_dt),
        .a(dt),
        .b(yk)
    );

    signed_mult dt_z (
        .out(z_dt),
        .a(dt),
        .b(zk)
    );

    signed_mult dt_rho (
        .out(rho_dt),
        .a(dt),
        .b(rho)
    );

    signed_mult dt_beta (
        .out(beta_dt),
        .a(dt),
        .b(beta)
    );

    signed_mult dt_sigma (
        .out(sigma_dt),
        .a(dt),
        .b(sigma)
    );


    //Make xnext
    signed wire [26:0] delt_yx;
    assign delt_yx = y_dt - x_dt; 
    
    signed_mult sigma_mult (
        .out(x_comb), 
        .a(sigma), 
        .b(delt_yx)
    );

    //make ynext
    signed wire [26:0] yx_mul;

    signed_mult xy_mult (
        .out(yx_mul), 
        .a(y_dt), 
        .b(xk)
    );
    
    signed wire [26:0] zb_mul;

    signed_mult ab_mult (
        .out(zb_mul), 
        .a(z_dt), 
        .b(beta)
    );

    assign z_comb = yx_mul - zb_mul;

    signed wire [26:0] delt_zr;

    assign delt_zr = rho - zk;

    signed wire [26:0] delt_zrx_mul;

    signed_mult drz_x_mult (
        .out(delt_zrx_mul), 
        .a(x_dt), 
        .b(delt_zr)
    );

    assign y_comb = delt_zrx_mul - y_dt;    
endmodule


module lorenz (
        input clk,
        input reset,
        input signed   [26:0] xo,
        input signed   [26:0] yo,
        input signed   [26:0] zo,
        input signed   [26:0] sigma,
        input signed   [26:0] rho,
        input signed   [26:0] beta,
        input signed   [26:0] dt,
        output signed  [26:0] x_out,
        output signed  [26:0] y_out,
        output signed  [26:0] z_out
    );

    wire signed [26:0] x_comb;
    wire signed [26:0] y_comb;
    wire signed [26:0] z_comb;

    lorenz_comb comb_inst (
        .xk(x_out),
        .yk(y_out),
        .zk(z_out),
        .sigma(sigma),
        .rho(rho),
        .beta(beta),
        .dt(dt),
        .x_comb(x_comb),
        .y_comb(y_comb),
        .z_comb(z_comb)
    );
    
    integrator x_integrator (
        .out(x_out),
        .funct(x_comb),
        .InitialOut(xo),
        .clk(clk),
        .reset(reset)
    );

    integrator y_integrator (
        .out(y_out),
        .funct(y_comb),
        .InitialOut(yo),
        .clk(clk),
        .reset(reset)
    );

    integrator z_integrator (
        .out(z_out),
        .funct(z_comb),
        .InitialOut(zo),
        .clk(clk),
        .reset(reset)
    );
endmodule
