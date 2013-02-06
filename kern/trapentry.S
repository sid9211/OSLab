/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushq $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushq $0;							\
	pushq $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
   Find the PUSHA,POPA macros in kern/macro.h. Since amd64 doesn't support
   pusha,popa so you will find these macros handy.
 */


/*
*	TRAPS
*
*/

	TRAPHANDLER_NOEC(trapDivide, T_DIVIDE)
    	TRAPHANDLER_NOEC(trapDebug, T_DEBUG)
    	TRAPHANDLER_NOEC(trapNmi, T_NMI)
    	TRAPHANDLER_NOEC(trapBrkpt, T_BRKPT)
    	TRAPHANDLER_NOEC(trapOflow, T_OFLOW)
    	TRAPHANDLER_NOEC(trapBound, T_BOUND)
    	TRAPHANDLER_NOEC(trapIllop, T_ILLOP)
    	TRAPHANDLER_NOEC(trapDevice, T_DEVICE)
    	TRAPHANDLER     (trapDblFlt, T_DBLFLT)
	TRAPHANDLER     (trapTss, T_TSS)
    	TRAPHANDLER	(trapSegnp, T_SEGNP)
    	TRAPHANDLER	(trapStack, T_STACK)
    	TRAPHANDLER	(trapGpflt, T_GPFLT)
    	TRAPHANDLER	(trapPgflt, T_PGFLT)
	TRAPHANDLER_NOEC(trapFpErr, T_FPERR)
	TRAPHANDLER_NOEC(trapSyscall,T_SYSCALL)



/*
*	IRQs
*
*/

	TRAPHANDLER_NOEC(irq32, IRQ_OFFSET)
    	TRAPHANDLER_NOEC(irq33, IRQ_OFFSET+1)
   	TRAPHANDLER_NOEC(irq34, IRQ_OFFSET+2)
    	TRAPHANDLER_NOEC(irq35, IRQ_OFFSET+3)
    	TRAPHANDLER_NOEC(irq36, IRQ_OFFSET+4)
    	TRAPHANDLER_NOEC(irq37, IRQ_OFFSET+5)
    	TRAPHANDLER_NOEC(irq38, IRQ_OFFSET+6)
    	TRAPHANDLER_NOEC(irq39, IRQ_OFFSET+7)
    	TRAPHANDLER_NOEC(irq40, IRQ_OFFSET+8)
    	TRAPHANDLER_NOEC(irq41, IRQ_OFFSET+9)
    	TRAPHANDLER_NOEC(irq42, IRQ_OFFSET+10)
    	TRAPHANDLER_NOEC(irq43, IRQ_OFFSET+11)
    	TRAPHANDLER_NOEC(irq44, IRQ_OFFSET+12)
    	TRAPHANDLER_NOEC(irq45, IRQ_OFFSET+13)
    	TRAPHANDLER_NOEC(irq46, IRQ_OFFSET+14)
    	TRAPHANDLER_NOEC(irq47, IRQ_OFFSET+15)

/*
 * Lab 3: Your code here for _alltraps
 */
       _alltraps:
	movq $0, %r15
	movw %ds,%r15 
   	pushq %r15
	movw %es, %r15
    	pushq %r15
    	PUSHA
	mov %rsp, %rdi
	movq $0, %r15
    	movw $(GD_KD), %r15
    	movw %r15, %es
    	movw %r15, %ds
    	call trap
    	POPA_
	//Now the func returns
	mov $0, %rax
	popq %rax
	movw %rax, %es
	popq %rax
	movw %rax, %ds
	addq $8, %rsp
    	iret  
