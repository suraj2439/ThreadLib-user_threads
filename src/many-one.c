#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>  
#include <linux/sched.h>
#include <sched.h>
#include <unistd.h>
#include <syscall.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <signal.h>

#define INVAL_INP	10
#define DEFAULT_STACK_SIZE	32768
#define THREAD_EMBRYO   19
#define THREAD_RUNNING 20
#define THREAD_TERMINATED 21
#define THREAD_RUNNABLE 22
#define NO_THREAD_FOUND 22
#define GUARD_PAGE_SIZE	4096
#define ALARM_TIME 100000  // in microseconds 


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
node_list thread_list = NULL;

node scheduler_node;
node *curr_running_proc = NULL;

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

void traverse() {
    node *nn = thread_list;
    while(nn) {
        printf("%d %d   ", nn->state, nn->stack_size);
        nn = nn->next;
    }
    printf("\n");
}

// setjump returns 0 for the first time, next time it returns value used in longjump(here 2) 
// so switch to scheduler will execute only once.
void signal_handler_alarm() {
    // printf("inside signal handler\n");    
    // disable alarm
    ualarm(0,0);

    // switch context to scheduler
    int value = sigsetjmp(*(curr_running_proc->t_context), 1);
    // printf("inside setjump %d\n", value);
    // traverse();
    if(! value) {
        siglongjmp(*(scheduler_node.t_context), 2);
    }

    return;
}

int execute_me() {
	// printf("termination start\n");
	node *nn = curr_running_proc;
	nn->state = THREAD_RUNNING;
	nn->wrapper_fun->fun(nn->wrapper_fun->args);
	nn->state = THREAD_TERMINATED;
	// printf("termination done\n");
	return 0;
}

void enable_alarm_signal() {
    sigset_t __signalList;
    sigfillset(&__signalList);
    sigaddset(&__signalList, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &__signalList, NULL);
}


void scheduler() {
    enable_alarm_signal();
    while(1) {
        // printf("hello makin runnable\n");
        if(curr_running_proc->state == THREAD_RUNNING)
            curr_running_proc->state = THREAD_RUNNABLE;
        // point next_proc to next thread of currently running process
        node *next_proc = curr_running_proc->next;
        if(! next_proc) next_proc = thread_list;

        while(next_proc->state != THREAD_RUNNABLE && next_proc->state != THREAD_EMBRYO) {
            // printf("list %d\n", next_proc->state);
            if(next_proc->next) next_proc = next_proc->next;
            else next_proc = thread_list;
        }

        curr_running_proc = next_proc;

        next_proc->state = THREAD_RUNNING;
        ualarm(ALARM_TIME, 0);
        siglongjmp(*(next_proc->t_context), 2);
    }
    
}


// insert thread_id node in beginning of list
void thread_insert(node* nn) {
	nn->next = thread_list;
	thread_list = nn;
}

 
void init_many_one() {
    
    scheduler_node.t_context = (jmp_buf*)malloc(sizeof(jmp_buf));

    scheduler_node.stack_start = mmap(NULL, GUARD_PAGE_SIZE + DEFAULT_STACK_SIZE , PROT_READ|PROT_WRITE,MAP_STACK|MAP_ANONYMOUS|MAP_PRIVATE, -1 , 0);
	mprotect(scheduler_node.stack_start, GUARD_PAGE_SIZE, PROT_NONE);
    scheduler_node.stack_size = DEFAULT_STACK_SIZE;
    scheduler_node.wrapper_fun = (wrap_fun_info*)malloc(sizeof(wrap_fun_info));
    scheduler_node.wrapper_fun->fun = scheduler;
    scheduler_node.wrapper_fun->args = NULL;

    (*(scheduler_node.t_context))->__jmpbuf[6] = mangle((long int)scheduler_node.stack_start+DEFAULT_STACK_SIZE+GUARD_PAGE_SIZE );
    (*(scheduler_node.t_context))->__jmpbuf[7] = mangle((long int)scheduler_node.wrapper_fun->fun);

    node *main_fun_node = (node *)malloc(sizeof(node));
    main_fun_node->state = THREAD_RUNNING;
    main_fun_node->tid = 0;
    main_fun_node->t_context = (jmp_buf*) malloc(sizeof(jmp_buf));
    main_fun_node->ret_val = 0;         // not required
    main_fun_node->stack_start = NULL;  // not required
    main_fun_node->stack_size = 0;      // not required
    main_fun_node->wrapper_fun = NULL;  // not required

    curr_running_proc = main_fun_node;
    thread_insert(main_fun_node);

    signal(SIGALRM, signal_handler_alarm);

    ualarm(ALARM_TIME, 0);
}

int thread_create(mThread *thread, void *attr, void *routine, void *args) {
    static thread_id id = 0;
    node *t_node = (node *)malloc(sizeof(node));
    t_node->tid = id++;
    t_node->t_context = (jmp_buf*) malloc(sizeof(jmp_buf));
    t_node->ret_val = 0;         // not required
    t_node->stack_start = mmap(NULL, GUARD_PAGE_SIZE + DEFAULT_STACK_SIZE , PROT_READ|PROT_WRITE,MAP_STACK|MAP_ANONYMOUS|MAP_PRIVATE, -1 , 0);
	mprotect(t_node->stack_start, GUARD_PAGE_SIZE, PROT_NONE);
    t_node->stack_size = DEFAULT_STACK_SIZE;      // not required

    wrap_fun_info *info = (wrap_fun_info*)malloc(sizeof(wrap_fun_info));
	info->fun = routine;
	info->args = args;
	info->thread = thread;
    t_node->wrapper_fun = info;  // not required

    (*(t_node->t_context))->__jmpbuf[6] = mangle((long int)t_node->stack_start+DEFAULT_STACK_SIZE+GUARD_PAGE_SIZE );
    (*(t_node->t_context))->__jmpbuf[7] = mangle((long int)execute_me);
    
    t_node->state = THREAD_RUNNABLE;
    thread_insert(t_node);
    
}


void f1() {
	while(1){
        sleep(1);
	    printf("inside 1st fun.\n");
    }
}

void f2() {
    while(1){
        sleep(1);
	    printf("inside 2nd fun.\n");
    }
}

void f3() {

    while(1){
	    printf("inside 3rd function\n");
        sleep(1);
    }

}

int main() {
    mThread td;
	mThread tt;

	init_many_one();

	thread_create(&td, NULL, f1, NULL);
    thread_create(&td, NULL, f2, NULL);
    thread_create(&td, NULL, f3, NULL);
	
    node* t = thread_list;
    
    while(1){
        sleep(1);
	    printf("inside main fun.\n");
    }


    return 0;
}