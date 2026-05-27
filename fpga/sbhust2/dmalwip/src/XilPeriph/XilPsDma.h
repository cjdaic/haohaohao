#ifndef __XIL_PS_DMA_H__
#define __XIL_PS_DMA_H__
#include "xdmaps.h"
#include "XilIntc.h"
#include "xil_printf.h"
class XilPsDma{
private:
	u8						deviceId;
	u8						mm2sIntVecId;
	u8						s2mmIntVecId;
	XilIntc*				intc;
	XDmaPs 					Dma;
	XDmaPs_Cmd				dmaps_cmd;
public:
	XilPsDma(
		u8				deviceId,
		XilIntc*		intc
	);

	int XilPsDmaInit();
	int XilPsDmaRegisterInterrupt(XDmaPsDoneHandler handler);
	int DmaSimpleTransfer(
		unsigned int SrcBurstSize,
		unsigned int SrcBurstLen,
		unsigned int SrcInc,
		unsigned int DstBurstSize,
		unsigned int DstBurstLen,
		unsigned int DstInc,
		u32 SrcAddr,
		u32 DstAddr,
		unsigned int Length
		);
	void DmaReset();
	XDmaPs* returnPsDma();
};

#endif
