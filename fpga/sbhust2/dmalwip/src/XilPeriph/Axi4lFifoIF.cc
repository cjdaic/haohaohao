#include "Axi4lFifoIF.h"
#include "xil_io.h"
#include "stdlib.h"
#include <set>
#include <assert.h>


using namespace std;

std::set<Axi4lFifoIF::RegisterID> Axi4lFifoIF::register_id_set={
    Axi4lFifoIF::RegisterID::RX_FIFO_DCNT,
    Axi4lFifoIF::RegisterID::RXF_IRQ_TH,
    Axi4lFifoIF::RegisterID::IE
};

Axi4lFifoIF::Axi4lFifoIF(u32 FIFO_BASEADDR,u32 FIFO_INTR_ID,XilIntc* intc)
    :FIFO_BASEADDR(FIFO_BASEADDR),FIFO_INTR_ID(FIFO_INTR_ID),intc(intc)
{
    
}

Axi4lFifoIF::~Axi4lFifoIF()
{

}

u32 Axi4lFifoIF::get_register_value32(Axi4lFifoIF::RegisterID register_id)
{
    assert(Axi4lFifoIF::register_id_set.find(register_id)!=register_id_set.end());
    u32 t=Xil_In32(this->FIFO_BASEADDR+4*(u32)register_id);
    return t;
}

int Axi4lFifoIF::set_register_value32(Axi4lFifoIF::RegisterID register_id,u32 value)
{
    assert(Axi4lFifoIF::register_id_set.find(register_id)!=register_id_set.end());
    Xil_Out32(this->FIFO_BASEADDR+4*(u32)register_id,value);
    return XST_SUCCESS;
}

u32 Axi4lFifoIF::get_rxf_irq_th()
{
    return this->get_register_value32(Axi4lFifoIF::RegisterID::RXF_IRQ_TH);
}

int Axi4lFifoIF::set_rxf_irq_th(u32 threshold)
{
    return this->set_register_value32(Axi4lFifoIF::RegisterID::RXF_IRQ_TH,threshold);
}

u32 Axi4lFifoIF::get_ie()
{
    return this->get_register_value32(Axi4lFifoIF::RegisterID::IE);
}

int Axi4lFifoIF::set_ie(u32 value)
{
    return this->set_register_value32(Axi4lFifoIF::RegisterID::IE,value);
}

int Axi4lFifoIF::enable_ie_rxf()
{
    u32 t=this->get_ie();
    t|=0x00000001;
    return this->set_ie(t);
}

int Axi4lFifoIF::disable_ie_rxf()
{
    u32 t=this->get_ie();
    t&=~0x00000001;
    return this->set_ie(t);
}

u32 Axi4lFifoIF::get_rx_dcnt()
{
    return this->get_register_value32(Axi4lFifoIF::RegisterID::RX_FIFO_DCNT);
}

int Axi4lFifoIF::Axi4lFifoIfRegisterInterrupt(Xil_ExceptionHandler handler)
{
    int status;
    status=intc->InitChannelInt(this->FIFO_INTR_ID, (XInterruptHandler)handler, (void *)this);
    return status;
}

int testbench(){

}