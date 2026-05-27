`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2022/03/14 08:35:46
// Design Name: 
// Module Name: laser_control
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module laser_control(
    input clk,
    input rst,
    output gate,
    output reg trig
    );
    reg [31:0] cnt;
    
    assign gate = 1'd1;
    
always@(posedge clk or negedge rst)
    if( rst == 1'd0 )
        cnt <= 32'd0;
    else if( cnt == 32'd50000 )
        cnt <= 32'd0;
    else
        cnt <= cnt + 32'd1;
        
always@(posedge clk or negedge rst)
    if( rst == 1'd0 )
        trig <= 1'd0;
    else if( cnt == 32'd49999 )
        trig <= ~trig;
    else
        trig <= trig;

endmodule
