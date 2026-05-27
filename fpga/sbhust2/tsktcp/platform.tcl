# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct D:\biye2\scanner\scanner.vitis\tsktcp\platform.tcl
# 
# OR launch xsct and run below command.
# source D:\biye2\scanner\scanner.vitis\tsktcp\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {tsktcp}\
-hw {D:\biye2\scanner\design_1_wrapper.xsa}\
-proc {ps7_cortexa9_0} -os {freertos10_xilinx} -fsbl-target {psu_cortexa53_0} -out {D:/biye2/scanner/scanner.vitis}

platform write
platform generate -domains 
platform active {tsktcp}
bsp reload
bsp setlib -name lwip211 -ver 1.3
bsp config tick_rate "1000"
bsp config total_heap_size "65536"
bsp config total_heap_size "13421778"
bsp config total_heap_size "134217728"
bsp write
bsp reload
catch {bsp regenerate}
bsp config api_mode "SOCKET_API"
bsp write
bsp reload
catch {bsp regenerate}
platform generate
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
domain active {zynq_fsbl}
bsp reload
bsp reload
domain active {freertos10_xilinx_domain}
bsp reload
bsp write
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform active {tsktcp}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform active {tsktcp}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate
platform active {tsktcp}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform active {tsktcp}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform active {tsktcp}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform active {tsktcp}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform active {tsktcp}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform active {tsktcp}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform active {tsktcp}
bsp reload
bsp reload
platform config -updatehw {D:/biye2/scanner_with_LPC/hardware_platfore_yang.xsa}
platform generate
platform active {tsktcp}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper_with_LPC.xsa}
platform generate -domains 
platform active {tsktcp}
bsp reload
platform generate -domains 
platform active {tsktcp}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper_with_LPC.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper_with_LPC.xsa}
platform clean
platform generate
platform generate
platform active {tsktcp}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
domain active {zynq_fsbl}
bsp reload
bsp reload
bsp reload
domain active {freertos10_xilinx_domain}
bsp reload
bsp reload
domain active {zynq_fsbl}
bsp reload
domain active {freertos10_xilinx_domain}
bsp reload
platform active {tsktcp}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper_with_LPC.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform active {tsktcp}
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper_without_lpc.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/design_1_wrapper_with_lpc_01.xsa}
platform generate -domains 
platform config -updatehw {D:/biye2/scanner/with_LPC_and_zerolize.xsa}
platform generate -domains 
platform generate
platform active {tsktcp}
platform config -updatehw {D:/Desktop/FPGA/project_to_luwangrong/design_1_wrapper.xsa}
bsp reload
domain active {zynq_fsbl}
bsp reload
catch {bsp regenerate}
platform generate
platform active {tsktcp}
platform config -updatehw {D:/Desktop/FPGA/project_to_luwangrong/design_1_wrapper.xsa}
bsp reload
catch {bsp regenerate}
platform generate
platform active {tsktcp}
platform config -updatehw {D:/Desktop/FPGA/project_to_luwangrong/design_1_wrapper.xsa}
platform clean
bsp reload
catch {bsp regenerate}
platform generate
domain active {freertos10_xilinx_domain}
bsp reload
platform config -updatehw {D:/Desktop/FPGA/project_to_luwangrong/design_1_wrapper.xsa}
domain active {zynq_fsbl}
bsp reload
catch {bsp regenerate}
domain active {freertos10_xilinx_domain}
bsp reload
catch {bsp regenerate}
platform generate
platform active {tsktcp}
platform generate
platform active {tsktcp}
platform active {tsktcp}
platform config -updatehw {D:/Desktop/FPGA/project_to_liuyi/project_to_luwangrong/design_1_wrapper.xsa}
platform clean
platform generate
platform active {tsktcp}
platform config -updatehw {Z:/vitis_poject/202409261/design_1_wrapper_1.xsa}
platform generate
platform config -updatehw {Z:/vitis_poject/202409261/design_1_wrapper_2.xsa}
platform generate -domains 
