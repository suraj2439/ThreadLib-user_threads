#define _GNU_SOURCE
#include "many-one.h"
#include <stdio.h>
#include <stdlib.h>  
#include <unistd.h>
#include <sys/types.h>
#include "lock.h"
#include "utils.h"

extern node* curr_running_proc;

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
}
