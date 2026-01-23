/* AI Code starts here */
`timescale 1ns/1ps

module testbench;

    reg clk;
    reg reset;

    // 7.20 Fixed point parameters
    // dt = 1/256 = 0.00390625
    // 0.00390625 * 2^20 = 4096
    reg signed [26:0] dt = 27'sd4096;

    // Initial Conditions
    // x0 = -1.0 * 2^20 = -1048576
    reg signed [26:0] xo = -27'sd1048576;
    
    // y0 = 0.1 * 2^20 = 104857.6 -> 104858
    reg signed [26:0] yo = 27'sd104858;
    
    // z0 = 25.0 * 2^20 = 26214400
    reg signed [26:0] zo = 27'sd26214400;

    // Parameters
    // sigma = 10.0 * 2^20 = 10485760
    reg signed [26:0] sigma = 27'sd10485760;

    // rho = 28.0 * 2^20 = 29360128
    reg signed [26:0] rho = 27'sd29360128;

    // beta = (8/3) * 2^20 = 2796202.66... -> 2796203
    reg signed [26:0] beta = 27'sd2796203;

    // Outputs
    wire signed [26:0] x_out;
    wire signed [26:0] y_out;
    wire signed [26:0] z_out;

    // Instantiate the Unit Under Test (UUT)
    lorenz uut (
        .clk(clk),
        .reset(reset),
        .xo(xo),
        .yo(yo),
        .zo(zo),
        .sigma(sigma),
        .rho(rho),
        .beta(beta),
        .dt(dt),
        .x_out(x_out),
        .y_out(y_out),
        .z_out(z_out)
    );

    // Clock generation
    initial begin
        clk = 0;
        forever #5 clk = ~clk; // 100MHz clock (10ns period)
    end

    // Test sequence
    initial begin
        // Initialize Inputs
        reset = 1;

        // Wait 100 ns for global reset to finish
        #100;
        
        // Release reset
        reset = 0;
        
        // Simulation duration
        // Just run for enough cycles to see movement
        #1000000;
        
        $stop;
    end
      
endmodule
/* AI Code ends here */