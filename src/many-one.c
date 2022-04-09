#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>  
#include <linux/sched.h>
#include <sched.h>
#include <unistd.h>
 #include <syscall.h>
 #include <setjmp.h>
#include <sys/mman.h>

#define INVAL_INP	10
#define DEFAULT_STACK_SIZE	32768
#define THREAD_RUNNING 20
#define THREAD_TERMINATED 21
#define NO_THREAD_FOUND 22
#define GUARD_PAGE_SIZE	4096


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
	wrap_fun_info* wrapper_fun;
	int state;
	void* ret_val;

    jmp_buf *t_context;      // use to store thread specific context
    struct node* next;
} node;

typedef node* node_list;


// global tid table to store thread ids 
// of current running therads
node_list thread_list;

node scheduler_node;

long int mangle(long int p) {
    long int ret;
    asm(" mov %1, %%rax;\n"
        " xor %%fs:0x30, %%rax;"
        " rol $0x11, %%rax;"
        " mov %%rax, %0;"
        : "=r"(ret)
        : "r"(p)
        : "%rax"
    );
    return ret;
}


void scheduler() {

}
 
void init_many_one() {
    scheduler_node.t_context = (jmp_buf*)malloc(sizeof(jmp_buf));

    scheduler_node.stack_start = mmap(NULL, GUARD_PAGE_SIZE + DEFAULT_STACK_SIZE , PROT_READ|PROT_WRITE,MAP_STACK|MAP_ANONYMOUS|MAP_PRIVATE, -1 , 0);
	mprotect(scheduler_node.stack_start, GUARD_PAGE_SIZE, PROT_NONE);
    scheduler_node.stack_size = DEFAULT_STACK_SIZE;
    scheduler_node.wrapper_fun = (wrap_fun_info*)malloc(sizeof(wrap_fun_info));
    scheduler_node.wrapper_fun->fun = scheduler;
    scheduler_node.wrapper_fun->args = NULL;

    (*(scheduler_node.t_context))->__jmpbuf[6] = mangle((long int)scheduler_node.stack_start);
    (*(scheduler_node.t_context))->__jmpbuf[7] = mangle((long int)scheduler_node.wrapper_fun->fun);
}



int main() {

    return 0;
}