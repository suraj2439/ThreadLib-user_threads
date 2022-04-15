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
#include "many-one.h"
#include "lock.h"
#include "utils.h"

#define MMAP_FAILED		11
#define SYSCALL_ERROR	13


void scheduler();
void thread_exit(void *retval);

// global tid table to store thread ids 
// of current running therads
tid_list thread_list;

node scheduler_node;
node *curr_running_proc = NULL;


void traverse() {
    node *nn = thread_list.list;
    while(nn) {
        printf("%d %d  %ld ", nn->state, nn->stack_size, nn->tid);
        nn = nn->next;
    }
    printf("\n");
}

void handle_pending_signals() {
    if (!curr_running_proc)
        return;
    ualarm(0,0);
    int k = curr_running_proc->sig_info->rem_sig_cnt;
    sigset_t signal_list;
    for (int i = 0; i < k; i++)
    {
        sigaddset(&signal_list, curr_running_proc->sig_info->arr[i]);
        sigprocmask(SIG_UNBLOCK, &signal_list, NULL);
        printf("ss = %d\n",  curr_running_proc->sig_info->arr[curr_running_proc->sig_info->rem_sig_cnt - 1]);
        curr_running_proc->sig_info->rem_sig_cnt--;
        // printf("ps = %d\n", curr_running_proc->sig_info->rem_sig_cnt);
        raise(curr_running_proc->sig_info->arr[curr_running_proc->sig_info->rem_sig_cnt]);
        // kill(getpid(), --curr_running_proc->sig_info->arr[curr_running_proc->sig_info->rem_sig_cnt]);
    }
    ualarm(ALARM_TIME,0);
    enable_alarm_signal();
    // printf("kk %d\n",  curr_running_proc->sig_info->rem_sig_cnt);
}

//only for testing;
void signal_handler_vtalarm() {
    printf("signal_handler_vtalarm %ld\n", curr_running_proc->tid);
    return;
}

// setjump returns 0 for the first time, next time it returns value used in longjump(here 2) 
// so switch to scheduler will execute only once.
void signal_handler_alarm() {
    // printf("inside signal handler\n");    
    // disable alarm
    ualarm(0,0);

    // printf("printing signals \n");
    // int k = curr_running_proc->sig_info->rem_sig_cnt;
    // for(int i=0; i<k; i++){
    //     printf("sig = %d\n", curr_running_proc->sig_info->arr[i]);
    // }

    // switch context to scheduler
    int value = sigsetjmp(*(curr_running_proc->t_context), 1);
    if(! value) {
        siglongjmp(*(scheduler_node.t_context), 2);
    }
    handle_pending_signals();
    return;
}

void sigterm_signal_handler() {
    // 
    printf("inside sigterm_signal_handler \n");    

    thread_exit(NULL);
    return;
}


int execute_me() {
	// printf("inside execute me\n");
	node *nn = curr_running_proc;
	nn->state = THREAD_RUNNING;
	nn->wrapper_fun->fun(nn->wrapper_fun->args);
    printf("execute me end\n");
	// nn->state = THREAD_TERMINATED;
    thread_exit(NULL);
	// printf("termination done\n");
    //TODO: IMP: don't call scheduler() directly,instead use long jump
    siglongjmp(*(scheduler_node.t_context), 2);

	return 0;
}


void scheduler() {
    while(1) {
        disable_alarm_signal();
        // printf("inside scheduler\n");
        if(curr_running_proc->state == THREAD_RUNNING)
            curr_running_proc->state = THREAD_RUNNABLE;
            
        // point next_proc to next thread of currently running process
        acquire(&thread_list.lock);

        node *next_proc = curr_running_proc->next;
        if(! next_proc) next_proc = thread_list.list;

        while(next_proc->state != THREAD_RUNNABLE) {
            if(next_proc->next) next_proc = next_proc->next;
            else next_proc = thread_list.list;
        }

        release(&thread_list.lock);
        curr_running_proc = next_proc;


        next_proc->state = THREAD_RUNNING;
        enable_alarm_signal();
        ualarm(ALARM_TIME, 0);
        siglongjmp(*(next_proc->t_context), 2);
    }
    
}

