#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>


// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;
	int i;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING) and never choose an
	// idle environment (env_type == ENV_TYPE_IDLE).  If there are
	// no runnable environments, simply drop through to the code
	// below to switch to this CPU's idle environment.

	// LAB 4: Your code here.

	// Lab 4 ex 6
	//if(thiscpu->cpu_env == NULL)cprintf("Helloooo");
/*	int index = 0;
	if(curenv == NULL) 
	{
	for(i = NCPU; i<NENV; i++)
	{
		if((envs[i].env_status == ENV_RUNNABLE) && (envs[i].env_type != ENV_TYPE_IDLE))
			{
				env_run(&envs[i]);
				return;
			}
	}
	}
	else
	{
		int Env_index = (int) ENVX(curenv->env_id);	
		if(Env_index < NENV - 1)
			for(i = Env_index +1 ; i<NENV; i++){
				if((envs[i].env_status == ENV_RUNNABLE) && (envs[i].env_type != ENV_TYPE_IDLE) ){
					env_run(&envs[i]);
					return;
			}	
	}
		if(Env_index > NCPU || Env_index == NENV-1 )
		for(i = NCPU ; i<Env_index -1; i++){
			 if((envs[i].env_status == ENV_RUNNABLE) && (envs[i].env_type != ENV_TYPE_IDLE) ){  
                                env_run(&envs[i]);
				return;
        		}
		}
	
	
	
	if(curenv->env_status == ENV_RUNNING)
	{
		env_run(&envs[ENVX(curenv->env_id)]);
		return;
		}
	}
*/	
	struct Env *currentEnv = thiscpu->cpu_env;
	
	int index = NCPU-1;
	
	if(currentEnv != NULL)
		index = ENVX(currentEnv->env_id);
	
	i = (index+1)%NENV;
	
	while( i != index)
	{
		if(i < NCPU) 
		{
			i++;
			continue;
		}
		if(envs[i].env_type != ENV_TYPE_IDLE  && envs[i].env_status == ENV_RUNNABLE)
			break;
		i = (i +1)%NENV;
	}
	if( envs[i].env_type != ENV_TYPE_IDLE && (envs[i].env_status == ENV_RUNNABLE || 
			( i == index && envs[i].env_status == ENV_RUNNING)))
	{
		env_run(&envs[i]);
		return;
	}
	
	
	// For debugging and testing purposes, if there are no
	// runnable environments other than the idle environments,
	// drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if (envs[i].env_type != ENV_TYPE_IDLE &&
		    (envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING))
			break;
	}
	if (i == NENV) {
		cprintf("No more runnable environments!\n");
		while (1)
			monitor(NULL);
	}

	// Run this CPU's idle environment when nothing else is runnable.
	idle = &envs[cpunum()];
	if (!(idle->env_status == ENV_RUNNABLE || idle->env_status == ENV_RUNNING))
		panic("CPU %d: No idle environment!", cpunum());
	env_run(idle);
}
