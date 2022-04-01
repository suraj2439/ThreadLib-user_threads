#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>  
#include <linux/sched.h>
#include <sched.h>
#include <unistd.h>

#define INVAL_INP	10

typedef unsigned long int thread_id;
typedef unsigned long int mThread;

typedef struct node {
	thread_id tid;
	struct node* next;
} node;

typedef node* tid_list;

typedef struct wrap_fun_info {
	void (*fun)(void *);
	void *args;
	mThread *thread;
} wrap_fun_info;

// global tid table to store thread ids 
// of current running therads
tid_list tid_table;


// insert thread_id node in beginning of list
int tid_insert(thread_id tid) {
	node* nn = (node*)malloc(sizeof(node));
	nn->tid = tid;
	nn->next = NULL;

	node *tmp = tid_table;
	tid_table = nn;
	nn->next = tmp;
}

void init_threading() {
	tid_table = NULL;
}


int execute_me(void *execute_arg_struct) {
	wrap_fun_info *info = (wrap_fun_info*)execute_arg_struct;
	info->fun(info->args);
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
	*thread = clone(execute_me, stack, CLONE_FLAGS, (void *)info);
	tid_insert(*thread);
}

void myFun() {
	printf("thread fun: waiting for 1 sec.\n");
	sleep(1);
	printf("hello inside my function.\n");
}

int main() {
	mThread td;
	thread_create(&td, NULL, myFun, NULL);
	printf("main fun waiting for 3 sec.\n");
	sleep(3);
	printf("%ld\n", td);
	return 0;
}
