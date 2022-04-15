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
#include "many-many.h"
#include "utils.h"
#include "lock.h"

#define MMAP_FAILED		11
#define CLONE_FAILED	12
#define INVALID_SIGNAL	13
#define RAISE_ERROR     14

node_list thread_list;
int alarm_index = -1;
thread_id main_ktid;

void scheduler();
void thread_exit(void *retval);


node* scheduler_node_array;
node** curr_running_proc_array;
int kthread_index[NO_OF_KTHREADS];

void traverse() {
    node *nn = thread_list.list;

    while(nn) {
        printf("traverse %d %ld %ld  ", nn->state, nn->tid, nn->kernel_tid);
        nn = nn->next;
    }
    printf("\n");
    
}

void cleanup(thread_id tid) {
	printf("given tid %ld\n", tid);
	// acquire(&thread_list.lock);
	node *prev = NULL, *curr = thread_list.list;
	while(curr && curr->tid != tid) {
		prev = curr;
		curr = curr->next;
	}
	if(curr == thread_list.list) {
		// head node
		node *tmp = thread_list.list;
		thread_list.list = thread_list.list->next;
        // release(&thread_list.lock);
		munmap(tmp, tmp->stack_size);
		free(tmp->wrapper_fun);
        free(tmp->sig_info);
		free(tmp);
        // printf("cleaned one node\n");
		return;
	}
	if(! curr) {
		printf("DEBUG: cleanup node not found\n");
        // release(&thread_list.lock);
		return;
	}
	prev->next = curr->next;
	// release(&thread_list.lock);
	munmap(curr->stack_start, curr->stack_size);
	free(curr->wrapper_fun);
    free(curr->sig_info);
	free(curr);
    // printf("cleaned one node\n");
    return;
}

void cleanupAll() {
	printf("Cleaning all theread stacks\n");
	while(thread_list.list)
		cleanup(thread_list.list->tid);
}

void handle_pending_signals() {
    int curr_kthread_index = get_curr_kthread_index();
    
    // if currently no thread is executing on current kernel thread
    if (! curr_running_proc_array[curr_kthread_index])
        return;

    ualarm(0,0);
    node* curr_thread = curr_running_proc_array[curr_kthread_index];
    int k = curr_thread->sig_info->rem_sig_cnt;
    printf("total pending signals %d\n", k);
    sigset_t signal_list;
    for (int i = 0; i < k; i++) {
        int signal_to_handle = curr_thread->sig_info->signal_list->t_signal;
        printf("handle pending signal %d of %ld tid\n", signal_to_handle, curr_thread->tid);
        sigaddset(&signal_list, signal_to_handle);
        sigprocmask(SIG_UNBLOCK, &signal_list, NULL);
        // printf("ss = %d\n",  curr_thread->sig_info->arr[curr_thread->sig_info->rem_sig_cnt - 1]);
        curr_thread->sig_info->rem_sig_cnt--;
        curr_thread->sig_info->signal_list = curr_thread->sig_info->signal_list->next;
        // printf("ps = %d\n", curr_running_proc->sig_info->rem_sig_cnt);
        raise(signal_to_handle);
        // kill(getpid(), --curr_running_proc->sig_info->arr[curr_running_proc->sig_info->rem_sig_cnt]);
    }
    ualarm(ALARM_TIME,0);
    enable_alarm_signal();
    printf("kk %d\n",  curr_thread->sig_info->rem_sig_cnt);
}

// setjump returns 0 for the first time, next time it returns value used in longjump(here 2) 
// so switch to scheduler will execute only once.
void signal_handler_alarm() {
    // printf("inside signal handler\n");    
    // disable alarm
    ualarm(0,0);
    alarm_index = (alarm_index + 1) % NO_OF_KTHREADS;
	syscall(SYS_tgkill, getpid(), kthread_index[alarm_index], SIGVTALRM);
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
    handle_pending_signals();
    return;
}


