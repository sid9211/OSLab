// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display the rbp, rip and arguments ", mon_backtrace }
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}


/************************************************************/
/********************mon_backtrace***************************/
/*     int mon_backtrace(int,char **,struct Trapframe *)    */
/************************************************************/
/*         This function prints the rbp, rip adn arguments
           of the calling function to the end of stack
	   then it also prints the file names, the
	   return addresses and function names calling it
	   and the offset of the return address of the calling
	   functions
*************************************************************/
int mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uint64_t *a = (uint64_t *)read_rbp(); 		//Get the value of rbp into *a
	uint64_t x = *a;				// Variable to keep track of the end of stack
	
	do{
		
		x = *a;
		
		//Print values as per the given format --- rbp, then rip, then argument values
		cprintf("rbp %016x rip %016x args %016x %016x %016x %016x\n",a,*(a+1),*((int *)a - 1),*((int*)a - 2), *((int *)a -3), *((int *)a -4));
		uintptr_t ripVal = (uintptr_t) *(a+1);
		struct Ripdebuginfo info;
		int status = debuginfo_rip(ripVal,&info);
		
		if(status == 0)                   // Cannot handle user address, and returns negative for call from userspace
		{
		
			uintptr_t offset = (uintptr_t)ripVal - info.rip_fn_addr;    		//Finds the offset of the return address from the start of function
		
			cprintf("../%s",info.rip_file);			//Prints file location
			cprintf(":%d:",info.rip_line);		//Prints line number
			cprintf(" %.*s",info.rip_fn_namelen,(char *)info.rip_fn_name);		//Prints function name
			cprintf("+%016x\n",offset);			//Prints offset of the return address from the function start address
		}
	
		a = (uint64_t *)*a; 		//Goto the upper function rbp
	}while(x!=0);		//x>=0xf010a000);			//Repeat till end of stack
	
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
