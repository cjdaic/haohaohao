#
# This file was created by Xilinx Vitis tool.
#


#
# tools
#

RM = rm -f
RMDIR = rm -rf



#
# variables
#

SDCARD_GEN = sdcard_gen
GENERATE_BIF = ::scw::generate_bif

XPFM_PATH = Z:/vitis_poject/202409261/tsktcp/export/tsktcp/tsktcp.xpfm
SYS_CONFIG = tsktcp
DOMAINS = freertos10_xilinx_domain
BIF_PATH = Z:/vitis_poject/202409261/dmalwip_system/Debug/system.bif

BITSTREAM_PATH = Z:/vitis_poject/202409261/dmalwip/_ide/bitstream/design_1_wrapper_2.bit
SD_FILES = --elf Z:/vitis_poject/202409261/dmalwip/Debug/dmalwip.elf,ps7_cortexa9_0

#
# primary build targets
#

all: package

package:
	-@echo Generating bif file for the system project
	generate_system_bif.bat 59402 ${XPFM_PATH} ${DOMAINS} ${BIF_PATH}
	${SDCARD_GEN} --xpfm ${XPFM_PATH} --sys_config ${SYS_CONFIG} --bif ${BIF_PATH} --bitstream ${BITSTREAM_PATH} ${SD_FILES}

clean:
	$(RMDIR) ./_sds
	${RMDIR} package
	${RM} system.bif

.PHONY: all package clean


