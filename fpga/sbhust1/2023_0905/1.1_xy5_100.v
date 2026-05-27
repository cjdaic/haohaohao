`timescale 1ns / 1ps

module xy5_100(
    input Clk,          //2MHz时钟
    input clk_50MHz,    //频率生成所采用的时钟
    input Reset_n,
    
//AXIS数据通道（输入该模块）//
    input [127:0] tdata,    //
                            //
    input   tvalid,         //
    output  tready,         //
//////////////////////////////

//输出通道////////////////////////////////////
                                            //
    output reg SYNC,        //同步信号      //
    output reg CH_X,        //各个通道信号  //
    output reg CH_Y,                        //
	output reg CH_Z,                        //
	output reg CH_A_1,                      //
	output reg CH_A_2,                      //
	output reg CH_B_1,                      //
	output reg CH_B_2,                      //
	output     CH_Laser,    //出光信号      //
//////////////////////////////////////////////
    
    
    output      ledout,     //不重要，我们调试时确保程序烧录完成的指示灯


//输出至电平转换芯片的信号，会将频率信号输出至电平转换芯片，由3.3V转换为5V再输出至激光器。

	output reg	gate,

    output      dir,        //输出至片外的电平转换芯片，控制电平转换方向，该信号常为高电平
    output reg  trig,       //输出至片外的电平转换芯片，该信号的频率与激光器期望频率一致
    

    output reg          laser_LPC     , //输出至功率控制模块，指示功率控制模块将进行功率调节
    output reg [15:0 ]  laser_LPC_data  //输出至功率控制模块，该值为将要调节的电压，16'h8000-16'h65535对应了0-10V的模拟电压输出
    
    
    
    
    );
    




reg [10:0] count = 0 ;      //计数器
reg even_check_x ;          //不同通道的奇偶校验位
reg even_check_y ;          //不同通道的奇偶校验位
reg even_check_z ;          //不同通道的奇偶校验位
reg even_check_a_1 ;        //不同通道的奇偶校验位
reg even_check_a_2 ;        //不同通道的奇偶校验位
reg even_check_b_1 ;        //不同通道的奇偶校验位
reg even_check_b_2 ;        //不同通道的奇偶校验位
reg flag = 0;

reg         rd_en_sig    ;
reg  [79:0] data_output;
reg  [79:0] tdata_reg;
reg  [79:0] tdata_reg_reg;
reg  [79:0] tdata_reg_reg_reg;
reg         tready;

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
    assign dir = 1;  //电平转换模块方向控制引脚，恒为3.3V      
        

//开光信号判断///////////////////////////////////////////////////////////
always@(posedge Clk or negedge Reset_n)                                //
    if(!Reset_n )                                                      //
        tdata_82_reg <= 8'd0;                                          //
    else                                                               //
        tdata_82_reg <= tdata[87:80];                                  //
                                                                       //
assign CH_Laser = rd_en_sig == 1 ? tdata_82_reg[4] : 0 ;  //开光信号   //
/////////////////////////////////////////////////////////////////////////


//激光器频率控制/////////////////////////////////////////////////////////////////////////////////////////////////
                                                                                                               //
																											   //
assign trig = gate_1stg;																					   //
assign gate = (gate_0stg == 1 || gate_1stg == 1 || gate_2stg == 1) ;                                           //
																											   //
always @(posedge clk_50MHz or negedge Reset_n)                                                                 //
   if(!Reset_n) begin                                                                                          //
		gate_1stg <= 0;                                                                                        //
		gate_2stg <= 0;                                                                                        //
	end else begin                                                                                             //
		gate_1stg <= gate_0stg;                                                                                //
	    gate_2stg <= gate_1stg;                                                                                //
	end                                                                                                        //
                                                                                                               //
always @(posedge clk_50MHz or negedge Reset_n)                                                                 //
   if(!Reset_n)                                                                                                //
        gate_0stg <= 1'd0;                                                                                     //
   else if( laser_freq_reg_reg >= 32'd60000 )                                                                  //
        gate_0stg <= 1'd0;                                                                                     //
   else if( (laser_freq_reg_reg <= 32'd60000 && laser_freq_reg_reg >= 32'd200) && laser_freq_cnt <= 32'd50 )   //
        gate_0stg <= 1'd1;                                                                                     //
   else                                                                                                        //
        gate_0stg <= 1'd0;   //采样laser_freq_reg_reg来产生激光器频率信号trig                                  //
                                                                                                               //
                                                                                                               //
                                                                                                               //
always @(posedge clk_50MHz or negedge Reset_n)                                                                 //
   if(!Reset_n)                                                                                                //
        laser_freq_cnt <= 32'd0;                                                                               //
   else if( laser_freq_cnt >= laser_freq_reg_reg + 1 )                                                         //
        laser_freq_cnt <= 32'd0;                                                                               //
   else                                                                                                        //
        laser_freq_cnt <= laser_freq_cnt + 32'd1;   //计时到laser_freq_reg_reg就清零                           //
                                                                                                               //
                                                                                                               //
                                                                                                               //
always @(posedge clk_50MHz or negedge Reset_n) // 对频率数据再打拍                                             //
   if(!Reset_n)                                                                                                //
        laser_freq_reg <= 32'd0;                                                                               //
   else                                                                                                        //
        laser_freq_reg <= laser_freq;                                                                          //
                                                                                                               //
always @(posedge clk_50MHz or negedge Reset_n) // 对频率数据打拍                                               //
   if(!Reset_n)                                                                                                //
        laser_freq_reg_reg <= 32'd0;                                                                           //
   else                                                                                                        //
        laser_freq_reg_reg <= laser_freq_reg;                                                                  //
                                                                                                               //
                                                                                                               //
                                                                                                               //
                                                                                                               //
always @(posedge Clk or negedge Reset_n)            //取出激光器频率数据放入laser_freq中                       //
   if(!Reset_n)                                                                                                //
        laser_freq <= 32'd0;                                                                                   //
   else if( laser_freq_set >= 1'd1 && count == 2 )                                                             //
        laser_freq <= tdata[31:0];                                                                             //
   else                                                                                                        //
        laser_freq <= laser_freq;                                                                              //
                                                                                                               //
                                                                                                               //
always @(posedge Clk or negedge Reset_n)                                                                       //
   if(!Reset_n)                                           //判断tdata的[95:88]的值，相当于解码，               //
       laser_freq_set <= 1'd0;                            //若为对应的值，将标志位laser_freq_set设为1          //
   else if( tdata[95:88] == 8'haa && count == 10'd19 )    //表示要设置激光器频率                               //
       laser_freq_set <= 1'd1;                                                                                 //
  else if( tdata[95:88] == 8'h55)                                                                              //
       laser_freq_set <= 1'd0;                                                                                 //
   else                                                                                                        //
       laser_freq_set <= laser_freq_set;                                                                       //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////





//激光功率控制部分代码////////////////////////////////////////////////////////////////////////////
                                                                                                //
always @(posedge Clk or negedge Reset_n)                                                        //
   if(!Reset_n)                                                                                 //
    begin                                                                                       //
       laser_LPC <= 1'd0;                                                                       //
       laser_LPC_data <= 16'd0;                                                                 //
    end                                                                                         //
   else if( tdata[95:88] == 8'hbb && count == 10'd5 )   //判断tdata的[95:88]的值，相当于解码，  //
   begin                                                //若为对应的值，将数据传输至激光器功率  //
       laser_LPC_data <= tdata[15:0];                   //调节模块的接口                        //
       laser_LPC <= 1'd1;                                                                       //
   end                                                                                          //
  else                                                                                          //
  begin                                                                                         //
       laser_LPC_data <= laser_LPC_data;                                                        //
       laser_LPC <= 1'd0;                                                                       //
  end                                                                                           //
//////////////////////////////////////////////////////////////////////////////////////////////////


always @(posedge Clk or negedge Reset_n)
   if(!Reset_n)
       rd_en_sig <= 1'd0;
   else if( tdata[95:88] == 8'hff && count == 10'd19 )//判断tdata的[95:88]的值，相当于解码，若为
       rd_en_sig <= 1'd1;                             //对应值，调整为可读，允许改变振镜位置。
  else if( tdata[95:88] == 8'h11 )
       rd_en_sig <= 1'd0;
   else
       rd_en_sig <= rd_en_sig;
  

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
        


//多通道振镜协议////////////////////////////////////////////////////////
                                                                      //
always @(posedge Clk or negedge Reset_n) //计数器，每帧协议20个cycle  //
    if(!Reset_n)                                                      //
        count <= 0;                                                   //
    else if( count == 10'd19 )                                        //
        count <= 10'd0 ;                                              //
    else                                                              //
        count <= count + 10'd1;                                       //
                                                                      //
always @(posedge Clk or negedge Reset_n)                              //
if(!Reset_n)                                                          //
begin                                                                 //
	SYNC <= 0;                                                        //
    tready          <= 1'd0     ;                                     //
end                                                                   //
else if (count == 0)                                                  //
  begin                                                               //
    SYNC            <= 1'b1     ;                     //同步信号1     //
    even_check_x    <= 1'b0     ;                                     //
    even_check_y    <= 1'b0     ;                                     //
	even_check_z    <= 1'b0     ;                                     //
	even_check_a_1  <= 1'b0     ;                                     //
	even_check_a_2  <= 1'b0     ;                                     //
	even_check_b_1  <= 1'b0     ;                                     //
	even_check_b_2  <= 1'b0     ;                                     //
    CH_X            <= 16'd0    ;                                     //
    CH_Y            <= 16'd0    ;                                     //
	CH_Z            <= 16'd0    ;                                     //
	CH_A_1          <= 16'd0    ;                                     //
	CH_A_2          <= 16'd0    ;                                     //
	CH_B_1          <= 16'd0    ;                                     //
	CH_B_2          <= 16'd0    ;                                     //
    tready          <= 1'd0     ;                                     //
  end                                                                 //
else if (count == 1)                                                  //
  begin                                                               //
    CH_X <= 16'd0;                                                    //
    CH_Y <= 16'd0;                                                    //
	CH_Z <= 16'd0;                                                    //
	CH_A_1 <= 16'd0;                                                  //
	CH_A_2 <= 16'd0;                                                  //
	CH_B_1 <= 16'd0;                                                  //
	CH_B_2 <= 16'd0;                                                  //
    tready <= 1'd0;                                                   //
  end                                                                 //
  else if (count == 2)                                                //
  begin                                                               //
    CH_X <= 16'd1;                                                    //
    CH_Y <= 16'd1;                                                    //
	CH_Z <= 16'd1;                                                    //
	CH_A_1 <= 16'd1;                                                  //
	CH_A_2 <= 16'd1;                                                  //
	CH_B_1 <= 16'd1;                                                  //
	CH_B_2 <= 16'd1;                                                  //
  end                                                                 //
  else if (count > 2 && count < 19)                                   //
  begin                                                               //
    CH_X <=    data_final[79-count+3];                                //
    CH_Y <=    data_final[63-count+3];                                //
	CH_Z <=    data_final[47-count+3];                                //
	CH_A_1 <=  data_final[31-count+3];                                //
	CH_A_2 <=  data_final[31-count+3];                                //
	CH_B_1 <=  data_final[15-count+3];                                //
	CH_B_2 <=  data_final[15-count+3];               	              //
    even_check_x <= even_check_x ^ data_final[79-count+3];            //
    even_check_y <= even_check_y ^ data_final[63-count+3];            //
	even_check_z <= even_check_z ^ data_final[47-count+3];            //
	even_check_a_1 <= even_check_a_1 ^ data_final[31-count+3];        //
	even_check_a_2 <= even_check_a_2 ^ data_final[31-count+3];	      //
	even_check_b_1 <= even_check_b_1 ^ data_final[15-count+3];        //
	even_check_b_2 <= even_check_b_2 ^ data_final[15-count+3];	      //
  end                                                                 //
  else                                                                //
  begin                                                               //
    CH_X    <= ~even_check_x;                                         //
    CH_Y    <= ~even_check_y;                                         //
	CH_Z    <= ~even_check_z;                                         //
	CH_A_1  <= ~even_check_a_1;                                       //
	CH_A_2  <= ~even_check_a_2;                                       //
	CH_B_1  <= ~even_check_b_1;                                       //
	CH_B_2  <= ~even_check_b_2;                                       //
    tready  <= 1'd1     ;                                             //
    SYNC    <= 1'b0;                                                  //
    end	                                                              //
////////////////////////////////////////////////////////////////////////    

    
    
    
endmodule