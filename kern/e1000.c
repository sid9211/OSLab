#include <kern/e1000.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <kern/pmap.h>
// LAB 6: Your driver code here

volatile uint32_t *e_mmio;

struct tx_desc tx_que[MAXTX_DESC]__attribute__ ((aligned (16)));
struct tx_pkt pkt_que[MAXTX_DESC];

struct rx_desc rx_que[MAXTX_DESC]__attribute__ ((aligned (16)));
struct rx_pkt rpkt_que[MAXTX_DESC];


int attachE1000(struct pci_func *pcif)
{
	pci_func_enable(pcif);
	
	int i;
	
	e_mmio = (uint32_t *)mmio_map_region((physaddr_t) pcif->reg_base[0],(size_t) pcif->reg_size[0]);

		
	// transmit_init
	memset(tx_que, 0x0, sizeof(struct tx_desc) * MAXTX_DESC);
	memset(pkt_que, 0x0, sizeof(struct tx_pkt) * MAXTX_DESC);
	
	for( i=0; i<MAXTX_DESC; i++)
	{
		tx_que[i].addr = PADDR(pkt_que[i].arr);
		tx_que[i].status |= E1000_TXD_STAT_DD;
	}

	//receive_init
	memset(rx_que, 0x0, sizeof(struct rx_desc) * MAXTX_DESC);
        memset(rpkt_que, 0x0, sizeof(struct rx_pkt) * MAXTX_DESC);

        for( i=0; i<MAXTX_DESC; i++)
        {
                rx_que[i].addr = PADDR(rpkt_que[i].arr);
                //tx_que[i].status |= E1000_TXD_STAT_DD;
        }
	
	

	e_mmio[E1000_RAL] = 0x52;
	e_mmio[E1000_RAL] |= (0x54) << 8;
	e_mmio[E1000_RAL] |= (0x00) << 16;
	e_mmio[E1000_RAL] |= (0x12) << 24;
	e_mmio[E1000_RAH] |= (0x34);
	e_mmio[E1000_RAH] |= (0x56) << 8;
	e_mmio[E1000_RAH] |= 0x80000000;
		
	//initialization of various registers
	e_mmio[E1000_TDBAH] = 0x0; 
	e_mmio[E1000_TDBAL] = PADDR(tx_que);
	e_mmio[E1000_TDLEN] = sizeof(struct tx_desc) * MAXTX_DESC;
	e_mmio[E1000_TDH] = 0x0;
	e_mmio[E1000_TDT] = 0x0;
	
	
	e_mmio[E1000_RDBAH] = 0x0;
        e_mmio[E1000_RDBAL] = PADDR(rx_que);
        e_mmio[E1000_RDLEN] = sizeof(struct rx_desc) * MAXTX_DESC;
        e_mmio[E1000_RDH] = 0x0;
        e_mmio[E1000_RDT] = 0x0;


	e_mmio[E1000_TCTL] |=  E1000_TCTL_EN|E1000_TCTL_PSP|(E1000_TCTL_CT & (0x10 << 4))|(E1000_TCTL_COLD & (0x40 << 12));

	
	e_mmio[E1000_RCTL] |= E1000_RCTL_EN;
	e_mmio[E1000_RCTL] &= ~E1000_RCTL_LPE;
	e_mmio[E1000_RCTL] &= ~(E1000_RCTL_LBM_MAC | E1000_RCTL_LBM_SLP |E1000_RCTL_LBM_TCVR);
	e_mmio[E1000_RCTL] &= ~(E1000_RCTL_RDMTS_QUAT | E1000_RCTL_RDMTS_EIGTH);
	e_mmio[E1000_RCTL] &= ~(E1000_RCTL_MO_3);
	e_mmio[E1000_RCTL] &= ~E1000_RCTL_BAM;
	e_mmio[E1000_RCTL] &= ~(E1000_RCTL_BSEX);
	e_mmio[E1000_RCTL] &= ~(E1000_RCTL_SZ_256);

	e_mmio[E1000_RCTL] |= E1000_RCTL_SECRC; 

	e_mmio[E1000_TIPG] = 0x0;
	e_mmio[E1000_TIPG] |= 0xA;
	e_mmio[E1000_TIPG] |= (0x6) << 20;
	e_mmio[E1000_TIPG] |= (0x4) << 10;	

/*	char a = 'A';
	char b = 'B';
	transmit(&a, 1);
	transmit(&b, 1);
	char c = 'B';
        transmit(&c, 1);
*/	return 0;
}


int transmit( const char * addr, size_t bytes)
{
	if(bytes > PKT_SIZE)
		return -error_long_packet;
	uint32_t tdt = e_mmio[E1000_TDT];
	if(tx_que[tdt].status & E1000_TXD_STAT_DD)
	{
		memmove(pkt_que[tdt].arr, addr, bytes);
		tx_que[tdt].status &= ~E1000_TXD_STAT_DD;
		tx_que[tdt].cmd |= E1000_TXD_CMD_EOP;
		tx_que[tdt].length = bytes;
		tx_que[tdt].cmd |= E1000_TXD_CMD_RS;
		tdt++;
		e_mmio[E1000_TDT] = tdt%MAXTX_DESC;
		return 0;
	}
	else
		return -error_queue_full;
}


int receive (const char *addr)
{
	uint32_t rdt = e_mmio[E1000_RDT];
	if((rx_que[rdt].status & E1000_RXD_STAT_DD) && (rx_que[rdt].status & E1000_RXD_STAT_EOP))
	{
		int bytes = rx_que[rdt].length;
		memmove((void *)addr,(void *) rpkt_que[rdt].arr, bytes);
		rx_que[rdt].status &= ~E1000_RXD_STAT_DD;
		rx_que[rdt].status &= ~E1000_RXD_STAT_EOP;
		rdt++;
		e_mmio[E1000_RDT] = rdt%MAXTX_DESC;
		return bytes;// something
	}
	else
	{
		return -1;//error
	}
}

