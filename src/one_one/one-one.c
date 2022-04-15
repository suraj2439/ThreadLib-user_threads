// TODO add pid in the node to use same library instance for multiple processes.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>  
#include <linux/sched.h>
#include <sched.h>
#include <unistd.h>
#include <syscall.h>
#include <sys/mman.h>
#include <signal.h>
#include "one-one.h"
#include "lock.h"

// global tid table to store thread ids 
// of current running therads
tid_list tid_table;

int c;
sleeplock test;

// insert thread_id node in beginning of list
int tid_insert(node* nn, thread_id tid, int stack_size, void *stack_start) {
	
	nn->tid = tid;
	nn->stack_size = stack_size;
	nn->stack_start = stack_start;
	nn->next = NULL;

	acquire(&tid_table.lock);
	nn->next = tid_table.list;
	tid_table.list = nn;
	release(&tid_table.lock);

}

void init_threading() {
	acquire(&tid_table.lock);
	tid_table.list = NULL;
	release(&tid_table.lock);

}

int execute_me(void *new_node) {
	node *nn = (node*)new_node;
	nn->state = THREAD_RUNNING;
	nn->wrapper_fun->fun(nn->wrapper_fun->args);
	// nn->state = THREAD_TERMINATED;
    thread_exit(NULL);
	printf("termination done\n");
	return 0;
}

int thread_join(mThread tid, void **retval) {	// TODO **retval with NULL value
	if(!retval)
		return INVAL_INP;
		
	acquire(&tid_table.lock);

	node* n = tid_table.list;

	while(n && n->tid!=tid)
		n = n->next;
	release(&tid_table.lock);

	if(!n)
		return NO_THREAD_FOUND;

	while(n->state!=THREAD_TERMINATED)
		;

	*retval = n->ret_val;
	//acquire
	//remove thread from queue
	//release
	return 0;
}

void thread_exit(void *retval) {
	thread_id curr_tid = (thread_id)gettid();
	
	acquire(&tid_table.lock);

	node *n = tid_table.list;

	while(n && n->tid != curr_tid)
		n = n->next;
	release(&tid_table.lock);
	
	if(!n ) return;

	// free(n->wrapper_fun); 	TODO
	// free(n->stack_start);
	n->ret_val = retval;
	n->state = THREAD_TERMINATED;
	syscall(SYS_exit, EXIT_SUCCESS);
}

int thread_kill(mThread thread, int signal) {
	if(!signal ) return INVALID_SIGNAL;
	int process_id = getpid();
	int val = syscall(SYS_tgkill, process_id, thread, signal);
	if(val == -1)
		return INVALID_SIGNAL;
	return 0;
}

int thread_create(mThread *thread, void *attr, void *routine, void *args) {

	static int is_init_done = 0;
	if(! is_init_done){
		init_threading();
		is_init_done = 1;
	}

	if(! thread || ! routine) return INVAL_INP;
	
	unsigned long int CLONE_FLAGS = CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD |CLONE_SYSVSEM|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID;
	wrap_fun_info *info = (wrap_fun_info*)malloc(sizeof(wrap_fun_info));
	info->fun = routine;
	info->args = args;
	info->thread = thread;
	
	void *stack = mmap(NULL, GUARD_PAGE_SIZE + DEFAULT_STACK_SIZE , PROT_READ|PROT_WRITE,MAP_STACK|MAP_ANONYMOUS|MAP_PRIVATE, -1 , 0);
	// printf("%p\n", stack);
	if(stack == MAP_FAILED)
		return MMAP_FAILED;
	mprotect(stack, GUARD_PAGE_SIZE, PROT_NONE);

	node *new_node = (node*)malloc(sizeof(node));
	new_node->wrapper_fun = info;
	
	*thread = clone(execute_me, stack + DEFAULT_STACK_SIZE + GUARD_PAGE_SIZE, CLONE_FLAGS, (void *)new_node);
	if(*thread == -1) 
		return CLONE_FAILED;
	tid_insert(new_node,*thread, DEFAULT_STACK_SIZE, stack);

	return 0;
}

void init_thread_lock(struct spinlock *lk){
    initlock(lk);
}

void thread_lock(struct spinlock *lk){
    acquire(lk);
}

void thread_unlock(struct spinlock *lk){
    release(lk);
}

void init_mutex_thread_lock(struct sleeplock *lk){
    initsleeplock(lk);
}

void thread_mutex_lock(struct sleeplock *lk){
    acquiresleep(lk);
}

void thread_mutex_unlock(struct sleeplock *lk){
    releasesleep(lk);
}



void myFun() {
	printf("inside 1st fun.\n");
	// sleep(3);
	// printf("above sleep\n");
	// void *t;
	// thread_exit(t);
	// printf("below sleep\n");
	
	int c1;
	while(1){
		acquiresleep(&test);
		c++;
		c1++;
		releasesleep(&test);
		if(c1>1000000)
			break;
	}
	printf("inside 2nd fun c1  = %d\n", c1);

}

void myF() {
	// sleep(3);
	printf("inside 2nd fun\n");
	int c2 = 0;
	while(1){
		acquiresleep(&test);
		c++;
		c2++;
		releasesleep(&test);
		if(c2>1000000)
			break;
	}
	printf("inside 2nd fun c2  = %d\n", c2);

}

void signal_handler() {
	printf("in sig handler %d\n", gettid());
}


int main() {
	mThread td;
	mThread tt;
	// init_threading();
	signal(SIGALRM, signal_handler);
	// int a = 4;
	initsleeplock(&test);
	thread_create(&td, NULL, myFun, NULL);
	thread_create(&tt, NULL, myF, NULL);
	printf("t1 = %ld, t2 = %ld\n", td, tt);
	// thread_kill(td, SIGALRM);
	// thread_kill(td, 12);	

	void **a;
	thread_join(tt, a);
	thread_join(td, a);
	printf("total c  = %d\n", c);
	while(1) {
		printf("in main \n");
		sleep(1);
	}
		printf("in main \n");
	sleep(6);
		printf("in main \n");

	
	// thread_create(&tt, NULL, myF, NULL);
	// printf("bfr join.\n");
	// thread_join(td, a);
	// printf("maftr join\n");
	// sleep(1);
	// printf("bfr join1.\n");
	// thread_join(tt, a);
	// thread_join(td, a);
	// printf("total c  = %d\n", c);
	// printf("maftr join2\n");
	// //printf("%ld\n", tid_table->next->tid);

	// node *tmp = tid_table;
	// while(tmp) {
	// 	printf("stack size %d\nabcd\n", tmp->stack_size);
	// 	tmp = tmp->next;
	// }
	return 0;
}