// insert thread_id node in beginning of list
void thread_insert(node* nn) {

    acquire(&thread_list.lock);
	nn->next = thread_list.list;
	thread_list.list = nn;
    release(&thread_list.lock);
}

 
void init_many_one() {
    
    scheduler_node.t_context = (jmp_buf*)malloc(sizeof(jmp_buf));

    scheduler_node.stack_start = mmap(NULL, GUARD_PAGE_SIZE + DEFAULT_STACK_SIZE , PROT_READ|PROT_WRITE,MAP_STACK|MAP_ANONYMOUS|MAP_PRIVATE, -1 , 0);
	mprotect(scheduler_node.stack_start, GUARD_PAGE_SIZE, PROT_NONE);
    scheduler_node.stack_size = DEFAULT_STACK_SIZE;
    scheduler_node.tid = 0;
    scheduler_node.wrapper_fun = (wrap_fun_info*)malloc(sizeof(wrap_fun_info));
    scheduler_node.wrapper_fun->fun = scheduler;
    scheduler_node.wrapper_fun->args = NULL;

    (*(scheduler_node.t_context))->__jmpbuf[6] = mangle((long int)scheduler_node.stack_start+DEFAULT_STACK_SIZE+GUARD_PAGE_SIZE );
    (*(scheduler_node.t_context))->__jmpbuf[7] = mangle((long int)scheduler_node.wrapper_fun->fun);

    node *main_fun_node = (node *)malloc(sizeof(node));
    main_fun_node->state = THREAD_RUNNING;
    main_fun_node->tid = 1;
    main_fun_node->t_context = (jmp_buf*) malloc(sizeof(jmp_buf));
    main_fun_node->ret_val = 0;         // not required
    main_fun_node->stack_start = NULL;  // not required
    main_fun_node->stack_size = 0;      // not required
    main_fun_node->wrapper_fun = NULL;  // not required

    main_fun_node->sig_info = (signal_info*)malloc(sizeof(signal_info));
    main_fun_node->sig_info->arr = (int*)malloc(sizeof(int) * DEFAULT_SIGNAL_ARRAY_LENGTH);
    main_fun_node->sig_info->arr_size = DEFAULT_SIGNAL_ARRAY_LENGTH;
    main_fun_node->sig_info->rem_sig_cnt = 0;

    curr_running_proc = main_fun_node;
    thread_insert(main_fun_node);

    signal(SIGALRM, signal_handler_alarm);
    signal(SIGTERM, sigterm_signal_handler);
    // signal(SIGALRM, signal_handler_alarm);
    printf("hello");

    ualarm(ALARM_TIME, 0);
}

