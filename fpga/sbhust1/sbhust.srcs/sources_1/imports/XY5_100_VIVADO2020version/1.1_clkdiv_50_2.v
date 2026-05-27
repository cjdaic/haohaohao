`timescale 1ns / 1ps

module clkdiv_10_1(
    input clkin,
    input rst_n,
    output reg clkout
    );
reg [3:0] count = 0;

always@(posedge clkin or negedge rst_n)
begin
    if(!rst_n)
    begin
        count <= 0;
        clkout <= 0;
    end
    else if (count == 4)
    begin
        count <= 0;
        clkout <= ~clkout;
    end
    else
    begin
        count <= count + 1;
    end
end
endmodule
