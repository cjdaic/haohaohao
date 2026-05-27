#include "common.h"

XilIntc* intc;
XilAxiDma* axi_dma;
Eth* net;

void initPeriph()
{
    intc=new XilIntc(XPAR_SCUGIC_0_DEVICE_ID);
    axi_dma=new XilAxiDma(XPAR_AXI_DMA_0_DEVICE_ID,XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR,0U,intc,NULL,NULL);
    net=new Eth(XPAR_PS7_ETHERNET_0_BASEADDR,IP_ADDR,NETMASK,GW);
}