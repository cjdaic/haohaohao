#include "XilAxiDma.h"

XilAxiDma::XilAxiDma
(
		u8				deviceId,
		u8				mm2sIntVecId,
		u8				s2mmIntVecId,
		XilIntc*		intc,
		void*			axiDmaMM2SIsr(void*),
		void*			axiDmaS2MMIsr(void*)
)
{
	int Status;
	this->deviceId = deviceId;
	this->mm2sIntVecId = mm2sIntVecId;
	this->s2mmIntVecId = s2mmIntVecId;
	this->intc = intc;
	XilAxiDmaInit();
}

int XilAxiDma::XilAxiDmaInit()
{
	int status;

	XAxiDma_Config 			*Config = NULL;
	Config = XAxiDma_LookupConfig(this->deviceId);
	if(Config == NULL){
		xil_printf("No config found for DMA %d\r\n", deviceId);
		return XST_FAILURE;
	}

	status = XAxiDma_CfgInitialize(&(this->Dma), Config);
	if(status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	XAxiDma_IntrDisable(&(this->Dma), XAXIDMA_IRQ_ALL_MASK,XAXIDMA_DMA_TO_DEVICE);
    XAxiDma_IntrDisable(&(this->Dma), XAXIDMA_IRQ_ALL_MASK,XAXIDMA_DEVICE_TO_DMA);
	return 0;
}

int XilAxiDma::XilAxiDmaRegisterInterrupt(void* axiDmaMM2SIsr(void*), void* axiDmaS2MMIsr(void*))
{
	int status;
	if(axiDmaMM2SIsr != NULL){
		XAxiDma_IntrEnable(&(this->Dma), XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
		status = intc->InitChannelInt(this->mm2sIntVecId, (XInterruptHandler)axiDmaMM2SIsr, (void *)this);
	}

	if(axiDmaS2MMIsr != NULL){
		XAxiDma_IntrEnable(&(this->Dma), XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);
		status = intc->InitChannelInt(this->s2mmIntVecId, (XInterruptHandler)axiDmaS2MMIsr, (void *)this);
	}
}

int XilAxiDma::DmaSimpleTransfer(UINTPTR BuffAddr, u32 Length, int Direction)
{
	int Status;

	Status =  XAxiDma_SimpleTransfer(&(this->Dma), BuffAddr, Length, Direction);
	return Status;
}

void XilAxiDma::DmaReset(){
	XAxiDma_Reset(&(this->Dma));
	while(!XAxiDma_ResetIsDone(&(this->Dma)));
}

XAxiDma* XilAxiDma::returnXAxiDma()
{
	return &this->Dma;
}
