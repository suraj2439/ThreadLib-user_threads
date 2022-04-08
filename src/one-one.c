#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>  
#include <linux/sched.h>
#include <sched.h>
#include <unistd.h>

#define INVAL_INP	10
#define DEFAULT_STACK_SIZE	4096
#define THREAD_RUNNING 20
#define THREAD_TERMINATED 21
#define NO_THREAD_FOUND 22

typedef unsigned long int thread_id;
typedef unsigned long int mThread;


typedef struct wrap_fun_info {
	void (*fun)(void *);
	void *args;
	mThread *thread;
} wrap_fun_info;

typedef struct node {
	thread_id tid;
	int stack_size;
	void *stack_start;
	struct node* next;
	wrap_fun_info* wrapper_fun;
	int state;
	void* ret_val;
} node;

typedef node* tid_list;


// global tid table to store thread ids 
// of current running therads
tid_list tid_table;
int lock =0;


// insert thread_id node in beginning of list
int tid_insert(node* nn, thread_id tid, int stack_size, void *stack_start) {
	
	nn->tid = tid;
	nn->stack_size = stack_size;
	nn->stack_start = stack_start;
	nn->next = NULL;
	

	lock=1;
	nn->next = tid_table;
	tid_table = nn;

	node *tmp = tid_table;

	lock = 0;
}
/*
void tid_insert(tid_list *l, thread_id tid, int stack_size, void *stack_start) {
    node *p = *l;
    node *nn = (node *)malloc(sizeof(node));
    nn->tid = tid;
    nn->stack_size = stack_size;
    nn->stack_start = stack_start;
    nn->next = NULL;

    *l = nn;
    nn->next = p;
    return;
}*/

void init_threading() {
	tid_table = NULL;
}


int execute_me(void *new_node) {
	node *nn = (node*)new_node;
	nn->state = THREAD_RUNNING;
	nn->wrapper_fun->fun(nn->wrapper_fun->args);
	nn->state = THREAD_TERMINATED;
	printf("termination done\n");
	return 0;
}

int thread_join(mThread tid, void **retval){
	if(!retval)
		return INVAL_INP;

	node* n = tid_table;

	while(n && n->tid!=tid)
		n = n->next;

	if(!n)
		return NO_THREAD_FOUND;

	while(n->state!=THREAD_TERMINATED)
		printf("%d %ld\n", n->state, n->tid);

	*retval = n->ret_val;
	return 0;
	
}


int thread_create(mThread *thread, void *attr, void *routine, void *args) {
	if(! thread || ! routine) return INVAL_INP;
	
	unsigned long int CLONE_FLAGS = CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD |CLONE_SYSVSEM|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID;
	wrap_fun_info *info = (wrap_fun_info*)malloc(sizeof(wrap_fun_info));
	info->fun = routine;
	info->args = args;
	info->thread = thread;
	void *stack = (void *)malloc(sizeof(int)*4000);
	
	node *new_node = (node*)malloc(sizeof(node));
	new_node->wrapper_fun = info;
	
	*thread = clone(execute_me, stack, CLONE_FLAGS, (void *)new_node);	
	tid_insert(new_node,*thread, DEFAULT_STACK_SIZE, stack);
	printf("hello %ld\n", *thread);
}

void myFun() {
	//sleep(1);
	printf("inside 1st fun.\n");
}

void myF() {
	sleep(3);
	printf("inside 2nd fun\n");
}

int main() {
	mThread td;
	mThread tt;
	init_threading();
	thread_create(&td, NULL, myFun, NULL);
	sleep(3);
	thread_create(&tt, NULL, myF, NULL);
	printf("bfr join.\n");
	void **a;
	thread_join(td, a);
	printf("maftr join\n");
	sleep(1);
	printf("bfr join1.\n");
	thread_join(tt, a);
	printf("maftr join2\n");
	//printf("%ld\n", tid_table->next->tid);

		while(lock==1)
		sleep(1);
	
	lock=1;

	node *tmp = tid_table;
	while(tmp) {
		printf("abcd tid %ld\n", tmp->tid);
		printf("stack size %d\nabcd\n", tmp->stack_size);
		tmp = tmp->next;
	}
	lock = 0;
	return 0;
	}