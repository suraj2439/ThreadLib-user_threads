#ifndef ONE_ONE_H
#define ONE_ONE_H

#define INVAL_INP	10
#define DEFAULT_STACK_SIZE	32768
#define THREAD_SLEEPING 19
#define THREAD_RUNNING 20
#define THREAD_TERMINATED 21
#define NO_THREAD_FOUND 22
#define GUARD_PAGE_SIZE	4096

#define MMAP_FAILED		11
#define CLONE_FAILED	12
#define INVALID_SIGNAL	13

typedef unsigned long int thread_id;
typedef unsigned long int mThread;

#include "lock.h"

typedef struct wrap_fun_info {
	void (*fun)(void *);
	void *args;
	mThread *thread;
} wrap_fun_info;

typedef struct node {
	thread_id tid;
	int stack_size;
	void *stack_start;
	struct node* next;
	wrap_fun_info* wrapper_fun;
	int state;
	void* ret_val;
} node;


typedef struct tid_list {
	node* list;
	spinlock lock;
} tid_list;

typedef struct mThread_attr {
	void *stack;
    int stackSize;
    int guardSize;
} mThread_attr;

void init_threading();  // TODO remove this

void init_mThread_attr();

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

void init_mutex_thread_lock(struct sleeplock *lk);
void thread_mutex_lock(struct sleeplock *lk);
void thread_mutex_unlock(struct sleeplock *lk);

#endif