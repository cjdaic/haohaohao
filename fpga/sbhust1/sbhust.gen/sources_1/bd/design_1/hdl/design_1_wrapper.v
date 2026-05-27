//Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2020.2 (win64) Build 3064766 Wed Nov 18 09:12:45 MST 2020
//Date        : Wed Jan 17 16:42:58 2024
//Host        : LAPTOP-6OB214A0 running 64-bit major release  (build 9200)
//Command     : generate_target design_1_wrapper.bd
//Design      : design_1_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module design_1_wrapper
   (CH_A_1_0,
    CH_A_1_N_0,
    CH_A_2_0,
    CH_A_2_N_0,
    CH_B_1_0,
    CH_B_1_N_0,
    CH_B_2_0,
    CH_B_2_N_0,
    CH_Laser_1,
    CH_X_0,
    CH_X_N_0,
    CH_Y_0,
    CH_Y_N_0,
    CH_Z_0,
    CH_Z_N_0,
    CLK_OUT_AB_1_0,
    CLK_OUT_AB_1_N_0,
    CLK_OUT_AB_2_0,
    CLK_OUT_AB_2_N_0,
    CLK_OUT_XY_0,
    CLK_OUT_XY_N_0,
    CLK_OUT_Z_0,
    CLK_OUT_Z_N_0,
    DDR_addr,
    DDR_ba,
    DDR_cas_n,
    DDR_ck_n,
    DDR_ck_p,
    DDR_cke,
    DDR_cs_n,
    DDR_dm,
    DDR_dq,
    DDR_dqs_n,
    DDR_dqs_p,
    DDR_odt,
    DDR_ras_n,
    DDR_reset_n,
    DDR_we_n,
    FIXED_IO_ddr_vrn,
    FIXED_IO_ddr_vrp,
    FIXED_IO_mio,
    FIXED_IO_ps_clk,
    FIXED_IO_ps_porb,
    FIXED_IO_ps_srstb,
    SYNC_AB_1_0,
    SYNC_AB_1_N_0,
    SYNC_AB_2_0,
    SYNC_AB_2_N_0,
    SYNC_XY_0,
    SYNC_XY_N_0,
    SYNC_Z_0,
    SYNC_Z_N_0,
    clk_in1_0,
    din_0,
    dir_0,
    ledout_0,
    reset_n,
    sck_0,
    syn_0,
    trig_0);
  output CH_A_1_0;
  output CH_A_1_N_0;
  output CH_A_2_0;
  output CH_A_2_N_0;
  output CH_B_1_0;
  output CH_B_1_N_0;
  output CH_B_2_0;
  output CH_B_2_N_0;
  output CH_Laser_1;
  output CH_X_0;
  output CH_X_N_0;
  output CH_Y_0;
  output CH_Y_N_0;
  output CH_Z_0;
  output CH_Z_N_0;
  output CLK_OUT_AB_1_0;
  output CLK_OUT_AB_1_N_0;
  output CLK_OUT_AB_2_0;
  output CLK_OUT_AB_2_N_0;
  output CLK_OUT_XY_0;
  output CLK_OUT_XY_N_0;
  output CLK_OUT_Z_0;
  output CLK_OUT_Z_N_0;
  inout [14:0]DDR_addr;
  inout [2:0]DDR_ba;
  inout DDR_cas_n;
  inout DDR_ck_n;
  inout DDR_ck_p;
  inout DDR_cke;
  inout DDR_cs_n;
  inout [3:0]DDR_dm;
  inout [31:0]DDR_dq;
  inout [3:0]DDR_dqs_n;
  inout [3:0]DDR_dqs_p;
  inout DDR_odt;
  inout DDR_ras_n;
  inout DDR_reset_n;
  inout DDR_we_n;
  inout FIXED_IO_ddr_vrn;
  inout FIXED_IO_ddr_vrp;
  inout [53:0]FIXED_IO_mio;
  inout FIXED_IO_ps_clk;
  inout FIXED_IO_ps_porb;
  inout FIXED_IO_ps_srstb;
  output SYNC_AB_1_0;
  output SYNC_AB_1_N_0;
  output SYNC_AB_2_0;
  output SYNC_AB_2_N_0;
  output SYNC_XY_0;
  output SYNC_XY_N_0;
  output SYNC_Z_0;
  output SYNC_Z_N_0;
  input clk_in1_0;
  output din_0;
  output dir_0;
  output ledout_0;
  input reset_n;
  output sck_0;
  output syn_0;
  output trig_0;

  wire CH_A_1_0;
  wire CH_A_1_N_0;
  wire CH_A_2_0;
  wire CH_A_2_N_0;
  wire CH_B_1_0;
  wire CH_B_1_N_0;
  wire CH_B_2_0;
  wire CH_B_2_N_0;
  wire CH_Laser_1;
  wire CH_X_0;
  wire CH_X_N_0;
  wire CH_Y_0;
  wire CH_Y_N_0;
  wire CH_Z_0;
  wire CH_Z_N_0;
  wire CLK_OUT_AB_1_0;
  wire CLK_OUT_AB_1_N_0;
  wire CLK_OUT_AB_2_0;
  wire CLK_OUT_AB_2_N_0;
  wire CLK_OUT_XY_0;
  wire CLK_OUT_XY_N_0;
  wire CLK_OUT_Z_0;
  wire CLK_OUT_Z_N_0;
  wire [14:0]DDR_addr;
  wire [2:0]DDR_ba;
  wire DDR_cas_n;
  wire DDR_ck_n;
  wire DDR_ck_p;
  wire DDR_cke;
  wire DDR_cs_n;
  wire [3:0]DDR_dm;
  wire [31:0]DDR_dq;
  wire [3:0]DDR_dqs_n;
  wire [3:0]DDR_dqs_p;
  wire DDR_odt;
  wire DDR_ras_n;
  wire DDR_reset_n;
  wire DDR_we_n;
  wire FIXED_IO_ddr_vrn;
  wire FIXED_IO_ddr_vrp;
  wire [53:0]FIXED_IO_mio;
  wire FIXED_IO_ps_clk;
  wire FIXED_IO_ps_porb;
  wire FIXED_IO_ps_srstb;
  wire SYNC_AB_1_0;
  wire SYNC_AB_1_N_0;
  wire SYNC_AB_2_0;
  wire SYNC_AB_2_N_0;
  wire SYNC_XY_0;
  wire SYNC_XY_N_0;
  wire SYNC_Z_0;
  wire SYNC_Z_N_0;
  wire clk_in1_0;
  wire din_0;
  wire dir_0;
  wire ledout_0;
  wire reset_n;
  wire sck_0;
  wire syn_0;
  wire trig_0;

  design_1 design_1_i
       (.CH_A_1_0(CH_A_1_0),
        .CH_A_1_N_0(CH_A_1_N_0),
        .CH_A_2_0(CH_A_2_0),
        .CH_A_2_N_0(CH_A_2_N_0),
        .CH_B_1_0(CH_B_1_0),
        .CH_B_1_N_0(CH_B_1_N_0),
        .CH_B_2_0(CH_B_2_0),
        .CH_B_2_N_0(CH_B_2_N_0),
        .CH_Laser_1(CH_Laser_1),
        .CH_X_0(CH_X_0),
        .CH_X_N_0(CH_X_N_0),
        .CH_Y_0(CH_Y_0),
        .CH_Y_N_0(CH_Y_N_0),
        .CH_Z_0(CH_Z_0),
        .CH_Z_N_0(CH_Z_N_0),
        .CLK_OUT_AB_1_0(CLK_OUT_AB_1_0),
        .CLK_OUT_AB_1_N_0(CLK_OUT_AB_1_N_0),
        .CLK_OUT_AB_2_0(CLK_OUT_AB_2_0),
        .CLK_OUT_AB_2_N_0(CLK_OUT_AB_2_N_0),
        .CLK_OUT_XY_0(CLK_OUT_XY_0),
        .CLK_OUT_XY_N_0(CLK_OUT_XY_N_0),
        .CLK_OUT_Z_0(CLK_OUT_Z_0),
        .CLK_OUT_Z_N_0(CLK_OUT_Z_N_0),
        .DDR_addr(DDR_addr),
        .DDR_ba(DDR_ba),
        .DDR_cas_n(DDR_cas_n),
        .DDR_ck_n(DDR_ck_n),
        .DDR_ck_p(DDR_ck_p),
        .DDR_cke(DDR_cke),
        .DDR_cs_n(DDR_cs_n),
        .DDR_dm(DDR_dm),
        .DDR_dq(DDR_dq),
        .DDR_dqs_n(DDR_dqs_n),
        .DDR_dqs_p(DDR_dqs_p),
        .DDR_odt(DDR_odt),
        .DDR_ras_n(DDR_ras_n),
        .DDR_reset_n(DDR_reset_n),
        .DDR_we_n(DDR_we_n),
        .FIXED_IO_ddr_vrn(FIXED_IO_ddr_vrn),
        .FIXED_IO_ddr_vrp(FIXED_IO_ddr_vrp),
        .FIXED_IO_mio(FIXED_IO_mio),
        .FIXED_IO_ps_clk(FIXED_IO_ps_clk),
        .FIXED_IO_ps_porb(FIXED_IO_ps_porb),
        .FIXED_IO_ps_srstb(FIXED_IO_ps_srstb),
        .SYNC_AB_1_0(SYNC_AB_1_0),
        .SYNC_AB_1_N_0(SYNC_AB_1_N_0),
        .SYNC_AB_2_0(SYNC_AB_2_0),
        .SYNC_AB_2_N_0(SYNC_AB_2_N_0),
        .SYNC_XY_0(SYNC_XY_0),
        .SYNC_XY_N_0(SYNC_XY_N_0),
        .SYNC_Z_0(SYNC_Z_0),
        .SYNC_Z_N_0(SYNC_Z_N_0),
        .clk_in1_0(clk_in1_0),
        .din_0(din_0),
        .dir_0(dir_0),
        .ledout_0(ledout_0),
        .reset_n(reset_n),
        .sck_0(sck_0),
        .syn_0(syn_0),
        .trig_0(trig_0));
endmodule
