#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include "many-one.h"

#define TEST_SUCCESS    printf("Test Passed.\n");
#define TEST_FAILURE    printf("Test Failed.\n");


spinlock lock;  // TODO lock init
spinlock rwlock;
int readers = 0;

void frw1()
{
    thread_lock(&lock);
    readers += 1;
    if (readers == 1)
    {
        printf("Acquired writers lock\n");
        thread_lock(&rwlock);
    }
    thread_unlock(&lock);
    printf("Reader process in\n");
    thread_lock(&lock);
    readers -= 1;
    if (readers == 0)
    {
        thread_unlock(&rwlock);
    }
    thread_unlock(&lock);
}

void frw2()
{
    thread_lock(&rwlock);
    printf("Writer process in\n");
    thread_unlock(&rwlock);
}

void readers_writers_test() {
    mThread t1, t2;
    thread_create(&t1, NULL, frw1, NULL);
    thread_create(&t2, NULL, frw2, NULL);
    thread_join(t2, NULL);
    thread_join(t1, NULL);
}

void line() {
    printf("\n");
    for(int i = 0; i < 100; i++) 
        printf("-");
    printf("\n");
}

void emptyfun() {
}

void join_fun() {
    // for(int i = 0; i < 3; i++) 
        // sleep(1);
    printf("fun finished\n");
}

void f1() {
	while(1){
	    printf("1st fun\n");
        sleep(2);
    }
}

void f2() {
    while(1){
	    printf("2nd fun\n");
        sleep(1);
    }
}

void thread_create_test() {
    int cnt = 5;
    printf("Testing thread_create() for %d threads.\n", cnt);
    int success = 0;
    int failure = 0;
    mThread threads[cnt];
    for(int i = 0; i < cnt; i++) {
        thread_create(&(threads[i]), NULL, emptyfun, NULL) ? failure++ : success++;
    }
    printf("thread_create() test result: \nsuccess : %d\nfailure : %d\n", success, failure);
}

void thread_join_test() {
    int success = 0;
    int failure = 0;
    int s = 0, f = 0;
    int cnt = 5;
    printf("Testing thread_join()\n");
    mThread t[cnt];
    puts("");
    printf("Joining threads upon creation in a sequential order\n");
    printf("creating %d threads.\n", cnt);
    for (int i = 0; i < cnt; i++) {
        if (thread_create(&t[i], NULL, join_fun, NULL) == 0) {
            printf("Thread %d created successfully with id %ld\n", i, t[i]);
            s++;
        }
        else {
            printf("Thread creation failed\n");
            f++;
        }
    }
    printf("join start\n");
    for (int i = 0; i < cnt; i++) {
        void **tmp;
        thread_join(t[i], tmp);
        sleep(0.5);
    }
    printf( "Join test completed with the following statistics:\n");
    printf("Success: %d\n", s);
    printf("Failures: %d\n", f);
    if (f)
        failure += 1;
    else
        success += 1;
    return;
}

void testSig() {
    int success = 0;
    int failure = 0;

    printf("Testing thread_kill()\n\n");
    mThread t1, t2, t3;
    printf("Sending a signal to a running thread\n");
    thread_create(&t1, NULL, f1, NULL);
    sleep(1);
    int ret = thread_kill(t1, SIGVTALRM);
    thread_join(t1, NULL);
    if (ret != -1) {
        TEST_SUCCESS
        success += 1;
    }
    else {
        TEST_FAILURE
        failure += 1;
    }

    printf("\nSending a signal to an already exited thread\n");
    thread_create(&t2, NULL, f1, NULL);
    thread_join(t2, NULL);
    ret = thread_kill(t2, SIGTERM);
    if (ret == -1) {
        TEST_SUCCESS
        success += 1;
    }
    else {
        TEST_FAILURE
        failure += 1;
    }

    printf("\nSending a process wide signal\n");
    thread_create(&t3, NULL, f1, NULL);
    thread_join(t3, NULL);
    ret = thread_kill(t3, SIGVTALRM);
    if (ret == -1)
        failure += 1;
}


void fexit() {
    int *tid = (int *)malloc(sizeof(int));
    *tid = gettid();
    printf("Exiting thread 1 with value %d\n", *(int *)tid);
    thread_exit(tid);
}

void thread_exit_test() {
    int *retVal1 = (int *)malloc(sizeof(int));
    int *retVal2 = (int *)malloc(sizeof(int));
    printf("Testing thread_exit()\n\n");
    mThread t1, t2;
    int *retVal = (int *)malloc(sizeof(int));
    thread_create(&t1, NULL, fexit, NULL);
    thread_create(&t2, NULL, fexit, NULL);
    thread_join(t1, (void **)&retVal1);
    thread_join(t2, (void **)&retVal2);
    printf("Joined thread 1 exit value : %d\n", *(retVal1));
    printf("Joined thread 2 exit value : %d\n", *(retVal2));
    TEST_SUCCESS
}