int execute_me_oo(void *new_node) {
	node *nn = (node*)new_node;
	nn->state = THREAD_RUNNING;
    // if(nn->wrapper_fun->args)
    //     printf("pa = %p\n", nn->wrapper_fun->fun);
    // printf("pa = %p\n", nn->wrapper_fun->fun);thread_kill
    ualarm(K_ALARM_TIME,0);
    enable_alarm_signal();
    nn->kernel_tid = gettid();
	nn->wrapper_fun->fun(nn->wrapper_fun->args);
	// nn->state = THREAD_TERMINATED;
    thread_exit(NULL);

	// printf("termination done %d\n", nn->kthread_index);
    // exit(1);
    
	siglongjmp(*(scheduler_node_array[nn->kthread_index].t_context), 2);
}

int execute_me_mo() {
    // thread_id curr_ktid = gettid();
    // int i;
    // for(i = 0; i < NO_OF_KTHREADS; i++) {
    //     if(curr_ktid == thread_list.list_array[i]->kernel_tid)
    //         break;
    // }
    acquire(&thread_list.lock);
    node *nn = thread_list.list;
    while(nn->state != THREAD_RUNNING)
        nn = nn->next;
    release(&thread_list.lock);
    
	// printf("inside execute me\n");
    nn->kernel_tid = gettid();
	nn->wrapper_fun->fun(nn->wrapper_fun->args);
    // printf("execute me end\n");
	// nn->state = THREAD_TERMINATED;
    thread_exit(NULL);

	// printf("termination done\n");
    //TODO: IMP: don't call scheduler() directly,instead use long jump
    // siglongjmp(*(scheduler_node.t_context), 2);
    siglongjmp(*(scheduler_node_array[nn->kthread_index].t_context), 2);
	return 0;
}

void insert_sig_node(signal_info *info, sig_node *node) {
    node->next = info->signal_list;
    info->signal_list = node;
}

node* search_thread(thread_id tid) {

    node n;
    int found_flag = 0;

    acquire(&thread_list.lock);
    node *tmp = thread_list.list;
    while(tmp && tmp->tid != tid)
        tmp = tmp->next;
    release(&thread_list.lock);

    if(! tmp) {
        // error condition
        printf("Debug: thread not found error. exiting\n");
        exit(1);
    }
	return tmp;
}



