/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv,s,len,PTE_P);
	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
	// Lab 4 ex 7
	struct Env *childEnv = NULL;

	if(curenv == NULL) panic("No parent env -- invalid fork");	

	int i = env_alloc(&childEnv,curenv->env_id);
	
	if(i == 0){
		childEnv->env_status = ENV_NOT_RUNNABLE;
		childEnv->env_tf = curenv->env_tf;
		//memcpy(&childEnv->env_tf , &curenv->env_tf,sizeof(struct Trapframe));
		childEnv->env_tf.tf_regs.reg_rax = 0; //return val
		
		childEnv->env_pgfault_upcall = curenv->env_pgfault_upcall;

		return childEnv->env_id;
		
	}
	else{
		//panic("problem exofork");
		return (envid_t)i; // E_NO_MEM
	}
	//panic("sys_exofork not implemented");
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
	// Lab 4 ex 7

	// To do lab 4 -- check permission	
	if(status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
                     return -E_INVAL;

	struct Env *env = NULL;
	
	int ifEnvSet = envid2env(envid,&env,1);
	
	if(ifEnvSet == 0 && env!= NULL)
	{
			envs[ENVX(envid)].env_status = status;
			//env->env_status = status;
			return 0;
	}
	else return -E_BAD_ENV;
	//panic("sys_env_set_status not implemented");
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	
	struct Env *env = NULL;

	int i;

	if((i = envid2env(envid,&env,1)) < 0) return i;

	user_mem_assert(env,tf,sizeof(struct Trapframe), PTE_U);

	tf->tf_cs |= 0x3;
	tf->tf_eflags |= FL_IF;	

	env->env_tf = *tf;	
	return 0;
	//panic("sys_env_set_trapframe not implemented");
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	// Lab 4 ex 8

	// To do lab 4 -- check permission - curevnv = running env
	struct Env *env = NULL;
	int ifEnvSet = envid2env(envid, &env, 1);
	if(ifEnvSet != 0)
	return -E_BAD_ENV;
	
	env->env_pgfault_upcall = func;

	return ifEnvSet;
//	panic("sys_env_set_pgfault_upcall not implemented");
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
	// To do lab 4--- check------permission problem, PTE
	// Lab 4 ex 7  
	if((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P))
	 return -E_INVAL;
	
	void *v1 = ROUNDDOWN(va,PGSIZE);
	if (v1 != va) return -E_INVAL;
	
	if((uint64_t)va >= UTOP) return -E_INVAL;
	
        struct Page *newPage  = page_alloc(ALLOC_ZERO);

        if(newPage == NULL) return -E_NO_MEM;
	//memset(page2kva(newPage), 0, PGSIZE);

	struct Env *env;

        int ifEnvSet = envid2env(envid,&env,1);

	if(ifEnvSet !=0) return -E_BAD_ENV;

	if(perm & ((~PTE_SYSCALL)& 0xfff))return -E_INVAL;
	
	int i = page_insert(env->env_pml4e,newPage,va,perm);
	
	if(i != 0)
	{
		 page_free(newPage);
		 return -E_NO_MEM;
	}
	
	return i;
	
	//panic("sys_page_alloc not implemented");
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	--E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	--E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	--E_INVAL is srcva is not mapped in srcenvid's address space.
//	--E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	// Lab 4 ex 7
	// To do lab 4 check perm

	int returnVal;
	struct Page *pageVal;
	struct Env *srcEnv,*destEnv;
	pte_t *srcPt,*destPt;


	if((uint64_t)srcva>UTOP)return -E_INVAL;
	if((uint64_t)dstva>UTOP)return -E_INVAL;
	if((uint64_t)srcva>(uint64_t)ROUNDDOWN(srcva,PGSIZE))return -E_INVAL;
	if((uint64_t)dstva>(uint64_t)ROUNDDOWN(dstva,PGSIZE))return -E_INVAL;

	if((perm & PTE_U) == 0) return -E_INVAL;
	if((perm & PTE_P) == 0) return -E_INVAL;
	if((perm & ~PTE_SYSCALL) != 0) return -E_INVAL;

	returnVal = envid2env(srcenvid,&srcEnv,0);
	if(returnVal!=0) return -E_BAD_ENV;

	returnVal = envid2env(dstenvid,&destEnv,0);
	if(returnVal!=0) return -E_BAD_ENV;

	pageVal = page_lookup(srcEnv->env_pml4e,srcva,&srcPt);
	if(pageVal == NULL) return -E_INVAL;

	if(!(perm & PTE_U) && !(perm & PTE_P)) return -E_INVAL;

	if(((perm & PTE_W) != 0) && ((*srcPt & PTE_W) == 0)) return -E_INVAL;

	returnVal = page_insert(destEnv->env_pml4e,pageVal,dstva,perm);

	if(returnVal != 0) return -E_INVAL;

	return returnVal;
}
// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	// Lab 4 ex 7
	void *v1 = ROUNDDOWN(va,PGSIZE);
	if(v1 != va) return -E_INVAL;

	if((uint64_t)va >= UTOP) return -E_INVAL;

        struct Env *env;

        int ifEnvSet = envid2env(envid,&env,1);

        if(ifEnvSet !=0) return -E_BAD_ENV;

	pte_t *pte;
        struct Page *page = page_lookup(env->env_pml4e,va,&pte);

        if(page == NULL) return -E_INVAL;

	page_remove(env->env_pml4e,va);

	return 0;
	//panic("sys_page_unmap not implemented");
}


// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	struct Env *env_recv ;
	
	int i;
	i = envid2env(envid,&env_recv, 0);
	
	if(i < 0)
		return -E_BAD_ENV;
	
	if(env_recv->env_ipc_recving != 1)
		return -E_IPC_NOT_RECV;
	
	pte_t *ptentry;
	
	if(srcva < (void *)UTOP)	
	{
		if(((uintptr_t)srcva % PGSIZE) != 0)
			return -E_INVAL;	
		
		if ( perm & ~(PTE_SYSCALL) || !(perm & (PTE_U|PTE_P))  )
			return -E_INVAL;
		
		if( (page_lookup(curenv->env_pml4e, srcva, &ptentry)) == NULL)
			return -E_INVAL;
		
		if( (perm & PTE_W) && !(*ptentry & PTE_W))
			return -E_INVAL;
	}
	
	if(env_recv->env_ipc_dstva<(void*)UTOP && srcva<(void*)UTOP)
	{
		i = sys_page_map(curenv->env_id,srcva,env_recv->env_id,env_recv->env_ipc_dstva, perm);
		if(i<0)
			return i;
		env_recv->env_ipc_perm = perm;
	}	
	
	env_recv->env_ipc_recving = 0;
	env_recv->env_ipc_from = curenv->env_id;
	env_recv->env_ipc_value = value;
	env_recv->env_status = ENV_RUNNABLE;

	return 0;
	
	//panic("sys_ipc_try_send not implemented");
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	
	if(dstva<(void*)UTOP)
	{
		if(((uintptr_t)dstva % PGSIZE) != 0)
			return -E_INVAL;
		
		curenv->env_ipc_dstva = dstva;
		curenv->env_ipc_perm = 0;
	}
	
	curenv->env_ipc_recving = 1;
	
	curenv->env_status = ENV_NOT_RUNNABLE;
	curenv->env_tf.tf_regs.reg_rax =  0;
	sched_yield();
	
	return 0;
	
	//panic("sys_ipc_recv not implemented");

}

// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.

	return time_msec();
	//panic("sys_time_msec not implemented");
}

//Syscall for packet transmission
static int
sys_transmit(const char *a1, size_t a2)
{
	 if((uint64_t)a1 >= UTOP) return -E_INVAL;
		return transmit(a1, a2);
}

//Syscall for packet reception
static int
sys_receive(char *data, int *length)
{
         if((uint64_t)data >= UTOP) return -E_INVAL;

	*length = receive(data);
	if (*length > 0)
		return 0;
	
	return *length;
}


// Dispatches to the correct kernel function, passing the arguments.
int64_t
syscall(uint64_t syscallno, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

	int64_t returnVal = -E_INVAL;

	if(syscallno == SYS_cputs){
		sys_cputs((char *)a1,(size_t)a2);
		returnVal = 0;
	}
	if(syscallno == SYS_cgetc){
		returnVal = (uint64_t)sys_cgetc();
	}
	if(syscallno == SYS_getenvid){
		returnVal = (uint64_t)sys_getenvid();
	}
	if(syscallno == SYS_env_destroy){
		returnVal = (uint64_t)sys_env_destroy((envid_t)a1);
	}
	if(syscallno == SYS_yield){
		sys_yield();
		returnVal = 0;
	} 
	if(syscallno == SYS_exofork){
		returnVal = (uint64_t)sys_exofork();
	}
	if(syscallno == SYS_env_set_status){
		returnVal = (uint64_t)sys_env_set_status((envid_t)a1,(uint64_t)a2);
	}
	if(syscallno == SYS_page_alloc){
		returnVal = (uint64_t)sys_page_alloc((envid_t)a1,(uint64_t *)a2,(int)a3);
	}
	if(syscallno == SYS_page_map){
		returnVal = (uint64_t)sys_page_map((envid_t)a1,(uint64_t *)a2,(envid_t)a3,(uint64_t *)a4,(int)a5);
	}
	if(syscallno == SYS_page_unmap){
		returnVal = (uint64_t)sys_page_unmap((envid_t)a1,(uint64_t *)a2);
	}
	if(syscallno == SYS_env_set_pgfault_upcall){
		returnVal = (uint64_t)sys_env_set_pgfault_upcall((envid_t)a1,(void *)a2);
	}
	if(syscallno == SYS_ipc_try_send){
		returnVal = (uint64_t)sys_ipc_try_send((envid_t)a1,(uint32_t)a2,(void *)a3,(unsigned)a4);
	}
	if(syscallno == SYS_ipc_recv){
		returnVal = (uint64_t)sys_ipc_recv((void *)a1);
	}
	if(syscallno == SYS_env_set_trapframe){
		returnVal = (uint64_t)sys_env_set_trapframe((envid_t)a1, (struct Trapframe *)a2);
	}
	if(syscallno == SYS_time_msec){
		returnVal = (uint64_t)sys_time_msec();
	}
	if(syscallno == SYS_transmit){
                returnVal = (uint64_t)sys_transmit((char *)a1, (size_t)a2);
	}
	if(syscallno == SYS_receive){
                returnVal = (uint64_t)sys_receive((char *)a1, (int  *)a2);
        }

	return returnVal;

}