void farg(void *arg) {
    int *ret = (int *)malloc(sizeof(int));
    int *a = (int*)arg;
    printf("Argument received is : %d\n", *a);
    if(*a == 100)
        *ret = 1;
    else *ret = 0;
    thread_exit(ret);
}

void thread_funArgs_test() {
    printf("Testing Function Arguments of thread.\n\n");
    printf("Passing argument as 100 to function.\n");
    mThread t1;
    int a = 100;
    thread_create(&t1, NULL, farg, (void*)&a);
    int *retVal = (int *)malloc(sizeof(int));
    thread_join(t1,  (void **)&retVal);
    if(*retVal) TEST_SUCCESS
    else TEST_FAILURE
}


void thread_create_robust() {
    mThread t;
    printf("Creating threads with invalid arguments\n");
    if (thread_create(NULL, NULL, NULL, NULL) == INVAL_INP && thread_create(&t, NULL, NULL, NULL) == INVAL_INP && thread_create(NULL, NULL, f1, NULL) == INVAL_INP)
        TEST_SUCCESS
    else TEST_FAILURE
}

void thread_join_robust() {
    printf("Joining random thread(thread already joined/random tid/thread rejoin).\n");
    mThread t;
    void **tmp;
    if(thread_join(t, tmp) == NO_THREAD_FOUND)
        TEST_SUCCESS
    else TEST_FAILURE

    // thread_create(&t, NULL, f1, NULL);
}

void thread_kill_robust() {
    printf("Sending 1) Invalid Signal and 2) Invalid tid \n");
    mThread t;
    if (thread_kill(t, 0) != 0 && thread_kill(t, SIGABRT) != 0) TEST_SUCCESS
    else TEST_FAILURE
}


int c = 0;
spinlock test;

void lockFun1() {
    int *ret = (int *)malloc(sizeof(int));
	printf("inside 1st fun.\n");
	int c1;
	while(1) {
		thread_lock(&test);
		c++;
		c1++;
		thread_unlock(&test);
		if(c1>1000000)
			break;
	}
	printf("inside 2nd fun c1  = %d\n", c1);
    *ret = c1;
    thread_exit(ret);
}


void lockFun2() {
    int *ret = (int *)malloc(sizeof(int));
	int c2 = 0;
	while(1) {
		thread_lock(&test);
		c++;
		c2++;
		thread_unlock(&test);
		if(c2>1000000)
			break;
	}
	printf("inside 2nd fun c2  = %d\n", c2);
    *ret = c2;
    thread_exit(ret);
}


void thread_lock_unlock_test() {
    printf("Testing thread_lock() and thread_unlock().");
    mThread t1, t2;
    thread_create(&t1, NULL, lockFun1, NULL);
    thread_create(&t2, NULL, lockFun2, NULL);
    void **tmp;
    int *c1 = (int *)malloc(sizeof(int));
    int *c2 = (int *)malloc(sizeof(int));
    int *retVal = (int *)malloc(sizeof(int));
    thread_join(t1, (void **)&c1);
    thread_join(t2, (void **)&c2);

    printf("value of c = %d\n", c);
    if(*c1 + *c2 == c)
        TEST_SUCCESS
    else TEST_FAILURE
}


void unitTesting() {
    line();
    printf("PERFORMING UNIT TESTING TO CHECK BASIC FEATURES.\n");
    line();
    thread_create_test();
    line();
    thread_join_test();
    line();
    // TODO attribute test 
    // testSig();      // TODO handle thread specific signal
    thread_exit_test();
    line();
    thread_funArgs_test();
    line();
    thread_lock_unlock_test();
    line();
}

void robustTesting() {
    line();
    printf("PERFORMING ROBUST TESTING TO CHECK RELIABILITY.\n");
    line();
    thread_create_robust();
    line();
    thread_join_robust();
    line();
    thread_kill_robust();
    line();
}


int main() {
    mThread t1, t2, t3, t4, t5;
    // thread_create(&t1, NULL, join_fun, NULL);
    // thread_create(&t2, NULL, join_fun, NULL);
    // thread_create(&t3, NULL, join_fun, NULL);
    // thread_create(&t4, NULL, join_fun, NULL);
    // thread_create(&t5, NULL, join_fun, NULL);

    // // thread_create_test();
    // printf("join start\n");
    // void **t;
    // thread_join(t2, t);
    // thread_join(t1, t);
    // thread_join(t3, t);
    // thread_join(t5, t);
    // thread_join(t4, t);
    // printf("join done\n");

    unitTesting();
    robustTesting();
    // readers_writers_test();
    thread_join_test();

    printf("done\n");
    while(1) {
        // printf("in main\n");
        sleep(1);
    }
}
