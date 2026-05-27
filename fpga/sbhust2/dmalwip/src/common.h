#ifndef __COMMON_H__
#define __COMMON_H__

#include "xparameters.h"
#include "XilPeriph/XilAxiDma.h"
#include "XilPeriph/Eth.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>


#define DMA_TRANS_SIZE 1600000        ///< dma鍗曟浼犺緭鏁版嵁鏁伴噺
#define DA_SAMPLE_RATE 2e6          ///< da杈撳嚭閲囨牱鐜�

#define IP_ADDR "192.168.1.10"      ///< FPGA寮�鍙戞澘 鏈嶅姟鍣╥p鍦板潃
#define NETMASK "255.255.255.0"     ///< FPGA寮�鍙戞澘 鏈嶅姟鍣ㄦ帺鐮�
#define GW      "192.168.1.1"       ///< FPGA寮�鍙戞澘 鏈嶅姟鍣ㄧ綉鍏�
#define SERVER_PORT 7               ///< FPGA寮�鍙戞澘 鏈嶅姟鍣ㄧ鍙�

#define DATA_BUF_NUM 2                ///< netbuf澶氱骇缂撳啿鏁伴噺
#define DATA_BUF_SIZE 1600000         ///< netbuf澶у皬

#define DEBUG 0

void initPeriph();

#endif
