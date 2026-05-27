`timescale 1ns / 1ps

module xy5_100(
    input Clk,
    input clk_50MHz,
    input Reset_n,
    input [127:0] tdata,
//	input [7:0] laser,
	//input data_done,
//    input rd_en_sig,
    input tvalid,
    output reg tready,

    output reg SYNC,
    output reg CH_X,
    output reg CH_Y,
	output reg CH_Z,
	output reg CH_A_1,
	output reg CH_A_2,
	output reg CH_B_1,
	output reg CH_B_2,
	output     CH_Laser,
    output      ledout,
  //  output     laser_freq_set,
    output      dir,         //论文删去
    output reg trig,         //论文删去
//    output      MO,
    
  //  output gate,
  //  output      freq_change,
  //  output [15:0] freq_data,
    
   //output              Clk_to_LPC      ,
   //output              Reset_n_to_LPC,
    output reg          laser_LPC     ,
    output reg [15:0 ]  laser_LPC_data
    
    
    
    
    );
    

    
// xy5-100 tmpvar
//wire dir     ;
//reg trig     ;
//wire clk_50MHz;

reg [10:0] count = 0 ;
reg even_check_x ;
reg even_check_y ;
reg even_check_z ;
reg even_check_a_1 ;
reg even_check_a_2 ;
reg even_check_b_1 ;
reg even_check_b_2 ;
reg flag = 0;
reg trig_in;

reg         rd_en_sig    ;
reg  [79:0] data_output;
reg  [79:0] tdata_reg;
reg  [79:0] tdata_reg_reg;
reg  [79:0] tdata_reg_reg_reg;

reg  [79:0] data_output_reg;
reg  [79:0] data_output_reg_reg;

wire [79:0] data_final;




reg         laser_freq_set;
reg [31:0]  laser_freq;
reg [31:0]  laser_freq_cnt;
reg [31:0]  laser_freq_reg    ;
reg [31:0]  laser_freq_reg_reg;

reg [7:0]   tdata_82_reg;
reg [7:0]   tdata_82_reg_reg;
reg [7:0]   tdata_82_reg_reg_reg;


assign ledout = 1;

always@(posedge Clk or negedge Reset_n)
    if(!Reset_n )
        tdata_82_reg <= 8'd0;
    else
        tdata_82_reg <= tdata[87:80];
        
    assign dir = 0;        
//    assign MO = 1;        
//( rd_en_sig == 1'd1 ) ? tdata_reg[79:0] : 80'h8000_8000_8000_8000_8000;//tdata_reg[79:0]; //( rd_en_sig == 1'd1 ) ? tdata_reg[79:0] : 80'h8000_8000_8000_8000_8000;

//assign rd_en_sig = (tdata[79:0] == 80'hA000_0000_0000_0000_0000) ? 1'd1 : ((tdata[79:0] == 80'hB000_0000_0000_0000_0000) ?  1'd0 : rd_en_sig) ; 

//laser control part


always @(posedge clk_50MHz or negedge Reset_n)
   if(!Reset_n)
        trig_in <= 1'd0;
   else if( laser_freq_reg_reg >= 32'd60000 )
        trig_in <= 1'd0;
   else if( (laser_freq_reg_reg <= 32'd60000 && laser_freq_reg_reg >= 32'd200) && laser_freq_cnt <= 32'd50 )
        trig_in <= 1'd1;
   else
        trig_in <= 1'd0;
		
always @(posedge clk_50MHz or negedge Reset_n)
   if(!Reset_n)
        trig <= 1'd0;
   else if( CH_Laser==1 )
        trig <= trig_in;
   else
        trig <= 1'd0;


always @(posedge clk_50MHz or negedge Reset_n)
   if(!Reset_n)
        laser_freq_reg <= 32'd0;
   else
        laser_freq_reg <= laser_freq;
        
always @(posedge clk_50MHz or negedge Reset_n)
   if(!Reset_n)
        laser_freq_reg_reg <= 32'd0;
   else
        laser_freq_reg_reg <= laser_freq_reg;        



always @(posedge clk_50MHz or negedge Reset_n)
   if(!Reset_n)
        laser_freq_cnt <= 32'd0;
   else if( laser_freq_cnt >= laser_freq_reg_reg + 1 )
        laser_freq_cnt <= 32'd0;
   else
        laser_freq_cnt <= laser_freq_cnt + 32'd1;


always @(posedge Clk or negedge Reset_n)
   if(!Reset_n)
        laser_freq <= 32'd0;
   else if( laser_freq_set >= 1'd1 && count == 2 )
        laser_freq <= tdata[31:0];
   else
        laser_freq <= laser_freq;


always @(posedge Clk or negedge Reset_n)
   if(!Reset_n)
       laser_freq_set <= 1'd0;
   else if( tdata[95:88] == 8'haa && count == 10'd19 )
       laser_freq_set <= 1'd1;
  else if( tdata[95:88] == 8'h55)
       laser_freq_set <= 1'd0;
   else
       laser_freq_set <= laser_freq_set;


assign CH_Laser = rd_en_sig == 1 ? tdata_82_reg[4] : 0 ;  //�?光信�?
//laser control       


always @(posedge Clk or negedge Reset_n) //�?光功率控�?
   if(!Reset_n)
    begin
       laser_LPC <= 1'd0;
       laser_LPC_data <= 16'd0;
    end
   else if( tdata[95:88] == 8'hbb && count == 10'd5 )
   begin
       laser_LPC_data <= tdata[15:0];
       laser_LPC <= 1'd1;
   end
  else
  begin
       laser_LPC_data <= laser_LPC_data;
       laser_LPC <= 1'd0;
  end


always @(posedge Clk or negedge Reset_n)
   if(!Reset_n)
       rd_en_sig <= 1'd0;
   else if( tdata[95:88] == 8'hff && count == 10'd19 )
       rd_en_sig <= 1'd1;
  else if( tdata[95:88] == 8'h11 )
       rd_en_sig <= 1'd0;
   else
       rd_en_sig <= rd_en_sig;
  
  
/*

assign data_output = ( rd_en_sig == 1'd1 ) ? tdata_reg[79:0] : 80'h8000_8000_8000_8000_8000;    
                                                                                                                                                          
*/
//*debug代码



always @(posedge Clk or negedge Reset_n)
   if(!Reset_n)
        data_output <= 80'h80008000800080008000;
    else if( rd_en_sig == 1'd1 )
        data_output <= tdata;
    else
        data_output <= data_output;
 
assign data_final = (rd_en_sig == 1) ? data_output : data_output_reg ;
 
always @(posedge Clk or negedge Reset_n)
   if(!Reset_n)
        data_output_reg <= 80'h80008000800080008000;
    else if( rd_en_sig == 1'd1 )
        data_output_reg <= data_output;
    else
        data_output_reg <= data_output_reg; 
        
////assign data_final = ( rd_en_sig == 1 ) ? data_output_reg : data_output_reg_reg;         
//        
//always @(posedge Clk or negedge Reset_n)
//   if(!Reset_n)
//        data_output_reg <= 80'h80008000800080008000;
//    else if( rd_en_sig == 1'd1 )
//        data_output_reg_reg <= data_output_reg;
//    else
//        data_output_reg_reg <= data_output_reg_reg;  

always @(posedge Clk or negedge Reset_n)
    if(!Reset_n)
        count <= 0;
    else if( count == 10'd19 )
        count <= 10'd0 ;
    else
        count <= count + 10'd1;

always @(posedge Clk or negedge Reset_n)
if(!Reset_n)
begin
	SYNC <= 0;
    tready          <= 1'd0     ;
end
else if (count == 0)
  begin
    SYNC            <= 1'b1     ;                     //同步信号1
    even_check_x    <= 1'b0     ;             //定义极濧�?测信号初始忿
    even_check_y    <= 1'b0     ;
	even_check_z    <= 1'b0     ;
	even_check_a_1  <= 1'b0     ;
	even_check_a_2  <= 1'b0     ;
	even_check_b_1  <= 1'b0     ;
	even_check_b_2  <= 1'b0     ;
    CH_X            <= 16'd0    ;                    //X轴第丿位信叿
    CH_Y            <= 16'd0    ;
	CH_Z            <= 16'd0    ;
	CH_A_1          <= 16'd0    ;
	CH_A_2          <= 16'd0    ;
	CH_B_1          <= 16'd0    ;
	CH_B_2          <= 16'd0    ;
    tready          <= 1'd0     ;
  end
else if (count == 1)
  begin
    CH_X <= 16'd0;                    //X轴第二位信号 
    CH_Y <= 16'd0;
	CH_Z <= 16'd0;
	CH_A_1 <= 16'd0;
	CH_A_2 <= 16'd0;
	CH_B_1 <= 16'd0;
	CH_B_2 <= 16'd0;
    tready <= 1'd0;
  end
  else if (count == 2)
  begin
    CH_X <= 16'd1;                    //X轴第三位信号
    CH_Y <= 16'd1;
	CH_Z <= 16'd1;
	CH_A_1 <= 16'd1;
	CH_A_2 <= 16'd1;
	CH_B_1 <= 16'd1;
	CH_B_2 <= 16'd1;
  end
  else if (count > 2 && count < 19)
  begin
    CH_X <=    data_final[79-count+3];                             //   data[79-count+3]+Bias_X[15-count+3];         //X轴位置信�?
    CH_Y <=    data_final[63-count+3];                             //   data[63-count+3]+Bias_Y[15-count+3];
	CH_Z <=    data_final[47-count+3];                             //   data[47-count+3]+Bias_Z[15-count+3];
	CH_A_1 <=  data_final[31-count+3];                           //   data[31-count+3]+Bias_A[15-count+3];
	CH_A_2 <=  data_final[31-count+3];                           //   (data[31-count+3]+Bias_A[15-count+3] + Bias_A_2[15-count+3]);
	CH_B_1 <=  data_final[15-count+3];                           //   data[15-count+3]+Bias_B[15-count+3];
	CH_B_2 <=  data_final[15-count+3];                           //   (data[15-count+3] +Bias_B[15-count+3] + Bias_B_2[15-count+3]);
    even_check_x <= even_check_x ^ data_final[79-count+3]; 
    even_check_y <= even_check_y ^ data_final[63-count+3]; 
	even_check_z <= even_check_z ^ data_final[47-count+3]; 
	even_check_a_1 <= even_check_a_1 ^ data_final[31-count+3];
	even_check_a_2 <= even_check_a_2 ^ data_final[31-count+3];	
	even_check_b_1 <= even_check_b_1 ^ data_final[15-count+3];
	even_check_b_2 <= even_check_b_2 ^ data_final[15-count+3];	
  end
  else
  begin
    CH_X    <= ~even_check_x;                     //X轴极性检测信�?
    CH_Y    <= ~even_check_y;
	CH_Z    <= ~even_check_z;
	CH_A_1  <= ~even_check_a_1;
	CH_A_2  <= ~even_check_a_2;
	CH_B_1  <= ~even_check_b_1;
	CH_B_2  <= ~even_check_b_2;
    tready  <= 1'd1     ;
    SYNC    <= 1'b0;                             //同步信号0
    end	
    
//    ila_0 ila0_inst (
//	.clk(Clk), // input wire clk
//
//
//	.probe0(rd_en_sig), // input wire [0:0]  probe0  
//	.probe1(count), // input wire [9:0]  probe1 
//	.probe2(data_output), // input wire [79:0]  probe2 
//	.probe3(probe3), // input wire [127:0]  probe3
//    .probe4(SYNC),
//    .probe5(),
//    
//    
//);
    
    
    
    
endmodule