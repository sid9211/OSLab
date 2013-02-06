
#include "fs.h"

// Return the virtual address of this disk block.
void*
diskaddr(uint64_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (vpml4e[VPML4E(va)] & PTE_P) && (vpde[VPDPE(va)] & PTE_P) && (vpd[VPD(va)] & PTE_P) && (vpt[PPN(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
	return (vpt[PPN(va)] & PTE_D) != 0;
}

// Fault any disk block that is read or written in to memory by
// loading it from disk.
// Hint: Use ide_read and BLKSECTS.
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint64_t blockno = ((uint64_t)addr - DISKMAP) / BLKSIZE;
	int r;

	//cprintf(" Hello bc.c\n");
	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_rip, addr, utf->utf_err);
	//cprintf(" Hi bc.c\n");
	// Sanity check the block number.
	//cprintf(" S_NBLOCKS IN BC.C %d",super->s_nblocks); 
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page, and mark the
	// page not-dirty (since reading the data from disk will mark
	// the page dirty).
	
	// Lab 5 ex 2
	// LAB 5: Your code here

	int i =0;
	addr =(void *) ROUNDDOWN(addr, PGSIZE);
	i = sys_page_alloc(0, addr, PTE_SYSCALL);
	if(i < 0)
		panic(" Sys_page_alloc fails for bc_pgfault \n");
		
	i = ide_read((uint32_t)(blockno * BLKSECTS), addr, BLKSECTS);
	if(i < 0)
		panic(" ide_read fails for bc_pgfault \n");

	// How to mark the page non-Dirty ? Using pml4e walk? or need to use VPT?? 
	
	// Awesome trick learnt from Flush_block's comments
	
	i = sys_page_map(0, addr, 0, addr, PTE_SYSCALL);
        if(i < 0)
                panic(" sys_page_map failed in bc_pgfault  \n ");

	//panic("bc_pgfault not implemented");

	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_SYSCALL constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	uint64_t blockno = ((uint64_t)addr - DISKMAP) / BLKSIZE;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);
	
	// Lab 5 ex 2
	// LAB 5: Your code here.

	//Do I need to ROUNDDOWN here only? How does VPD work?? 

	int i = 0;
	if(!va_is_mapped(addr))
		return;
	if(!va_is_dirty(addr))
		return;
	
	addr =(void *) ROUNDDOWN(addr,PGSIZE);
	i = ide_write((uint32_t)(blockno * BLKSECTS), addr, BLKSECTS);
	if(i < 0)
		panic("ide_write failed for fluch_block \n");
	// The following is a cool trick

	// -- Why zero?? 
	i = sys_page_map(0, addr, 0, addr, PTE_SYSCALL);
	if(i < 0)
		panic(" sys_page_map failed in flush_block \n ");
	

	//panic("flush_block not implemented");
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{
	set_pgfault_handler(bc_pgfault);
	check_bc();
}

