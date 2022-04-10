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
#define THREAD_RUNNING 20
#define THREAD_TERMINATED 21
#define THREAD_RUNNABLE 22
#define NO_THREAD_FOUND 22
#define GUARD_PAGE_SIZE	4096
#define ALARM_TIME 100000  // in microseconds 
#define NO_OF_KTHREADS 4 
#define K_ALARM_TIME    (ALARM_TIME / NO_OF_KTHREADS)
#define CLONE_FLAGS     CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD |CLONE_SYSVSEM|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID

typedef unsigned long int thread_id;
typedef unsigned long int mThread;


typedef struct wrap_fun_info {
	void (*fun)(void *);
	void *args;
	mThread *thread;
} wrap_fun_info;

typedef struct node {
	thread_id tid;
    thread_id kernel_tid;
    int kthread_index;      // use to locate the particular kernel thread in which the node belong
	int stack_size;
	void *stack_start;
	wrap_fun_info* wrapper_fun;
	int state;
	void* ret_val;
    jmp_buf *t_context;      // use to store thread specific context
    struct node* next;
} node;

typedef node* node_list;
node_list* thread_list_array;
int alarm_index = -1;

void scheduler();


// global tid table to store thread ids 
// of current running therads
node_list thread_list = NULL;

node* scheduler_node_array;
node** curr_running_proc_array;

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


int get_curr_kthread_index() {
    thread_id curr_ktid = gettid();
    for(int i = 0; i < NO_OF_KTHREADS; i++) {
        if(curr_ktid == thread_list_array[i]->kernel_tid)
            return i;
    }
    return -1;  // ERROR
}


// setjump returns 0 for the first time, next time it returns value used in longjump(here 2) 
// so switch to scheduler will execute only once.
void signal_handler_alarm() {
    // printf("inside signal handler\n");    
    // disable alarm
    ualarm(0,0);
    alarm_index = (alarm_index + 1) % NO_OF_KTHREADS;
	syscall(SYS_tgkill, getpid(), thread_list_array[alarm_index]->kernel_tid, SIGVTALRM);
    
    return;
}

// setjump returns 0 for the first time, next time it returns value used in longjump(here 2) 
// so switch to scheduler will execute only once.
void signal_handler_vtalarm() {
    // printf("inside signal handler\n");    
    // disable alarm
    ualarm(0,0);

    // switch context to scheduler
    int value = sigsetjmp(*(curr_running_proc_array[get_curr_kthread_index()]->t_context), 1);
    if(! value) {
        siglongjmp(scheduler_node_array[get_curr_kthread_index()].t_context, 2);
    }
    return;
}

int execute_me_oo(void *new_node) {
	node *nn = (node*)new_node;
	nn->state = THREAD_RUNNING;
	nn->wrapper_fun->fun(nn->wrapper_fun->args);
	nn->state = THREAD_TERMINATED;
	printf("termination done\n");
	siglongjmp(scheduler_node_array[nn->kthread_index].t_context, 2);
}

int execute_me_mo() {
    // thread_id curr_ktid = gettid();
    // int i;
    // for(i = 0; i < NO_OF_KTHREADS; i++) {
    //     if(curr_ktid == thread_list_array[i]->kernel_tid)
    //         break;
    // }

    node *nn = thread_list_array[get_curr_kthread_index()];
    while(nn->state != THREAD_RUNNING)
        nn = nn->next;
    
	// printf("inside execute me\n");
	nn->wrapper_fun->fun(nn->wrapper_fun->args);
    printf("execute me end\n");
	nn->state = THREAD_TERMINATED;
	// printf("termination done\n");
    //TODO: IMP: don't call scheduler() directly,instead use long jump
    // siglongjmp(*(scheduler_node.t_context), 2);
    siglongjmp(scheduler_node_array[nn->kthread_index].t_context, 2);
	return 0;
}


// insert thread_id node in beginning of list
void thread_insert(int thread_index, node* nn) {
	nn->next = thread_list_array[thread_index];
	thread_list_array[thread_index] = nn;
}

