#include "TskData.h"
#include <xparameters.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include "xil_io.h"
#include "../common.h"

using namespace std;

extern XilIntc* intc;
extern XilAxiDma* axi_dma;

const int tskDataPrio = 2;
const int tskDataStkSize = 16384;
static TaskHandle_t tskDataHandle;

static SemaphoreHandle_t semFifoEmpty;
static SemaphoreHandle_t semDma;
extern SemaphoreHandle_t semDataBufWr;
extern SemaphoreHandle_t semDataBufRd;
unsigned int databuf_rd_p=0;

extern XScuGic xInterruptController;
volatile int tx_done;
extern u8 databuf[DATA_BUF_NUM][DATA_BUF_SIZE];

/**
 * @file TskData. cc
 * 
 * fifo 濞戞搩鍘介弻鍥ㄥ緞閸曨厽鍊為柛鎴ｅГ閺嗭拷
 * 婵縿鍊栧鍌涙償閺冨浂鍤夐柟绋挎川閵囨瓰ma闁告瑯鍨禍鎺戭嚕閿熻姤鎱ㄧ�ｎ偅鍎旈弶鈺傚姈閺嗙喖骞戦敓锟�
 * 
 * @param CallbackRef not used
 * @return
 */
//void fifo_ISR(void* CallbackRef)
//{
//    static BaseType_t wake=pdTRUE;
//    XScuGic_Disable(&xInterruptController,62U);
//    xSemaphoreGiveFromISR(semFifoEmpty,&wake);
//    portYIELD_FROM_ISR(wake);
//}

/**
 * @file TskData. cc
 * 
 * dma閻庣懓鏈崹姘▔椤撶喐鐒介柛鎴ｅГ閺嗭拷
 * 鐟滅増鎹侀姘▔椤撶喐鐒介柛鎴ｅГ閺嗙喓鎲撮敃锟借ぐ鍌炲籍鐠佸湱绀夊ù鐙呯秬閵嗗啯绋夐敓钘夆枎椤＄尠a濞磋偐濞�閿熸垝绀侀悾顒勫箣閿燂拷
 * 
 * @param dmaps_channel
 * @param DmaCmd
 * @param CallbackRef
 * @return
 */
static void axi_dma_mm2s_ISR(void *CallbackRef){
    static BaseType_t wake=pdFALSE;
	u32 IrqStatus;
	IrqStatus = XAxiDma_IntrGetIrq(axi_dma->returnXAxiDma(), XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrAckIrq(axi_dma->returnXAxiDma(), IrqStatus, XAXIDMA_DMA_TO_DEVICE);
	if(!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)){
		return;
	}
	if((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)){
		XAxiDma_Reset(axi_dma->returnXAxiDma());
		return;
	}
	if((IrqStatus & XAXIDMA_IRQ_IOC_MASK)){
	//	printf("dma trams success\n\r");
        xSemaphoreGiveFromISR(semDma,&wake);
        portYIELD_FROM_ISR(wake);
	}
	printf("dma trams success\n\r");
  //  XScuGic_Enable(&xInterruptController,62U);
}

static void tskData(void* pvParameters){
    if(DEBUG)
        printf("in data task\n\r");
    u8* p;
    int ret;
    while(1){
        if(xSemaphoreTake(semDataBufRd,portMAX_DELAY)){//缁涘绶熼惄鏉戝煂閺堝淇婇崣鐑藉櫤瀵拷婵澧界悰锟�
            if(DEBUG)
                printf("sending data to device\n\r");
            p=databuf[databuf_rd_p];
            databuf_rd_p=databuf_rd_p==DATA_BUF_NUM-1?0:databuf_rd_p+1;
//            printf("start trams\n\r");
//            for(int i=0;i<DATA_BUF_SIZE/DMA_TRANS_SIZE;i++){
//                if(xSemaphoreTake(semFifoEmpty,portMAX_DELAY)){
                    // dma transfer
            Xil_DCacheFlushRange((UINTPTR)p, DMA_TRANS_SIZE);
              //  	printf("start trams");
            ret=axi_dma->DmaSimpleTransfer((UINTPTR)p, DMA_TRANS_SIZE, XAXIDMA_DMA_TO_DEVICE);
              //      printf("trams end");
            //Xil_DCacheInvalidateRange((UINTPTR)p, DMA_TRANS_SIZE);
            //((UINTPTR)p, DMA_TRANS_SIZE);
//                    printf("end\n\r");
//                    p=p+DMA_TRANS_SIZE;
            xSemaphoreTake(semDma,portMAX_DELAY);   // blocking wait dma for finishing
//                }

            xSemaphoreGive(semDataBufWr);
        }
    }
    
    vTaskDelete(NULL);
}

void tskDataInit()
{
    int ret;

    semFifoEmpty=xSemaphoreCreateBinary();
    configASSERT(semFifoEmpty);
   // xSemaphoreGive(semFifoEmpty);
    semDma=xSemaphoreCreateBinary();
    configASSERT(semDma);

    // register interrupt
    //intc->InitChannelInt(62U, fifo_ISR, (void *)NULL);
//    XScuGic_Connect(&xInterruptController,62U,(Xil_ExceptionHandler)fifo_ISR,(void*)NULL);
//    XScuGic_Enable(&xInterruptController,62U);

    axi_dma->DmaReset();
    XScuGic_Connect(&xInterruptController,61U,(Xil_ExceptionHandler)axi_dma_mm2s_ISR,(void*)axi_dma);
    XScuGic_Enable(&xInterruptController,61U);
	//intc->InitChannelInt(XPAR_FABRIC_AXIDMA_0_VEC_ID, axi_dma_mm2s_ISR, (void *)axi_dma);
	XAxiDma_IntrEnable(axi_dma->returnXAxiDma(), XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);

    ret=xTaskCreate(tskData,"TskData",1024,NULL,tskDataPrio,&tskDataHandle);
    configASSERT(ret==pdPASS);
}
