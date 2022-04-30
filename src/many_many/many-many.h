#ifndef MANY_MANY_H
#define MANY_MANY_H

#include <setjmp.h>

#define INVAL_INP	10
#define DEFAULT_STACK_SIZE	32768
#define THREAD_SLEEPING 19
#define THREAD_RUNNING 20
#define THREAD_TERMINATED 21
#define THREAD_RUNNABLE 22
#define NO_THREAD_FOUND 23
#define GUARD_PAGE_SIZE	4096
#define ALARM_TIME 100  // in microseconds 
#define NO_OF_KTHREADS 5
#define K_ALARM_TIME    (ALARM_TIME / NO_OF_KTHREADS)
#define CLONE_FLAGS     CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD |CLONE_SYSVSEM|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID

#define MMAP_FAILED		11
#define CLONE_FAILED	12
#define INVALID_SIGNAL	13
#define RAISE_ERROR     14

typedef unsigned long int thread_id;
typedef unsigned long int mThread;

#include "lock.h"

typedef struct sig_node {
    int t_signal;
    struct sig_node *next;
} sig_node;

typedef struct signal_info {
    sig_node *signal_list;
    int rem_sig_cnt;
} signal_info;


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
    signal_info *sig_info;
	int state;
	void* ret_val;
    jmp_buf *t_context;      // use to store thread specific context
    struct node* next;
    void* chan;
} node;

typedef struct node_list {
    node* list;
    spinlock lock;
}node_list;

typedef struct mThread_attr {
	void *stack;
    int stackSize;
    int guardSize;
} mThread_attr;

// The  thread_create() function starts a new thread in the calling process.  The new thread starts execution by invoking routine(); 
// arg is passed as the sole argument of routine().
int thread_create(mThread *thread, mThread_attr *attr, void *routine, void *args);

// The  thread_join() function waits for the thread specified by thread to terminate.  
// If that thread has already terminated, then pthread_join() returns immediately. 
int thread_join(mThread tid, void **retval);

// The  pthread_exit()  function terminates the calling thread and returns a value via retval that is available to another thread in 
// the same process that calls thread_join()
void thread_exit(void *retval);

// The thread_kill() function sends the signal sig to thread, a thread in the same process as the caller
int thread_kill(mThread thread, int signal);


void init_thread_lock(struct spinlock *lk);
void thread_lock(struct spinlock *lk);
void thread_unlock(struct spinlock *lk);

void init_mutex_thread_lock(struct sleeplock *lk);
void thread_mutex_lock(struct sleeplock *lk);
void thread_mutex_unlock(struct sleeplock *lk);

#endif
