#include <stdio.h>

#define INVAL_INP	10

typedef unsigned long int thread_id;
typedef unsigned long int mThread;

typedef struct node {
	thread_id tid;
	struct node* next;
} node;

typedef node* tid_list;

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


int thread_create(mThread *thread, void *attr, void *routine, void *args) {
	if(! thread || ! routine) return INVAL_INP;
	
	CLONE_FLAGS = CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD |CLONE_SYSVSEM|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID;



}



