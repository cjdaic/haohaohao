#ifndef _XILAXIDMA_H_
#define _XILAXIDMA_H_
#include "xaxidma.h"
#include "XilIntc.h"
#include "xil_printf.h"
class XilAxiDma{
private:
	u8						deviceId;
	u8						mm2sIntVecId;
	u8						s2mmIntVecId;
	XilIntc*				intc;
	XAxiDma 				Dma;

public:
	XilAxiDma(
		u8				deviceId,
		u8				mm2sIntVecId,
		u8				s2mmIntVecId,
		XilIntc*		intc,
		void*			axiDmaMM2SIsr(void*),
		void*			axiDmaS2MMIsr(void*)
	);

	int XilAxiDmaInit();
	int XilAxiDmaRegisterInterrupt(void* axiDmaMM2SIsr(void*), void* axiDmaS2MMIsr(void*));
	int DmaSimpleTransfer(UINTPTR BuffAddr, u32 Length,int Direction);
	void DmaReset();
	XAxiDma* returnXAxiDma();
};

#endif
