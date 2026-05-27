
module io_xy5_100(
    CLK_OUT_XY,
    CLK_OUT_XY_N,
    SYNC_XY, 
    SYNC_XY_N, 
    CH_X,
    CH_X_N,
    CH_Y, 
    CH_Y_N,
    CLK_OUT_Z, 
    CLK_OUT_Z_N, 
    SYNC_Z, 
    SYNC_Z_N,
    CH_Z,
    CH_Z_N,
    CLK_OUT_AB_1, 
    CLK_OUT_AB_1_N, 
    SYNC_AB_1, 
    SYNC_AB_1_N,
    CH_A_1,
    CH_A_1_N,
    CH_B_1,
    CH_B_1_N,
    CLK_OUT_AB_2, 
    CLK_OUT_AB_2_N, 
    SYNC_AB_2, 
    SYNC_AB_2_N,
    CH_A_2,
    CH_A_2_N,
    CH_B_2,
    CH_B_2_N,
    CLK_OUT_OBUFDS_I,
    SYNC_OBUFDS_I,
    CH_X_OBUFDS_I, 
    CH_Y_OBUFDS_I, 
    CH_Z_OBUFDS_I, 
    CH_A_1_OBUFDS_I,
	CH_A_2_OBUFDS_I,
    CH_B_1_OBUFDS_I,
    CH_B_2_OBUFDS_I	

    );
  output CLK_OUT_XY;
  output CLK_OUT_XY_N;
  output SYNC_XY;
  output SYNC_XY_N;
  output CH_X;
  output CH_X_N;
  output CH_Y;
  output CH_Y_N;
  output CLK_OUT_Z;
  output CLK_OUT_Z_N;
  output SYNC_Z;
  output SYNC_Z_N;
  output CH_Z;
  output CH_Z_N;
  output CLK_OUT_AB_1;
  output CLK_OUT_AB_1_N;
  output SYNC_AB_1;
  output SYNC_AB_1_N;
  output CH_A_1;
  output CH_A_1_N; 
  output CH_B_1;
  output CH_B_1_N;
  output CLK_OUT_AB_2;
  output CLK_OUT_AB_2_N;
  output SYNC_AB_2;
  output SYNC_AB_2_N;
  output CH_A_2;
  output CH_A_2_N; 
  output CH_B_2;
  output CH_B_2_N;    


  // internal wires associated with differential buffers

  input wire CLK_OUT_OBUFDS_I;
  input wire SYNC_OBUFDS_I;
  input wire CH_X_OBUFDS_I;
  input wire CH_Y_OBUFDS_I;
  input wire CH_Z_OBUFDS_I;
  input wire CH_A_1_OBUFDS_I;
  input wire CH_A_2_OBUFDS_I;  
  input wire CH_B_1_OBUFDS_I;
  input wire CH_B_2_OBUFDS_I;

  
assign  CLK_OUT_XY = CLK_OUT_OBUFDS_I;
assign  CLK_OUT_XY_N = ~CLK_OUT_OBUFDS_I;
assign  SYNC_XY = SYNC_OBUFDS_I;
assign  SYNC_XY_N = ~SYNC_OBUFDS_I;
assign  CH_X = CH_X_OBUFDS_I;
assign  CH_X_N = ~CH_X_OBUFDS_I;
assign  CH_Y = CH_Y_OBUFDS_I;
assign  CH_Y_N = ~CH_Y_OBUFDS_I;
assign  CLK_OUT_Z = CLK_OUT_OBUFDS_I;
assign  CLK_OUT_Z_N = ~CLK_OUT_OBUFDS_I;
assign  SYNC_Z = SYNC_OBUFDS_I;
assign  SYNC_Z_N = ~SYNC_OBUFDS_I;
assign  CH_Z = CH_Z_OBUFDS_I;
assign  CH_Z_N = ~CH_Z_OBUFDS_I;
assign  CLK_OUT_AB_1 = CLK_OUT_OBUFDS_I;
assign  CLK_OUT_AB_1_N = ~CLK_OUT_OBUFDS_I;
assign  SYNC_AB_1 = SYNC_OBUFDS_I;
assign  SYNC_AB_1_N = ~SYNC_OBUFDS_I;
assign  CH_A_1 = CH_A_1_OBUFDS_I;
assign  CH_A_1_N = ~CH_A_1_OBUFDS_I;
assign  CH_B_1 = CH_B_1_OBUFDS_I;
assign  CH_B_1_N = ~CH_B_1_OBUFDS_I;
assign  CLK_OUT_AB_2 = CLK_OUT_OBUFDS_I;
assign  CLK_OUT_AB_2_N = ~CLK_OUT_OBUFDS_I;
assign  SYNC_AB_2 = SYNC_OBUFDS_I;
assign  SYNC_AB_2_N = ~SYNC_OBUFDS_I;
assign  CH_A_2 = CH_A_2_OBUFDS_I;
assign  CH_A_2_N = ~CH_A_2_OBUFDS_I;
assign  CH_B_2 = CH_B_2_OBUFDS_I;
assign  CH_B_2_N = ~CH_B_2_OBUFDS_I;


endmodule