/**
 * @file  Eth.h
 * @brief tcp driver
 * @details
 * @author Duna
 * @version 1.0.0
 */

#ifndef __ETH_H__
#define __ETH_H__

#include "FreeRTOS.h"
#include "semphr.h"
#include "xparameters.h"
#include "xil_types.h"
#include "xstatus.h"
#include <string>

#include "xlwipconfig.h"
#include "lwipopts.h"
#include "netif/xadapter.h"
#include "lwip/init.h"
#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

class Eth{
	public:
		/**
		 * mac address
		 */
		u8_t mac_addr[6]={0x00,0x0a,0x35,0x00,0x01,0x02};
	public:
		/**
		 * ps eth base address
		 */
		u32 ETH_BASEADDR=0xE000B000;
	public:
		/**
		 * default ip address,netmask,gate way for tcp client side
		 */
		std::string ip_addr;
		std::string netmask;
		std::string gw;
	public:
		struct netif net_if;
	public:
		/**
		 * Eth. h
		 * 
		 * construct function
		 * @param ETH_BASEADDR ps ethernet base address
		 * @param ip_addr tcp client side ip address
		 * @param netmask tcp client side netmask
		 * @param gw	  tcp client side gate way
		 * @return
		 */
		Eth(u32 ETH_BASEADDR,std::string ip_addr,std::string netmask,std::string gw);
		/**
		 * TcpClient. hpp
		 * 
		 * destruct function
		 * @return
		 */
		virtual ~Eth();
		/**
		 * init function for tcp client
		 * this is just a simple wrapper for 'lwip_init'
		 * @return
		 */
		int init();
		/**
		 * TcpClient. hpp
		 * 
		 * ps mac negotiate with phy to determine a speed
		 */
		int mac_phy_auto_negotiation();
		/**
		 * assign ip_addr,netmask,gw to tcp client netif handle
		 * NOTE: this function must be called after 'xemacif_input_thread' scheduled
		 * @return
		 */
		int assign_ip();
		/**
		 * TcpClient. hpp
		 * 
		 * set tcp client mac address
		 * NOTE: this function must be called before 'mac_phy_autonegociation'
		 * 
		 * @param mac_addr mac address to set
		 * @return status
		 */
		int set_mac_addr(u8_t mac_addr[6]);
		
		int launch_server(int port);
		/**
		 * TcpClient. hpp
		 * 
		 * send a packet. This is a wrapper for 'lwip_send'
		 * @param sock socket handle
		 * @param buf  data would be transfered
		 * @param send_size how many data to send in buf
		 * @param apiflags tcp send symbol
		 * @return bytes send
		 */
		int send_packet(int sock,u_char* buf,size_t send_size,u8_t apiflags=MSG_MORE);
	};

#endif
