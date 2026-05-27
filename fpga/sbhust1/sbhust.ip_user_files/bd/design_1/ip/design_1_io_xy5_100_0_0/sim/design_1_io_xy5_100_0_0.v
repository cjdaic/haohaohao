// (c) Copyright 1995-2022 Xilinx, Inc. All rights reserved.
// 
// This file contains confidential and proprietary information
// of Xilinx, Inc. and is protected under U.S. and
// international copyright and other intellectual property
// laws.
// 
// DISCLAIMER
// This disclaimer is not a license and does not grant any
// rights to the materials distributed herewith. Except as
// otherwise provided in a valid license issued to you by
// Xilinx, and to the maximum extent permitted by applicable
// law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// (2) Xilinx shall not be liable (whether in contract or tort,
// including negligence, or under any other theory of
// liability) for any loss or damage of any kind or nature
// related to, arising under or in connection with these
// materials, including for any direct, or any indirect,
// special, incidental, or consequential loss or damage
// (including loss of data, profits, goodwill, or any type of
// loss or damage suffered as a result of any action brought
// by a third party) even if such damage or loss was
// reasonably foreseeable or Xilinx had been advised of the
// possibility of the same.
// 
// CRITICAL APPLICATIONS
// Xilinx products are not designed or intended to be fail-
// safe, or for use in any application requiring fail-safe
// performance, such as life-support or safety devices or
// systems, Class III medical devices, nuclear facilities,
// applications related to the deployment of airbags, or any
// other applications that could lead to death, personal
// injury, or severe property or environmental damage
// (individually and collectively, "Critical
// Applications"). Customer assumes the sole risk and
// liability of any use of Xilinx products in Critical
// Applications, subject only to applicable laws and
// regulations governing limitations on product liability.
// 
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// PART OF THIS FILE AT ALL TIMES.
// 
// DO NOT MODIFY THIS FILE.


// IP VLNV: xilinx.com:module_ref:io_xy5_100:1.0
// IP Revision: 1

`timescale 1ns/1ps

(* IP_DEFINITION_SOURCE = "module_ref" *)
(* DowngradeIPIdentifiedWarnings = "yes" *)
module design_1_io_xy5_100_0_0 (
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

output wire CLK_OUT_XY;
output wire CLK_OUT_XY_N;
output wire SYNC_XY;
output wire SYNC_XY_N;
output wire CH_X;
output wire CH_X_N;
output wire CH_Y;
output wire CH_Y_N;
output wire CLK_OUT_Z;
output wire CLK_OUT_Z_N;
output wire SYNC_Z;
output wire SYNC_Z_N;
output wire CH_Z;
output wire CH_Z_N;
output wire CLK_OUT_AB_1;
output wire CLK_OUT_AB_1_N;
output wire SYNC_AB_1;
output wire SYNC_AB_1_N;
output wire CH_A_1;
output wire CH_A_1_N;
output wire CH_B_1;
output wire CH_B_1_N;
output wire CLK_OUT_AB_2;
output wire CLK_OUT_AB_2_N;
output wire SYNC_AB_2;
output wire SYNC_AB_2_N;
output wire CH_A_2;
output wire CH_A_2_N;
output wire CH_B_2;
output wire CH_B_2_N;
input wire CLK_OUT_OBUFDS_I;
input wire SYNC_OBUFDS_I;
input wire CH_X_OBUFDS_I;
input wire CH_Y_OBUFDS_I;
input wire CH_Z_OBUFDS_I;
input wire CH_A_1_OBUFDS_I;
input wire CH_A_2_OBUFDS_I;
input wire CH_B_1_OBUFDS_I;
input wire CH_B_2_OBUFDS_I;

  io_xy5_100 inst (
    .CLK_OUT_XY(CLK_OUT_XY),
    .CLK_OUT_XY_N(CLK_OUT_XY_N),
    .SYNC_XY(SYNC_XY),
    .SYNC_XY_N(SYNC_XY_N),
    .CH_X(CH_X),
    .CH_X_N(CH_X_N),
    .CH_Y(CH_Y),
    .CH_Y_N(CH_Y_N),
    .CLK_OUT_Z(CLK_OUT_Z),
    .CLK_OUT_Z_N(CLK_OUT_Z_N),
    .SYNC_Z(SYNC_Z),
    .SYNC_Z_N(SYNC_Z_N),
    .CH_Z(CH_Z),
    .CH_Z_N(CH_Z_N),
    .CLK_OUT_AB_1(CLK_OUT_AB_1),
    .CLK_OUT_AB_1_N(CLK_OUT_AB_1_N),
    .SYNC_AB_1(SYNC_AB_1),
    .SYNC_AB_1_N(SYNC_AB_1_N),
    .CH_A_1(CH_A_1),
    .CH_A_1_N(CH_A_1_N),
    .CH_B_1(CH_B_1),
    .CH_B_1_N(CH_B_1_N),
    .CLK_OUT_AB_2(CLK_OUT_AB_2),
    .CLK_OUT_AB_2_N(CLK_OUT_AB_2_N),
    .SYNC_AB_2(SYNC_AB_2),
    .SYNC_AB_2_N(SYNC_AB_2_N),
    .CH_A_2(CH_A_2),
    .CH_A_2_N(CH_A_2_N),
    .CH_B_2(CH_B_2),
    .CH_B_2_N(CH_B_2_N),
    .CLK_OUT_OBUFDS_I(CLK_OUT_OBUFDS_I),
    .SYNC_OBUFDS_I(SYNC_OBUFDS_I),
    .CH_X_OBUFDS_I(CH_X_OBUFDS_I),
    .CH_Y_OBUFDS_I(CH_Y_OBUFDS_I),
    .CH_Z_OBUFDS_I(CH_Z_OBUFDS_I),
    .CH_A_1_OBUFDS_I(CH_A_1_OBUFDS_I),
    .CH_A_2_OBUFDS_I(CH_A_2_OBUFDS_I),
    .CH_B_1_OBUFDS_I(CH_B_1_OBUFDS_I),
    .CH_B_2_OBUFDS_I(CH_B_2_OBUFDS_I)
  );
endmodule