int thread_create(mThread *thread, void *attr, void *routine, void *args) {


	static int is_init_done = 0;
	if(! is_init_done){
		init_many_one();
		is_init_done = 1;
	}

    if(! thread || ! routine) return INVAL_INP;

    static thread_id id = 2;
    node *t_node = (node *)malloc(sizeof(node));
    t_node->tid = id++;
    *thread = t_node->tid;
    t_node->t_context = (jmp_buf*) malloc(sizeof(jmp_buf));
    t_node->ret_val = 0;         // not required
    t_node->stack_start = mmap(NULL, GUARD_PAGE_SIZE + DEFAULT_STACK_SIZE , PROT_READ|PROT_WRITE,MAP_STACK|MAP_ANONYMOUS|MAP_PRIVATE, -1 , 0);
	if(t_node->stack_start == MAP_FAILED)
		return MMAP_FAILED;
    mprotect(t_node->stack_start, GUARD_PAGE_SIZE, PROT_NONE);
    t_node->stack_size = DEFAULT_STACK_SIZE;      // not required

    t_node->sig_info = (signal_info*)malloc(sizeof(signal_info));
    t_node->sig_info->arr = (int*)malloc(sizeof(int) * DEFAULT_SIGNAL_ARRAY_LENGTH);
    t_node->sig_info->arr_size = DEFAULT_SIGNAL_ARRAY_LENGTH;
    t_node->sig_info->rem_sig_cnt = 0;

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

int thread_join(mThread tid, void **retval) {
	if(! retval)
		return INVAL_INP;

    acquire(&thread_list.lock);

	node* n = thread_list.list;

	while(n && n->tid != tid)
		n = n->next;

    release(&thread_list.lock);

	if(!n)
		return NO_THREAD_FOUND;

	while(n->state != THREAD_TERMINATED)
		;

	*retval = n->ret_val;
	return 0;
}


int thread_kill(mThread thread, int signal){
    ualarm(0,0);
    if (signal == SIGINT || signal == SIGKILL)
        kill(getpid(), signal);
    else if(signal == SIGCONT){

        acquire(&thread_list.lock);

        node* n = thread_list.list;

        while(n && n->tid != thread){
            n = n->next;
            if(n==NULL)
                return -1;
        }
        release(&thread_list.lock);
        n->state = THREAD_RUNNABLE;
    }
    else
    {
        if(curr_running_proc->tid == thread)
            raise(signal);
        else{
            acquire(&thread_list.lock);

            node* n = thread_list.list;

            while(n && n->tid != thread){
                n = n->next;
                if(n == NULL)
                    return -1;  // TODO return error no
            }
            release(&thread_list.lock);
            n->sig_info->arr[n->sig_info->rem_sig_cnt++] = signal;
            printf("inside thread kill %d %d\n", n->sig_info->arr[n->sig_info->rem_sig_cnt - 1], signal);
        }
    }
    ualarm(ALARM_TIME, 0);
    return 0;
}


void thread_exit(void *retval) {
    printf("inside thread exit\n");
	node* n = curr_running_proc;
	n->ret_val = retval;
    traverse();

	n->state = THREAD_TERMINATED;
    traverse();
    siglongjmp(*(scheduler_node.t_context), 2);

	// syscall(SYS_exit, EXIT_SUCCESS);
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

sleeplock test;
int c = 0;
void myFun() {
	printf("inside 1st fun.\n");
	// sleep(3);
	// printf("above sleep\n");
	// void *t;
	// thread_exit(t);
	// printf("below sleep\n");
	
	int c1;
	while(1){
	    printf("inside 1st fun.\n");
		acquiresleep(&test);
		c++;
		c1++;
		releasesleep(&test);
		if(c1>10)
			break;
	}
    sleep(1);

	printf("inside 2nd fun c1  = %d\n", c1);

}

void myF() {
	// sleep(3);
	printf("inside 2nd fun\n");
	int c2 = 0;
	while(1){
	    printf("inside 2nd fun\n");
		acquiresleep(&test);
		c++;
		c2++;
		releasesleep(&test);
        sleep(1);
    	if(c2>20)
			break;
	}
	printf("inside 2nd fun c2  = %d\n", c2);

}


int main() {
    initsleeplock(&test);
    mThread td;
	mThread tt, tm;

	// init_many_one(); 

	thread_create(&td, NULL, myF, NULL);
    thread_create(&tt, NULL, myFun, NULL);
    // thread_create(&tm, NULL, f3, NULL);
    // signal(SIGVTALRM, signal_handler_vtalarm);

    // printf("sending signal to %ld\n", tt);
    // thread_kill(tt, SIGVTALRM);

	
    // node* t = thread_list.list;
    
    void **a;
    // printf("%ld\n", tm);
    // exit(1);
    thread_join(td, a);
    printf("join success");
    printf("c=%d\n", c);
    // traverse();
    // return 0;
    // sleep(1);

    // thread_kill(td, SIGTERM);
    for(int i=0; i<10; i++)
        printf("inside main fun waiting for cont.\n");
    // thread_kill(td, SIGCONT);


    while(1){
        // sleep(1);
	    // printf("inside main fun.\n");
    }
    return 0;
}
