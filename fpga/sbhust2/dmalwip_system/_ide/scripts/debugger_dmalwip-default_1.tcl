# Usage with Vitis IDE:
# In Vitis IDE create a Single Application Debug launch configuration,
# change the debug type to 'Attach to running target' and provide this 
# tcl script in 'Execute Script' option.
# Path of this script: C:\Users\a\Desktop\5_AxisScan\FPGA2022.7.30\dmalwip_system\_ide\scripts\debugger_dmalwip-default_1.tcl
# 
# 
# Usage with xsct:
# To debug using xsct, launch xsct and run below command
# source C:\Users\a\Desktop\5_AxisScan\FPGA2022.7.30\dmalwip_system\_ide\scripts\debugger_dmalwip-default_1.tcl
# 
connect -url tcp:127.0.0.1:3121
targets -set -nocase -filter {name =~"APU*"}
rst -system
after 3000
targets -set -filter {jtag_cable_name =~ "Digilent JTAG-SMT2 C305A703ABCD" && level==0 && jtag_device_ctx=="jsn-JTAG-SMT2-C305A703ABCD-23727093-0"}
fpga -file C:/Users/a/Desktop/5_AxisScan/FPGA2022.7.30/dmalwip/_ide/bitstream/with_LPC_and_zerolize.bit
targets -set -nocase -filter {name =~"APU*"}
loadhw -hw C:/Users/a/Desktop/5_AxisScan/FPGA2022.7.30/tsktcp/export/tsktcp/hw/with_LPC_and_zerolize.xsa -mem-ranges [list {0x40000000 0xbfffffff}] -regs
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*"}
source C:/Users/a/Desktop/5_AxisScan/FPGA2022.7.30/dmalwip/_ide/psinit/ps7_init.tcl
ps7_init
ps7_post_config
targets -set -nocase -filter {name =~ "*A9*#0"}
dow C:/Users/a/Desktop/5_AxisScan/FPGA2022.7.30/dmalwip/Debug/dmalwip.elf
configparams force-mem-access 0
targets -set -nocase -filter {name =~ "*A9*#0"}
con
