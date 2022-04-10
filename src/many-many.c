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
#define NO_THREAD_FOUND 23
#define GUARD_PAGE_SIZE	4096
#define ALARM_TIME 100000  // in microseconds 
#define NO_OF_KTHREADS 2
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
    node *nn;

    for(int i=0; i<NO_OF_KTHREADS; i++){
        nn = thread_list_array[i];
        printf("printing %d list\n", i);

        while(nn) {
            printf("%d %d   ", nn->state, nn->stack_size);
            nn = nn->next;
        }
        printf("\n");
    }
}

void enable_alarm_signal() {
    sigset_t __signalList;
    sigfillset(&__signalList);
    sigaddset(&__signalList, SIGALRM);
    sigaddset(&__signalList, SIGVTALRM);
    sigprocmask(SIG_UNBLOCK, &__signalList, NULL);
}


int get_curr_kthread_index() {
    thread_id curr_ktid = gettid();
    // printf("ktid = %ld\n", curr_ktid);
    for(int i = 0; i < NO_OF_KTHREADS; i++) {
    // printf("ktid = %ld\n", thread_list_array[i]->kernel_tid);

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
    // printf("inside vt signal handler\n");    
    // disable alarm
    ualarm(0,0);

    // switch context to scheduler
    int value = sigsetjmp(*(curr_running_proc_array[get_curr_kthread_index()]->t_context), 1);
    if(! value) {
        siglongjmp(*(scheduler_node_array[get_curr_kthread_index()].t_context), 2);
    }
    return;
}


int execute_me_oo(void *new_node) {
	node *nn = (node*)new_node;
	nn->state = THREAD_RUNNING;
    // if(nn->wrapper_fun->args)
    //     printf("pa = %p\n", nn->wrapper_fun->fun);
    // printf("pa = %p\n", nn->wrapper_fun->fun);
ualarm(K_ALARM_TIME,0);
    enable_alarm_signal();
	nn->wrapper_fun->fun(nn->wrapper_fun->args);
	nn->state = THREAD_TERMINATED;
	// printf("termination done %d\n", nn->kthread_index);
    // exit(1);
    
	siglongjmp(*(scheduler_node_array[nn->kthread_index].t_context), 2);
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
    // printf("execute me end\n");
	nn->state = THREAD_TERMINATED;
	// printf("termination done\n");
    //TODO: IMP: don't call scheduler() directly,instead use long jump
    // siglongjmp(*(scheduler_node.t_context), 2);
    siglongjmp(*(scheduler_node_array[nn->kthread_index].t_context), 2);
	return 0;
}


// insert thread_id node in beginning of list
void thread_insert(int thread_index, node* nn) {
        // printf("thread_insert\n");

	nn->next = thread_list_array[thread_index];
	thread_list_array[thread_index] = nn;
}

void scheduler() {
  
    while(1) {
        // printf("inside scheduler\n");
        // traverse();
// exit(1);
        int index = get_curr_kthread_index();
//   printf("inside scheduler2 %d \n", index);

        node* curr_running_proc = curr_running_proc_array[index];
        // printf("inside scheduler\n");

        if(curr_running_proc->state == THREAD_RUNNING)
            curr_running_proc->state = THREAD_RUNNABLE;
//   printf("inside scheduler2\n");
            
        // point next_proc to next thread of currently running process
        node *next_proc = curr_running_proc->next;
        if(! next_proc) next_proc = thread_list_array[index];            // TODO wrong

        while(next_proc->state != THREAD_RUNNABLE) {
            if(next_proc->next) next_proc = next_proc->next;
            else next_proc = thread_list_array[index];
        }

        curr_running_proc_array[index] = next_proc;

        next_proc->state = THREAD_RUNNING;
        enable_alarm_signal();
        ualarm(ALARM_TIME, 0);
        // printf("%ld %ld %d gg ", next_proc->kernel_tid, next_proc->tid, next_proc->kthread_index);
        siglongjmp(*(next_proc->t_context), 2);
    }
    
}

void init_many_many() {     // TODO call only once in therad_create

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
    for(int i=0; i < NO_OF_KTHREADS; i++)
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

    signal(SIGALRM, signal_handler_alarm);
    signal(SIGVTALRM, signal_handler_vtalarm);


    // ualarm(K_ALARM_TIME, 0);
}


int thread_create(mThread *thread, void *attr, void *routine, void *args) {
    if(! thread || ! routine) return INVAL_INP;

    static thread_id id = 0;

    wrap_fun_info *info = (wrap_fun_info*)malloc(sizeof(wrap_fun_info));
    info->fun = routine;
    info->args = args;
    info->thread = thread;
    // printf("p = %p\n", routine);
    node *t_node = (node *)malloc(sizeof(node));
    t_node->tid = id++;
    *thread = t_node->tid;
    t_node->t_context = (jmp_buf*) malloc(sizeof(jmp_buf));
    t_node->ret_val = 0;         // not required
    t_node->stack_start = mmap(NULL, GUARD_PAGE_SIZE + DEFAULT_STACK_SIZE , PROT_READ|PROT_WRITE,MAP_STACK|MAP_ANONYMOUS|MAP_PRIVATE, -1 , 0);
	mprotect(t_node->stack_start, GUARD_PAGE_SIZE, PROT_NONE);
    t_node->stack_size = DEFAULT_STACK_SIZE;      // not required
    t_node->wrapper_fun = info;  // not required
    
    

    if(t_node->tid < NO_OF_KTHREADS) {
        // printf("clone called\n");
        thread_list_array[t_node->tid] = t_node;
        curr_running_proc_array[t_node->tid] = t_node;
        t_node->kernel_tid = clone(execute_me_oo, t_node->stack_start + DEFAULT_STACK_SIZE + GUARD_PAGE_SIZE, CLONE_FLAGS, (void *)t_node);	
        return 0;
    }
    else {
        (*(t_node->t_context))->__jmpbuf[6] = mangle((long int)t_node->stack_start+DEFAULT_STACK_SIZE+GUARD_PAGE_SIZE );
        (*(t_node->t_context))->__jmpbuf[7] = mangle((long int)execute_me_mo);
        
        t_node->state = THREAD_RUNNABLE;
        t_node->kernel_tid = thread_list_array[t_node->tid % NO_OF_KTHREADS]->kernel_tid;
        thread_insert(t_node->tid % NO_OF_KTHREADS, t_node);    // TODO modifye this scheme
    }
    
    return 0;
}


int thread_join(mThread tid, void **retval) {
	if(! retval)
		return INVAL_INP;
	node* n ;
    int found_flag = 0;

	for(int i=0; i<NO_OF_KTHREADS; i++){
        n = thread_list_array[i];
        while(n) {
            if(n->tid == tid){
                found_flag = 1;
                break;
            }
            n = n->next;
        }
        if(found_flag)
            break;
    }

	if(!n)
		return NO_THREAD_FOUND;

	while(n->state != THREAD_TERMINATED)
		;

	*retval = n->ret_val;
	return 0;
}

void thread_exit(void *retval) {

    node* nn;
    int index = get_curr_kthread_index();

    nn = thread_list_array[index];
    while(nn->state != THREAD_RUNNING)
        nn = nn->next;

	nn->ret_val = retval;
	nn->state = THREAD_TERMINATED;
    siglongjmp(*(scheduler_node_array[index].t_context), 2);

	// syscall(SYS_exit, EXIT_SUCCESS);
}

void f1() {
    printf("inside first function\n");
    sleep(2);
    // return;
	while(1){
        sleep(1);
	    printf("inside 1st fun.\n");
    }
}

void f2() {
	    printf("inside 2nd fun.\n");

    while(1){
        sleep(1);
	    printf("inside 2nd fun.\n");
    }
}

void f3() {
    int count = 0;
    void *a;
    while(1){
	    printf("inside 3rd function\n");
        sleep(1);
        count+=1;
        if(count > 4)
            thread_exit(a);
    }
}

void f4() {
    int count = 0;
    void *a;
    while(1){
	    printf("inside 4th function\n");
        sleep(1);
        count+=1;
        if(count > 4)
            thread_exit(a);
    }
}

int main() {
    mThread t1,t2,t3,t4;
    // printf("pam = %p\n", f1);

	init_many_many();
	thread_create(&t1, NULL, f1, NULL);
    thread_create(&t2, NULL, f2, NULL);
    thread_create(&t3, NULL, f3, NULL);
    thread_create(&t4, NULL, f4, NULL);

    void **a;
    // printf("%ld\n", tm);
    // exit(1);
    // thread_join(tm, a);
    // return 0;
    // sleep(1);
    // printf("before join 1\n");
    // thread_join(t3, a);

    // printf("before join 2\n");
    // thread_join(t4, a);
    while(1){
        sleep(3);
	    printf("inside main fun.\n");
    }
    return 0;
}