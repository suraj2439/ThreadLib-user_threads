#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>  
#include <unistd.h>
#include <sys/types.h>
#include "many-many.h"
#include "lock.h"
#include "utils.h"

extern node** curr_running_proc_array;
extern node* scheduler_node_array;
extern thread_id main_ktid;
extern node_list thread_list;

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

    thread_id curr_calling_ktid=-10;

    if(gettid() == main_ktid)
        curr_calling_ktid = main_ktid;
    else{
        if(curr_running_proc_array[get_curr_kthread_index()])
            curr_calling_ktid = curr_running_proc_array[get_curr_kthread_index()]->tid;
    }
    return curr_calling_ktid;
}


void acquire(struct spinlock *lk){
    // return;
    disable_alarm_signal();
    // if(!curr_running_proc_array[get_curr_kthread_index()])
        // printf("hi\n\n");
    // get_curr_kthread_index();
    thread_id curr_calling_ktid = get_calling_ktid();

	if(lk->locked && lk->tid==curr_calling_ktid){
    	printf("gettid = %ld, and lk->tid = %ld\n", lk->tid, curr_calling_ktid );
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

	// printf("s2 \n");
	if(lk->locked==0 || (lk->tid != curr_calling_ktid)){
    	// printf("release lktid = %ld, and lk->tid = %ld\n", lk->tid, curr_running_proc_array->tid);

		perror("trying to release same lock twiceeee");
		exit(EXIT_FAILURE);
	}

	lk->tid = -1;

	// printf("s3 \n");
	asm volatile("movl $0, %0" : "+m" (lk->locked) : );
    enable_alarm_signal();
	// printf("s4 \n");
}


void sleep_lock(void *chan, struct spinlock *lk){

	printf("sleeping \n");
    if (lk == NULL){
		perror("sleeping without lock1 \n");
		exit(EXIT_FAILURE);
	}

    int index = get_curr_kthread_index();
    // Go to sleep.
    node* curr_running_proc = curr_running_proc_array[index];
    curr_running_proc->chan = chan;
    curr_running_proc->state = THREAD_SLEEPING;

	release(lk);
	int value = sigsetjmp(*(curr_running_proc->t_context), 1);
    if(! value) {
        siglongjmp(*(scheduler_node_array[index] .t_context), 2);
    }
	acquire(lk);

    index = get_curr_kthread_index();
    curr_running_proc = curr_running_proc_array[index];
    // Tidy up.
    curr_running_proc->chan = 0;
}

static void wakeup_lock(void *chan){

	printf("wake up\n");
	acquire(&thread_list.lock);

	node* n = thread_list.list;

	while(n){
        printf("hi\n");
		if(n->state == THREAD_SLEEPING && n->chan == chan){
			printf("wake up sleeping %ld\n", n->tid);
			n->state = THREAD_RUNNABLE;
			break;
		}
		n = n->next;
	}

    printf("wakeup exit\n");
    release(&thread_list.lock);
    printf("wakeup2 exit\n");
}



void initsleeplock(struct sleeplock *lk){
	initlock(&lk->lk);
	lk->locked = 0;
	lk->tid = -2;
}


void acquiresleep(struct sleeplock *lk){
	acquire(&lk->lk);
    
    thread_id curr_calling_ktid = get_calling_ktid();

	if (lk->locked && lk->tid == curr_calling_ktid){
		printf("gettid = %ld, and lk->tid = %d", lk->tid, gettid());

		perror("trying to acquire same sleeplock twice\n");
		release(&lk->lk);
		exit(1);
	}

	while(lk->locked){

		printf("in if\n");
		sleep_lock(lk, &lk->lk);
	}
	lk->locked = 1;
	lk->tid = curr_calling_ktid;
	release(&lk->lk);
}

void releasesleep(struct sleeplock *lk){
	acquire(&lk->lk);
    thread_id curr_calling_ktid = get_calling_ktid();

	if (lk->locked==0 || lk->tid!=curr_calling_ktid){
		printf("gettid = %ld, and lk->tid = %d and lock = %d\n", lk->tid, gettid(), lk->locked);
		perror("trying to release same sleeplock twice\n");
		exit(1);
	}
	lk->locked = 0;
	lk->tid = -1;
	wakeup_lock(lk);
	release(&lk->lk);

}
