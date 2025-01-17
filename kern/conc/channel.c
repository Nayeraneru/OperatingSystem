/*
 * channel.c
 *
 *  Created on: Sep 22, 2024
 *      Author: HP
 */
#include "channel.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <inc/string.h>
#include <inc/disk.h>

//===============================
// 1) INITIALIZE THE CHANNEL:
//===============================
// initialize its lock & queue
void init_channel(struct Channel *chan, char *name)
{
	strcpy(chan->name, name);
	init_queue(&(chan->queue));
}

//===============================
// 2) SLEEP ON A GIVEN CHANNEL:
//===============================
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// Ref: xv6-x86 OS code
void sleep(struct Channel *chan, struct spinlock *lk)
{
    //TODO: [PROJECT'24.MS1 - #10] [4] LOCKS - sleep
 //COMMENT THE FOLLOWING LINE BEFORE START CODING
// panic("sleep is not implemented yet");
// set the curr process to wait on this channel
struct Env *currProc = get_cpu_proc();

acquire_spinlock(&ProcessQueues.qlock);
enqueue(&chan->queue, currProc);
//release_spinlock(&ProcessQueues.qlock);
release_spinlock(lk);   // releasing the guard
currProc->env_status = ENV_BLOCKED;

//acquire_spinlock(&ProcessQueues.qlock);
sched(); // sleep (but not really sleep)
release_spinlock(&ProcessQueues.qlock);
acquire_spinlock(lk); // to ensure that we are entering the critical section in the acquire of the sleep lock while still protected

}

//==================================================
// 3) WAKEUP ONE BLOCKED PROCESS ON A GIVEN CHANNEL:
//==================================================
// Wake up ONE process sleeping on chan.
// The qlock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes
void wakeup_one(struct Channel *chan)
{
	    //TODO: [PROJECT'24.MS1 - #11] [4] LOCKS - wakeup_one
		//COMMENT THE FOLLOWING LINE BEFORE START CODING
	    //panic("wakeup_one is not implemented yet");
		//Your Code is Here...

		acquire_spinlock(&ProcessQueues.qlock);
		struct Env *envitem= dequeue(&chan->queue); //with & or not?!
		release_spinlock(&ProcessQueues.qlock);
		if(envitem!=NULL){
			acquire_spinlock(&ProcessQueues.qlock);
			sched_insert_ready0(envitem);//with & or not?!
			release_spinlock(&ProcessQueues.qlock);
			envitem->env_status=ENV_READY;
		}

}

//====================================================
// 4) WAKEUP ALL BLOCKED PROCESSES ON A GIVEN CHANNEL:
//====================================================
// Wake up all processes sleeping on chan.
// The queues lock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes

void wakeup_all(struct Channel *chan)
{
	    //TODO: [PROJECT'24.MS1 - #12] [4] LOCKS - wakeup_all
	    //COMMENT THE FOLLOWING LINE BEFORE START CODING
	    //panic("wakeup_all is not implemented yet");
	    //Your Code is Here...
	    struct Env *env_queue_ready;

	    while(queue_size(&chan->queue) > 0){
	     	acquire_spinlock(&ProcessQueues.qlock);
	        env_queue_ready= dequeue(&chan->queue);
	        release_spinlock(&ProcessQueues.qlock);

	        if(env_queue_ready != NULL) {
	                   acquire_spinlock(&ProcessQueues.qlock);
	                   sched_insert_ready0(env_queue_ready);
	                   env_queue_ready->env_status = ENV_READY;
	                   release_spinlock(&ProcessQueues.qlock);
	               }

	    }
}