void scheduler() {
    while(1) {
        int index = get_curr_kthread_index();
        node* curr_running_proc = curr_running_proc_array[index];
        // printf("inside scheduler\n");
        if(curr_running_proc->state == THREAD_RUNNING)
            curr_running_proc->state = THREAD_RUNNABLE;
            
        // point next_proc to next thread of currently running process
        node *next_proc = curr_running_proc->next;
        if(! next_proc) next_proc = thread_list;

        while(next_proc->state != THREAD_RUNNABLE) {
            if(next_proc->next) next_proc = next_proc->next;
            else next_proc = thread_list;
        }

        curr_running_proc_array[index] = next_proc;

        next_proc->state = THREAD_RUNNING;
        enable_alarm_signal();
        ualarm(ALARM_TIME, 0);
        siglongjmp(*(next_proc->t_context), 2);
    }
    
}

void init_many_many() {
    scheduler_node_array = (node*)malloc(sizeof(node)*NO_OF_KTHREADS);
    for(int i=0; i<NO_OF_KTHREADS; i++){
        
        scheduler_node_array[i].t_context = (jmp_buf*)malloc(sizeof(jmp_buf));

        scheduler_node_array[i].stack_start = mmap(NULL, GUARD_PAGE_SIZE + DEFAULT_STACK_SIZE , PROT_READ|PROT_WRITE,MAP_STACK|MAP_ANONYMOUS|MAP_PRIVATE, -1 , 0);
        mprotect(scheduler_node_array[i].stack_start, GUARD_PAGE_SIZE, PROT_NONE);
        scheduler_node_array[i].stack_size = DEFAULT_STACK_SIZE;
        scheduler_node_array[i].wrapper_fun = (wrap_fun_info*)malloc(sizeof(wrap_fun_info));
        scheduler_node_array[i].wrapper_fun->fun = scheduler;
        scheduler_node_array[i].wrapper_fun->args = NULL;

        (*(scheduler_node_array[i].t_context))->__jmpbuf[6] = mangle((long int)scheduler_node_array[i].stack_start+DEFAULT_STACK_SIZE+GUARD_PAGE_SIZE );
        (*(scheduler_node_array[i].t_context))->__jmpbuf[7] = mangle((long int)scheduler_node_array[i].wrapper_fun->fun);
    }

    thread_list_array = (node_list*)malloc(sizeof(node_list)*NO_OF_KTHREADS);
    for(int i=0; i<NO_OF_KTHREADS; i++)
        thread_list_array[i] = NULL;

    // node *main_fun_node = (node *)malloc(sizeof(node));
    // main_fun_node->state = THREAD_RUNNING;
    // main_fun_node->tid = 0;
    // main_fun_node->t_context = (jmp_buf*) malloc(sizeof(jmp_buf));
    // main_fun_node->ret_val = 0;         // not required
    // main_fun_node->stack_start = NULL;  // not required
    // main_fun_node->stack_size = 0;      // not required
    // main_fun_node->wrapper_fun = NULL;  // not required

    curr_running_proc_array = (node**)malloc(sizeof(node*) * NO_OF_KTHREADS);
    for(int i=0; i<NO_OF_KTHREADS; i++)
        curr_running_proc_array[i] = NULL;

    // curr_running_proc = main_fun_node;
    // thread_insert(0, main_fun_node);

    // signal(SIGALRM, signal_handler_alarm);

    ualarm(K_ALARM_TIME, 0);
}


int thread_create(mThread *thread, void *attr, void *routine, void *args) {
    if(! thread || ! routine) return INVAL_INP;
    static thread_id id = 0;
    node *t_node = (node *)malloc(sizeof(node));
    t_node->tid = id++;
    *thread = t_node->tid;
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
    

    if(t_node->tid < NO_OF_KTHREADS) {
        curr_running_proc_array[t_node->tid] = t_node;
        t_node->kernel_tid = clone(execute_me_oo, t_node->stack_start + DEFAULT_STACK_SIZE + GUARD_PAGE_SIZE, CLONE_FLAGS, (void *)t_node);	
        return 0;
    }
    else {

        (*(t_node->t_context))->__jmpbuf[6] = mangle((long int)t_node->stack_start+DEFAULT_STACK_SIZE+GUARD_PAGE_SIZE );
        (*(t_node->t_context))->__jmpbuf[7] = mangle((long int)execute_me_mo);
        
        t_node->state = THREAD_RUNNABLE;
        thread_insert(t_node->tid % NO_OF_KTHREADS, t_node);    // TODO modifye this scheme
    }
    return 0;
    
}