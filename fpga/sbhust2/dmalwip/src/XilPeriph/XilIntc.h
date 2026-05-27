#ifndef __XIL_INTC_H__
#define __XIL_INTC_H__

#include "xparameters.h"
#include "xscugic.h"
#include "xil_printf.h"

class XilIntc{
private:
	XScuGic	 			Intc;
	XScuGic_Config 		*GicCfg;
	u8 					deviceId;


public:
	XilIntc(
		u8 IntcId
	);

	int XilIntcInit();
	int InitChannelInt(u8 InterruptVectorId, XInterruptHandler Interrupt_ISR, void *CallBack);
	void XilIntcExceptionEnable();
	void EnableIntr(u8 InterruptVectorId);
	void DisableIntr(u8 InterruptVectorId);
	XScuGic* returnXIntc();
};




#endif
