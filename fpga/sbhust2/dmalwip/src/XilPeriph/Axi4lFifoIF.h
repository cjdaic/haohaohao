/**
 * @file  Axi4lFifoIF.hpp
 * @brief driver for Axi4lFifoIF
 * @details
 * @author Duna
 * @version 1.0.0
 */

#ifndef __AXI4LFIFOIF_HPP__
#define __AXI4LFIFOIF_HPP__
#include "xil_types.h"
#include "xstatus.h"
#include "XilIntc.h"
#include <set>

/**
 * ===================IP functionanity===========================
 * reg #       rd                  wr
    0       rx_fifo_dout        tx_fifo_din
    1       rx_fifo_dcnt        write any value to clear rx_fifo
    2       tx_fifo_dcnt        ........................ tx_fifo
    3       rxf_irq_th          rxf_irq_th
    4       txe_irq_th          txe_irq_th
    
    8       ie = {txe, rxf}     ie = {txe, rxf}
    9       is = {txe, rxf}     xxxxx
    * 
    * in this driver we only focus on rx_fifo_cnt, rxf_irq_th, ie
    * • rx_fifo_cnt: records there are how many data in the fifo.记录FIFO中有多少数据可读
    * • rxf_irq_th : when rx_fifo_cnt>rxf_irq_th and intrrupt mask(ie) is set 1, interrupt occurs. Interrupt is generated with {ie[0] && (rx_fifo_cnt>rxf_irq_th)}
    *                当rx_fifo_cnt>rxf_irq_th并且中断掩码设置为1，产生中断。中断生成利用 {ie[0] && (rx_fifo_cnt>rxf_irq_th)}
    * • ie         : interrupt mask. Usage: see rxf_irq_th usage
    * ===============================================================
    */
   
class Axi4lFifoIF{
    public:
        /// base address of fifo, defined in xparameters.h
        u32 FIFO_BASEADDR;

        /// IRQ id. This IRQ is from PL to PS. Must be 61-68,84-91(closure).
        /// define in xparameters.h
        u32 FIFO_INTR_ID;
    public:
        XilIntc* intc;
    public:
        enum class RegisterID {
            RX_FIFO_DOUT=0,RX_FIFO_DCNT=1,TX_FIFO_DCNT=2,
            RXF_IRQ_TH=3,TXE_IRQ_TH=4,
            IE=8,IS=9
        };
        static std::set<Axi4lFifoIF::RegisterID> register_id_set;
    public:
        /**
         * Axi4lFifoIF. hpp
         * 
         * construct function
         * empty construct function does't exist any more
         */
        Axi4lFifoIF(u32 FIFO_BASEADDR,u32 FIFO_INTR_ID,XilIntc* intc);
        /**
         * Axi4lFifoIF. hpp
         * 
         * destruct function
         */
        virtual ~Axi4lFifoIF();
        /**
         * Axi4lFifoIF. hpp
         * 
         * read the content in register through AXI4 bus
         * @param register_id this register #id is define by ip.See ip functionality
         * @return value of register
         */
        u32 get_register_value32(Axi4lFifoIF::RegisterID register_id);
        /**
         * Axi4lFifoIF. hpp
         * 
         * set the content in register through AXI4 bus
         * @param register_id this register #id is define by ip.See ip functionality
         * @param value set the register content with this value
         * @return status
         */
        int set_register_value32(Axi4lFifoIF::RegisterID register_id,u32 value);
        /**
         * Axi4lFifoIF. hpp
         * 
         * get rx irq threshold, register #3
         * @return value of register rxf_irq_th
         */
        u32 get_rxf_irq_th();
        /**
         * Axi4lFifoIF. hpp
         * 
         * set rx irq threshold, register #3
         * @param u32 threshold
         * @return status
         */
        int set_rxf_irq_th(u32 threshold);
        /**
         * Axi4lFifoIF. hpp
         * 
         * @return value of register ie
         */
        u32 get_ie();
        /**
         * Axi4lFifoIF. hpp
         * 
         * set ie, register #8
         * @return status
         */
        int set_ie(u32 value);
        /**
         * Axi4lFifoIF. hpp
         * 
         * set ie[0]=1, enable rx interrupt, regitser #8
         * @return status
         */
        int enable_ie_rxf();
        /**
         * Axi4lFifoIF. hpp
         * 
         * set ie[0]=0, disable rx interrupt, register #8
         * @return status
         */
        int disable_ie_rxf();
        /**
         * Axi4lFifoIF. hpp
         * 
         * get data quantity in fifo, register #1
         * @return value of register rx_fifo_dcnt
         */
        u32 get_rx_dcnt();

        int Axi4lFifoIfRegisterInterrupt(Xil_ExceptionHandler handler);
        
        int testbench();
    };

#endif