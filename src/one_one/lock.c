#define _GNU_SOURCE
#include "mthread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include "lock.h"

void initlock(struct spinlock *lk){

	lk->locked = 0;
}

static inline uint xchg(volatile uint *addr, uint newval){

	uint result;

	asm volatile("lock; xchgl %0, %1"
				 : "+m"(*addr), "=a"(result)
				 : "1"(newval)
				 : "cc");
	return result;
}

void acquire(struct spinlock *lk){

	// printf("gettid = %ld, and lk->tid = %ld", lk->tid, lk->tid);
	if (lk->locked && lk->tid == gettid())
	{
		perror("trying to acquire same lock twice");
		exit(1);
	}

	// The xchg is atomic.
	while (xchg(&lk->locked, 1) != 0)
		;

	lk->tid = gettid();
}

// Release the lock.
void release(struct spinlock *lk){

	if (!(lk->locked && lk->tid == gettid()))
	{
		perror("trying to acquire same lock twice");
		exit(1);
	}

	lk->tid = -1;

	asm volatile("movl $0, %0"
				 : "+m"(lk->locked)
				 :);
}

void initsleeplock(struct sleeplock *lk){
	initlock(&lk->lk);
	lk->locked = 0;
	lk->tid = -1;
}


void acquiresleep(struct sleeplock *lk){
	acquire(&lk->lk);
	if (lk->locked && lk->tid == gettid()){
		printf("gettid = %ld, and lk->tid = %d", lk->tid, gettid());

		perror("trying to acquire same sleeplock twice\n");
		release(&lk->lk);
		exit(1);
	}
	
	while(lk->locked==1){
		release(&lk->lk);
		syscall(SYS_futex, lk->locked, FUTEX_WAIT, 1, NULL, NULL, 0);
		acquire(&lk->lk);

		// printf("inside while\n");
	}
	lk->locked = 1;
	lk->tid = gettid();
	release(&lk->lk);
}

void releasesleep(struct sleeplock *lk){
	acquire(&lk->lk);
	if (!(lk->locked && lk->tid == gettid())){
		printf("gettid = %ld, and lk->tid = %d and lock = %d\n", lk->tid, gettid(), lk->locked);
		perror("trying to release same sleeplock twice\n");
		exit(1);
	}
	lk->locked = 0;
	lk->tid = -1;
    syscall(SYS_futex, lk->locked, FUTEX_WAKE, 1, NULL, NULL, 0);
	release(&lk->lk);

}
