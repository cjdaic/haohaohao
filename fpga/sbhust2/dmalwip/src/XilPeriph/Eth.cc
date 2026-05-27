#include "Eth.h"
#include <string>

using namespace std;

Eth::Eth(u32 ETH_BASEADDR,string ip_addr,string netmask,string gw)
	:ETH_BASEADDR(ETH_BASEADDR),ip_addr(ip_addr),netmask(netmask),gw(gw)
{

}

Eth::~Eth()
{

}

int Eth::init(){
	lwip_init();
	return XST_SUCCESS;
}

int Eth::mac_phy_auto_negotiation()
{
	if (!xemac_add(&this->net_if,NULL,NULL,NULL,this->mac_addr,XPAR_PS7_ETHERNET_0_BASEADDR)) {
		return XST_FAILURE;
	}
	netif_set_default(&this->net_if);
	netif_set_up(&this->net_if);
}

int Eth::assign_ip()
{
	int err=1;
	err &= inet_aton((this->ip_addr).c_str(),&(this->net_if.ip_addr));
	err &= inet_aton(this->netmask.c_str(), &(this->net_if.netmask));
	err &= inet_aton(this->gw.c_str(),&(this->net_if.gw));
	if(!err){
		return XST_FAILURE;
	}
	return XST_SUCCESS;
}

int Eth::set_mac_addr(u8_t mac_addr[6])
{
	memcpy(this->mac_addr,mac_addr,sizeof(u8_t)*6);
	return XST_SUCCESS;
}

int Eth::launch_server(int port)
{
	int sock;
	struct sockaddr_in address;
	if((sock = lwip_socket(AF_INET, SOCK_STREAM,0))<0) {
		return sock;
	}
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = INADDR_ANY;

	if(bind(sock,(struct sockaddr *)&address,sizeof(address)) < 0) {
		close(sock);
		return sock;
	}

	if (listen(sock,1)<0) {
		close(sock);
		return sock;
	}

	return sock;
}

int Eth::send_packet(int sock,u_char* buf,size_t send_size,u8_t apiflags)
{
	int bytes_send;
	bytes_send=lwip_send(sock,buf,send_size,apiflags);
	if(bytes_send<0){
	}
	return bytes_send;
}
