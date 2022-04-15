#define _GNU_SOURCE
#include "one-one.h"
#include <stdio.h>
#include <stdlib.h>  
#include <unistd.h>
#include <sys/types.h>
#include "lock.h"

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
	// printf("gettid = %ld, and lk->tid = %ld", lk->tid, lk->tid);
	if(lk->locked && lk->tid==gettid()){
		perror("trying to acquire same lock twice");
		exit(1);
	}
	
	// The xchg is atomic.
	while(xchg(&lk->locked, 1) != 0)
    	;

	lk->tid = gettid();
}

// Release the lock.
void release(struct spinlock *lk){

	if(!(lk->locked && lk->tid==gettid())){
		perror("trying to acquire same lock twice");
		exit(1);
	}

	lk->tid = -1;

	asm volatile("movl $0, %0" : "+m" (lk->locked) : );
}
