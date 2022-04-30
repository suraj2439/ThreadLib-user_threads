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
spinlock test;

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

void cleanup(thread_id tid) {
	// printf("given tid %ld\n", tid);
	acquire(&tid_table.lock);
	node *prev = NULL, *curr = tid_table.list;
	while(curr && curr->tid != tid) {
		prev = curr;
		curr = curr->next;
	}
	if(! curr) {
		printf("DEBUG: cleanup node not found\n");
		release(&tid_table.lock);
		return;
	}
	if(curr == tid_table.list) {
		// head node
		node *tmp = tid_table.list;
		tid_table.list = tid_table.list->next;
		release(&tid_table.lock);
		if(tmp->stack_size)		// stack size not 0 means stack is not given by user
			munmap(tmp, DEFAULT_STACK_SIZE + GUARD_PAGE_SIZE);
		free(tmp->wrapper_fun);
		// free(tmp);
		return;
	}

	prev->next = curr->next;
	release(&tid_table.lock);
	if(curr->stack_size)		// stack size not 0 means stack is not given by user
		munmap(curr->stack_start, DEFAULT_STACK_SIZE + GUARD_PAGE_SIZE);
	free(curr->wrapper_fun);
	// free(curr);
	return;
}

void cleanupAll() {
	while(tid_table.list) 
		cleanup(tid_table.list->tid);
	printf("Cleaned all theread stacks\n");
}

void sigusr_signal_handler(){
	thread_exit(NULL);
	return;
}

void sigusr2_signal_handler() {
	printf("SIGUSR2 interrupt received.\n");
}

void init_threading() {
	signal(SIGUSR1, sigusr_signal_handler);
    signal(SIGUSR2, sigusr2_signal_handler);
	if(atexit(cleanupAll)) printf("atexit registration failed\n");
	acquire(&tid_table.lock);
	tid_table.list = NULL;
	release(&tid_table.lock);
}

void init_mThread_attr(mThread_attr **attr) {
	*attr = (mThread_attr*)malloc(sizeof(mThread_attr));
	(*attr)->guardSize = GUARD_PAGE_SIZE;
	(*attr)->stack = NULL;
	(*attr)->stackSize = DEFAULT_STACK_SIZE;
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
	// if(!retval)
	// 	return INVAL_INP;
		
	acquire(&tid_table.lock);

	node* n = tid_table.list;

	while(n && n->tid!=tid)
		n = n->next;
	release(&tid_table.lock);

	if(!n)
		return NO_THREAD_FOUND;

	while(n->state!=THREAD_TERMINATED)
		;

	if(retval)
		*retval = n->ret_val;
	//acquire
	cleanup(tid);
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
	
	if(signal == SIGTERM) {
		int val = syscall(SYS_tgkill, process_id, thread, SIGUSR1);
		if(val == -1)
			return INVALID_SIGNAL;
		return 0;
	}
	int val = syscall(SYS_tgkill, process_id, thread, signal);
	if(val == -1)
		return INVALID_SIGNAL;
	return 0;
}

int thread_create(mThread *thread, const mThread_attr *attr, void *routine, void *args) {
	if(! thread || ! routine) return INVAL_INP;

	int guardSize, stackSize;
	void *stack;
	if(attr) {
		// if guardSize not equal to zero
		if(guardSize) guardSize = attr->guardSize;
		else guardSize = GUARD_PAGE_SIZE;

		// if stack size is given by user, use that stack size else use default
		if(attr->stackSize && !attr->stack) stackSize = attr->stackSize;
		else stackSize = DEFAULT_STACK_SIZE;

		// if user given users stack use that stack, else mmap new stack
		if(attr->stack) {
			stack = attr->stack;
            stackSize = 0; // indicating current stack user stack
		}
		else {
			stack = mmap(NULL, guardSize + stackSize , PROT_READ|PROT_WRITE,MAP_STACK|MAP_ANONYMOUS|MAP_PRIVATE, -1 , 0);
			if(stack == MAP_FAILED)
				return MMAP_FAILED;
			mprotect(stack, guardSize, PROT_NONE);
		}
	}
	else {
		guardSize = GUARD_PAGE_SIZE;
		stackSize = DEFAULT_STACK_SIZE;
		stack = mmap(NULL, guardSize + stackSize , PROT_READ|PROT_WRITE,MAP_STACK|MAP_ANONYMOUS|MAP_PRIVATE, -1 , 0);
		if(stack == MAP_FAILED)
			return MMAP_FAILED;
		mprotect(stack, guardSize, PROT_NONE);
	}

	static int is_init_done = 0;
	if(! is_init_done){
		init_threading();
		is_init_done = 1;
	}
	
	unsigned long int CLONE_FLAGS = CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD |CLONE_SYSVSEM|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID;
	wrap_fun_info *info = (wrap_fun_info*)malloc(sizeof(wrap_fun_info));
	info->fun = routine;
	info->args = args;
	info->thread = thread;

	node *new_node = (node*)malloc(sizeof(node));
	new_node->wrapper_fun = info;
	
	// printf("stacksize : %d\n guardsize %d\n", stackSize,guardSize);
	*thread = clone(execute_me, stack + stackSize + guardSize, CLONE_FLAGS, (void *)new_node);
	if(*thread == -1) 
		return CLONE_FAILED;
	tid_insert(new_node,*thread, stackSize, stack);

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

void myFun() {
	int c1;
	while(1){
		acquire(&test);
		printf("inside 1st fun.\n");
		sleep(1);
		c++;
		c1++;
		release(&test);
		if(c1>1000000)
			break;
	}
	printf("inside 2nd fun c1  = %d\n", c1);

}

void myF() {
	// sleep(3);
	int c2 = 0;
	while(1){
		acquire(&test);
		printf("inside 2nd fun\n");
		sleep(1);
		c++;
		c2++;
		release(&test);
		if(c2>1000000)
			break;
	}
	printf("inside 2nd fun c2  = %d\n", c2);

}

void signal_handler() {
	printf("in sig handler %d\n", gettid());
}


// int main() {
// 	mThread td;
// 	mThread tt;

// 	mThread_attr *attr;;
// 	init_mThread_attr(&attr);
// 	// signal(SIGALRM, signal_handler);
// 	int a = 4;
// 	attr->stackSize = 4000;
// 	thread_create(&td, attr, myFun, (void*)&a);
// 	thread_create(&tt, NULL, myF, NULL);
// 	thread_kill(td, SIGTERM);
// 	printf("sending signal to %ld\n", td);
// 	// thread_kill(td, SIGALRM);
// 	// thread_kill(td, 12);	
// 	printf("stack size %d\n", tid_table.list->stack_size);
// 	while(1) {
// 		printf("in main \n");
// 		sleep(1);
// 	}
// 		printf("in main \n");
// 	sleep(6);
// 		printf("in main \n");

	
// 	// thread_create(&tt, NULL, myF, NULL);
// 	// printf("bfr join.\n");
// 	// void **a;
// 	// thread_join(td, a);
// 	// printf("maftr join\n");
// 	// sleep(1);
// 	// printf("bfr join1.\n");
// 	// thread_join(tt, a);
// 	// printf("maftr join2\n");
// 	// //printf("%ld\n", tid_table->next->tid);

// 	// node *tmp = tid_table;
// 	// while(tmp) {
// 	// 	printf("stack size %d\nabcd\n", tmp->stack_size);
// 	// 	tmp = tmp->next;
// 	// }
// 	return 0;
// }

