
`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2022/07/06 15:08:59
// Design Name: 
// Module Name: laser_power_test
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


module laser_power_test_1(
    input       sys_clk,
    input       sys_rst,
    output      clr ,
    output      sck ,
    output reg  din ,
    output      ld  ,
    output reg  syn
    );
wire clk_out;

////仿真需要语句
//assign clk_out = sys_clk;
////////////////
reg [5:0] counter;
reg      fram_start_flag;
reg [15:0] data;
reg [23:0] data_comb;


reg [2:0] command;

reg [9:0]time_counter;

reg [3:0] state;
reg       chip_clk;
reg [9:0] divide_counter;

assign ld    = 0    ;
assign clr   = 1    ;
assign sck   = (syn == 0 && counter < 6'd26) ? clk_out : 1'd1 ;


always@(posedge clk_out or negedge sys_rst)
    if(sys_rst == 1'd0)
        state <= 4'd0;
    else case(state)
        4'b0000://idle
            if(counter == 6'd26)
                state <= 4'b0001;
        4'b0001://init
            if(counter == 6'd26)
                state <= 4'b0010;
        4'b0010://reset
            if(counter == 6'd26)
                state <= 4'b0011;
        4'b0011://power
            if(counter == 6'd26)
                state <= 4'b0100;
        4'b0100://启用内部参考
            if(counter == 6'd26)
                state <= 4'b0101;
        4'b0101://set gain
            if(counter == 6'd26)
                state <= 4'b0110;
        4'b0110://start working
            state <= 4'b0110;
        default:
            state <= 4'b0000;
    endcase

//always@(posedge clk_out or negedge sys_rst)
//    if(sys_rst == 1'd0)
//        chip_clk <= 1'd0;
//    else if(divide_counter == 10'd20)
//        chip_clk <= ~chip_clk;
//
//always@(posedge clk_out or negedge sys_rst)//一个时钟周期8微秒
//    if(sys_rst == 1'd0)
//        divide_counter <= 10'd0;
//    else if(divide_counter == 10'd40)
//        divide_counter <= 10'd0;
//    else
//        divide_counter <= divide_counter + 10'd1;

always@(posedge clk_out or negedge sys_rst)
    if(sys_rst == 1'd0)
        counter <= 6'd0;
    else if(counter == 6'd29)
        counter <= 6'd0;
    else
        counter <= counter + 6'd1;



always@(posedge clk_out or negedge sys_rst)
    if(sys_rst == 1'd0)
        syn <= 1'd1;
    else if( (state != 4'b0000 &&state != 4'b0001 ) && ( counter > 0 && counter < 6'd26) )
        syn <= 1'd0;
    else
        syn <= 1'd1;


always@(posedge clk_out or negedge sys_rst)
    if(sys_rst == 1'd0)
        data <= 16'd0;
    else case( state)
        4'b0000://idle
            data <= 16'h0000;
        4'b0001://init
            data <= 16'h0000;
        4'b0010://reset
            data <= 16'h0001;
        4'b0011://power
            data <= 16'h0003;
        4'b0100://启用内部参考
            data <= 16'h0001;
        4'b0101://set gain
            data <= 16'h0001;
        4'b0110://start working
            if(counter == 6'd1)
           data <= data + 16'd1;
           // data <= 16'd32768;
        default:data <= data;
    endcase
always@(*)
    if(sys_rst == 1'd0)
        data_comb <= 24'd0;
    else case( state )
        4'b0000://idle
            data_comb <= {8'h00,data};
        4'b0001://init
            data_comb <= {8'h00,data};
        4'b0010://reset
            data_comb <= {8'h28,data};
        4'b0011://power
            data_comb <= {8'h20,data};
        4'b0100://启用内部参考
            data_comb <= {8'h38,data};
        4'b0101://set gain
            data_comb <= {8'h02,data};
        4'b0110://start working
            data_comb <= {2'b0,3'b011,3'b0,data};
        default:data_comb <= data;
    endcase
//assign data_comb = {2'b0,3'b011,3'b0,data};
    
    
always@(posedge clk_out or negedge sys_rst)
    if(sys_rst == 1'd0)
        din <= 1'd0;
    else if(syn == 1'd1)
        din <= 0;
    else if(counter < 6'd25)
        din <= data_comb[6'd24 - counter];
    else
        din <= 0;
    
always@(posedge clk_out or negedge sys_rst)
    if(sys_rst == 1'd0)
        fram_start_flag <= 1'd0;
    else if((state == 4'b0110) && (counter == 6'd1))
        fram_start_flag <= 1;
    else 
        fram_start_flag <= 0;
   
    
////仿真需要语句
//assign clk_out = sys_clk;
////////////////    
    
    
clk_wiz_0 clk_wiz_0_inst
   (
    // Clock out ports
    .clk_out1(clk_out),     // output clk_out1
    // Status and control signals
    .resetn(sys_rst), // input resetn
    .locked(),       // output locked
   // Clock in ports
    .clk_in1(sys_clk));      // input clk_in1


    
endmodule



