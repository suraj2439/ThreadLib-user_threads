#define _GNU_SOURCE
#include "many-one.h"
#include <stdio.h>
#include <stdlib.h>  
#include <unistd.h>
#include <sys/types.h>
#include "lock.h"
#include "utils.h"

extern node* curr_running_proc;
extern tid_list thread_list;
extern node scheduler_node;


void initlock(struct spinlock *lk){
	
	lk->locked = 0;
}

static inline uint xchg(volatile uint *addr, uint newval){

  uint result;
  
  asm volatile("lock; xchgl %0, %1" :
               "+m" (*addr), "=a" (result) :
               "1" (newval) :
               "cc");
  return result;
} 


void acquire(struct spinlock *lk){

    disable_alarm_signal();
	if(lk->locked && lk->tid==curr_running_proc->tid){
    	printf("gettid = %ld, and lk->tid = %ld\n", lk->tid, curr_running_proc->tid);
		perror("Error acquiring lock\n");
		exit(EXIT_FAILURE);
	}
	
	// The xchg is atomic.
	while(xchg(&lk->locked, 1) != 0)
		;
    	// printf("waiting for lock\n");

	lk->tid = curr_running_proc->tid;
}

// Release the lock.
void release(struct spinlock *lk){

	if(lk->locked==0 || lk->tid!=curr_running_proc->tid){
    	// printf("release lktid = %ld, and lk->tid = %ld\n", lk->tid, curr_running_proc->tid);

		perror("trying to release same lock twiceeee");
		exit(EXIT_FAILURE);
	}

	lk->tid = -1;

	asm volatile("movl $0, %0" : "+m" (lk->locked) : );
    enable_alarm_signal();
	ualarm(ALARM_TIME,0);
}


void sleep_lock(void *chan, struct spinlock *lk){

	// printf("sleeping \n");
    if (lk == NULL){
		perror("sleeping without lock1 \n");
		exit(EXIT_FAILURE);
	}

    // Go to sleep.
    curr_running_proc->chan = chan;
    curr_running_proc->state = THREAD_SLEEPING;

	release(lk);
	int value = sigsetjmp(*(curr_running_proc->t_context), 1);
    if(! value) {
        siglongjmp(*(scheduler_node.t_context), 2);
    }

    // Tidy up.
    curr_running_proc->chan = 0;
	acquire(lk);
}

static void wakeup_lock(void *chan){

	// printf("wake up\n");
	acquire(&thread_list.lock);

	node* n = thread_list.list;

	while(n){
		if(n->state == THREAD_SLEEPING && n->chan == chan){
			// printf("wake up\n");
			n->state = THREAD_RUNNABLE;
			break;
		}
		n = n->next;
	}

    release(&thread_list.lock);
}



void initsleeplock(struct sleeplock *lk){
	initlock(&lk->lk);
	lk->locked = 0;
	lk->tid = -2;
}


void acquiresleep(struct sleeplock *lk){
	acquire(&lk->lk);
	if (lk->locked && lk->tid == curr_running_proc->tid){
		printf("gettid = %ld, and lk->tid = %d", lk->tid, gettid());

		perror("trying to acquire same sleeplock twice\n");
		release(&lk->lk);
		exit(1);
	}

	while(lk->locked){

		// printf("in if\n");
		sleep_lock(lk, &lk->lk);
	}
	lk->locked = 1;
	lk->tid = curr_running_proc->tid;
	release(&lk->lk);
}

void releasesleep(struct sleeplock *lk){
	acquire(&lk->lk);
	if (lk->locked==0 || lk->tid!=curr_running_proc->tid){
		printf("gettid = %ld, and lk->tid = %d and lock = %d\n", lk->tid, gettid(), lk->locked);
		perror("trying to release same sleeplock twice\n");
		exit(1);
	}
	lk->locked = 0;
	lk->tid = -1;
	wakeup_lock(lk);
	release(&lk->lk);

}
