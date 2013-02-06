#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	int RPKT_SIZE = 2048;
	
	int lengthOfPacket = RPKT_SIZE - 1;
	int r = 0;
	char buffer[RPKT_SIZE];
	
	while(1){
		while((r = sys_receive(buffer,&lengthOfPacket))<0){
		sys_yield();
	     }	
	
	while((r = sys_page_alloc(0, &nsipcbuf, PTE_P|PTE_U|PTE_W))<0);

	nsipcbuf.pkt.jp_len = lengthOfPacket;
	memmove(nsipcbuf.pkt.jp_data, buffer, lengthOfPacket);

	while((r = sys_ipc_try_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P|PTE_U|PTE_W)) < 0);
	
	}
}
