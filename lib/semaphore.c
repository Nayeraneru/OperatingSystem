// User-level Semaphore

#include "inc/lib.h"

struct semaphore create_semaphore(char *semaphoreName, uint32 value)
{
	//[PROJECT'24.MS3]
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("create_semaphore is not implemented yet");
	//Your Code is Here...
       // cprintf("in createee semaphoreee\n");
        struct semaphore sem;
        sem.semdata = (struct __semdata *)smalloc(semaphoreName, sizeof(struct __semdata), 1);
		if (sem.semdata == NULL){
			return sem;
		}

		sem.semdata->count = value;
		sem.semdata->lock = 0;
		strncpy(sem.semdata->name, semaphoreName, sizeof(sem.semdata->name) - 1);

		sys_queue_init(&(sem.semdata->queue));

		return sem;
}
struct semaphore get_semaphore(int32 ownerEnvID, char* semaphoreName)
{
	//[PROJECT'24.MS3]
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("get_semaphore is not implemented yet");
	//Your Code is Here...
	//cprintf("in get semaphoreee\n");
	struct semaphore sem;
	sem.semdata = (struct __semdata *)sget(ownerEnvID, semaphoreName);
	if (sem.semdata == NULL) {
		return sem;
	}

	return sem;
}

void wait_semaphore(struct semaphore sem)
{
	//[PROJECT'24.MS3]
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("wait_semaphore is not implemented yet");
	//Your Code is Here...
	//cprintf("in waittt semaphoreee\n");
	int key = 1;
	while(xchg(&(sem.semdata->lock),1) != 0);
	sem.semdata->count--;
//	//struct Env* env = sys_cur_proc();
//	if (env == NULL){
//		cprintf("env equal null semaphoreee\n");
//		 return;
//	 }
	if(sem.semdata->count < 0){

		//sem.semdata->lock = 0;
		sys_enqueue_queue(&sem);
		//cprintf("enqueue in wait semaphoreee\n");
		//return;
	  // sys_schedd();

		//env->env_status = ENV_BLOCKED;
	}
	//cprintf("count in wait semaphoreee %d\n", sem.semdata->count);
	sem.semdata->lock = 0;
}

void signal_semaphore(struct semaphore sem)
{
	//[PROJECT'24.MS3]
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("signal_semaphore is not implemented yet");
	//Your Code is Here...
	        int keys = 1;

	       // cprintf("in signalll semaphoreee\n");
	        // Acquire lock using xchg operation
	        while (xchg(&(sem.semdata->lock),1) != 0);

	        // Increment semaphore count
	        sem.semdata->count++;
	       // cprintf("after count increment semaphoreee\n");
	        // If count is <= 0, unblock a process
	      //  struct Env* blocked_env = sys_cur_proc();
	        if (sem.semdata->count <= 0) {
	            // Remove a process from the semaphore's queue

	           // sys_dequeue_queue(&(sem.semdata->queue));
	          //  cprintf("after dequeue in semaphoreee\n");

	          //  cprintf("before insert in ready semaphoreee\n");
				sys_ready_queue(&(sem.semdata->queue));
			//	cprintf("after insert in ready semaphoreee\n");
				// Set the process state to ready
			   // blocked_env->env_status = ENV_READY;
				// Place the process on the ready list
//	                sem.semdata->lock = 0;
	        }
	      //  cprintf("count in signal semaphoreee %d\n", sem.semdata->count);
	        // Release lock
	        sem.semdata->lock = 0;
}

int semaphore_count(struct semaphore sem)
{
	return sem.semdata->count;
}
