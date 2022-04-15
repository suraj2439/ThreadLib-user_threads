#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>  
#include <unistd.h>
#include <sys/types.h>
#include "many-many.h"
#include "lock.h"
#include "utils.h"

extern node** curr_running_proc_array;
extern thread_id main_ktid;

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

//TODO: change this, maybe make main as a thread
thread_id get_calling_ktid(){

    thread_id curr_calling_ktid;

    if(gettid() == main_ktid)
        curr_calling_ktid = main_ktid;
    else
        curr_calling_ktid = curr_running_proc_array[get_curr_kthread_index()]->tid;
    return curr_calling_ktid;
}


void acquire(struct spinlock *lk){
    // return;
    disable_alarm_signal();
    // if(!curr_running_proc_array[get_curr_kthread_index()])
    //     printf("hi\n\n");
    // get_curr_kthread_index();
    thread_id curr_calling_ktid = get_calling_ktid();

	if(lk->locked && lk->tid==curr_calling_ktid){
    	// printf("gettid = %ld, and lk->tid = %ld\n", lk->tid, curr_running_proc_array->tid);
		perror("Error acquiring lock\n");
		exit(EXIT_FAILURE);
	}
	
	// The xchg is atomic.
	while(xchg(&lk->locked, 1) != 0)
    	;

	lk->tid = curr_calling_ktid;
}

// Release the lock.
void release(struct spinlock *lk){
    // return;
    
    thread_id curr_calling_ktid = get_calling_ktid();

	if(lk->locked==0 || (lk->tid != curr_calling_ktid)){
    	// printf("release lktid = %ld, and lk->tid = %ld\n", lk->tid, curr_running_proc_array->tid);

		perror("trying to release same lock twiceeee");
		exit(EXIT_FAILURE);
	}

	lk->tid = -1;

	asm volatile("movl $0, %0" : "+m" (lk->locked) : );
    enable_alarm_signal();
}