// insert thread_id node in beginning of list
void thread_insert(node* nn) {
    acquire(&thread_list.lock);
	nn->next = thread_list.list;
	thread_list.list = nn;
    release(&thread_list.lock);
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
        // printf("inside scheduler2\n");
            
        // point next_proc to next thread of currently running process
        acquire(&thread_list.lock);
        node *next_proc = curr_running_proc->next;
        if(! next_proc) next_proc = thread_list.list;            // TODO wrong

        while(next_proc->state != THREAD_RUNNABLE) {
            if(next_proc->next) next_proc = next_proc->next;
            else next_proc = thread_list.list;
        }
        release(&thread_list.lock);

        curr_running_proc_array[index] = next_proc;

        next_proc->state = THREAD_RUNNING;
        enable_alarm_signal();
        ualarm(ALARM_TIME, 0);
        // printf("%ld %ld %d gg ", next_proc->kernel_tid, next_proc->tid, next_proc->kthread_index);
        next_proc->kernel_tid = gettid();
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
    main_ktid = gettid();
    acquire(&thread_list.lock);
    thread_list.list = (node*)malloc(sizeof(node)*NO_OF_KTHREADS);
    release(&thread_list.lock);
    // for(int i=0; i < NO_OF_KTHREADS; i++)
    //     thread_list.list_array[i] = NULL;

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

int thread_kill(mThread thread, int signal){
    ualarm(0,0);
    if (signal == SIGINT || signal == SIGCONT || signal == SIGSTOP)
        kill(getpid(), signal);
    else {

        int curr_kthread_index = get_curr_kthread_index();

        if(curr_kthread_index != -1 && curr_running_proc_array[curr_kthread_index]->tid == thread) {
            int val = raise(signal);
            if(val == -1)
		        return RAISE_ERROR;
        }
        else {
            node* n = (node *)malloc(sizeof(node)); // redundant
            sig_node *signal_node = (sig_node*)malloc(sizeof(sig_node));
            signal_node->t_signal = signal;
            int ktid;
            n = search_thread(thread);
            // printf("adding signal node in %ld tid node having %d ktid\n", n->tid, n->kthread_index);

            insert_sig_node(n->sig_info, signal_node);
            n->sig_info->rem_sig_cnt++;
            printf("inside thread kill %d %d\n", n->sig_info->signal_list->t_signal, signal);
        }
    }
    ualarm(ALARM_TIME, 0);
}


int thread_create(mThread *thread, void *attr, void *routine, void *args) {
	static int is_init_done = 0;
	if(! is_init_done) {
        atexit(cleanupAll);
		init_many_many();
		is_init_done = 1;
	}

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
	if(t_node->stack_start == MAP_FAILED)
		return MMAP_FAILED;
    mprotect(t_node->stack_start, GUARD_PAGE_SIZE, PROT_NONE);
    t_node->stack_size = DEFAULT_STACK_SIZE;      // not required
    t_node->wrapper_fun = info;  // not required

    t_node->sig_info = (signal_info*)malloc(sizeof(signal_info));
    t_node->sig_info->signal_list = NULL;
    t_node->sig_info->rem_sig_cnt = 0;
    
    
    if(t_node->tid < NO_OF_KTHREADS) {
        // printf("clone called\n");
        curr_running_proc_array[t_node->tid] = t_node;
        thread_insert(t_node);
        // kthread_index[t_node->tid] = t_node;
        kthread_index[t_node->tid] = clone(execute_me_oo, t_node->stack_start + DEFAULT_STACK_SIZE + GUARD_PAGE_SIZE, CLONE_FLAGS, (void *)t_node);	
        if(kthread_index[t_node->tid] == -1) 
		    return CLONE_FAILED;
        return 0;
    }
    else {
        (*(t_node->t_context))->__jmpbuf[6] = mangle((long int)t_node->stack_start+DEFAULT_STACK_SIZE+GUARD_PAGE_SIZE );
        (*(t_node->t_context))->__jmpbuf[7] = mangle((long int)execute_me_mo);
        
        t_node->state = THREAD_RUNNABLE;
        // t_node->kernel_tid = thread_list.list_array[t_node->tid % NO_OF_KTHREADS]->kernel_tid;
        thread_insert(t_node);    // TODO modifye this scheme
    }
    
    return 0;
}


int thread_join(mThread tid, void **retval) {
	if(! retval)
		return INVAL_INP;
    int found_flag = 0;

    acquire(&thread_list.lock);
	node* n = thread_list.list;

    while(n) {
        if(n->tid == tid){
            // found_flag = 1;
            break;
        }
        n = n->next;
    }
    release(&thread_list.lock);

	// for(int i=0; i<NO_OF_KTHREADS; i++){
    //     n = thread_list.list_array[i];
    //     while(n) {
    //         if(n->tid == tid){
    //             found_flag = 1;
    //             break;
    //         }
    //         n = n->next;
    //     }
    //     if(found_flag)
    //         break;
    // }

	if(!n)
		return NO_THREAD_FOUND;

	while(n->state != THREAD_TERMINATED)
		;

	*retval = n->ret_val;
    cleanup(tid);
	return 0;
}

void thread_exit(void *retval) {

    node* nn;
    int index = get_curr_kthread_index();

    acquire(&thread_list.lock);
    nn = thread_list.list;
    while(nn->state != THREAD_RUNNING && nn->kernel_tid == gettid())
        nn = nn->next;
    release(&thread_list.lock);

	nn->ret_val = retval;
	nn->state = THREAD_TERMINATED;
    siglongjmp(*(scheduler_node_array[index].t_context), 2);

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


// int main() {
//     mThread t1,t2,t3,t4;
//     // printf("pam = %p\n", f1);

// 	// init_many_many();
// 	thread_create(&t1, NULL, f1, NULL);
//     thread_create(&t2, NULL, f2, NULL);
//     thread_create(&t3, NULL, f3, NULL);
//     thread_create(&t4, NULL, f4, NULL);

//     // exit(1);
//     printf("giving signal to thread id %ld", t4);
//     // thread_kill(t4, SIGUSR1);

//     void **a;
//     // printf("%ld\n", tm);
//     // exit(1);
//     // thread_join(t3, a);
//     // return 0;
//     // sleep(1);
//     // printf("before join 1\n");
//     // thread_join(t3, a);

//     // printf("before join 2\n");
//     // thread_join(t4, a);
//     while(1){
//         sleep(3);
// 	    printf("inside main fun.\n");
//     }
//     return 0;
// }
