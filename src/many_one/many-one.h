#ifndef MANY_ONE_H
#define MANY_ONE_H

#include <setjmp.h>

#define INVAL_INP	10
#define DEFAULT_STACK_SIZE	32768
#define THREAD_RUNNING 20
#define THREAD_TERMINATED 21
#define THREAD_RUNNABLE 22
#define NO_THREAD_FOUND 22
#define GUARD_PAGE_SIZE	4096
#define ALARM_TIME 100000  // in microseconds 
#define DEFAULT_SIGNAL_ARRAY_LENGTH 10
#define MAIN_TID	1
#define START_TID	2

#define MMAP_FAILED		11
#define INVALID_SIGNAL	13


typedef unsigned long int thread_id;
typedef unsigned long int mThread;

#include "lock.h"

typedef struct wrap_fun_info {
	void (*fun)(void *);
	void *args;
	mThread *thread;
} wrap_fun_info;


typedef struct signal_info {
    int* arr;
    int arr_size;
    int rem_sig_cnt;
} signal_info;

typedef struct node {
	thread_id tid;
	int stack_size;
	void *stack_start;
	wrap_fun_info* wrapper_fun;
    signal_info* sig_info;
	int state;
	void* ret_val;
    jmp_buf *t_context;      // use to store thread specific context
    struct node* next;
} node;


typedef struct tid_list{
	node* list;
	spinlock lock;
} tid_list;

typedef struct mThread_attr {
	void *stack;
    int stackSize;
    int guardSize;
} mThread_attr;

// The  thread_create() function starts a new thread in the calling process.  The new thread starts execution by invoking routine(); 
// arg is passed as the sole argument of routine().
int thread_create(mThread *thread, const mThread_attr *attr, void *routine, void *args);

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

#endif