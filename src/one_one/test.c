#include <stdio.h>
#include <unistd.h>
#include "one-one.h"

void emptyloop() {
    while(1)
        sleep(1);
}

void join_fun() {
    for(int i = 0; i < 5; i++) sleep(1);
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
    printf("Testing thread_create() for 10 threads.\n");
    int success = 0;
    int failure = 0;
    mThread threads[10];
    for(int i = 0; i < 10; i++) {
        thread_create(&(threads[i]), NULL, emptyloop, NULL) ? failure++ : success++;
    }
    printf("thread_create() test result: \nsuccess : %d\nfailure : %d\n", success, failure);
}

void testJoin()
{
    int success = 0;
    int failure = 0;
    int s = 0, f = 0;
    printf("Testing thread_join()\n");
    mThread t[5];
    puts("");
    printf("Joining threads upon creation in a sequential order\n");
    printf("creating 5 threads.\n");
    for (int i = 0; i < 5; i++)
    {
        if (thread_create(&t[i], NULL, join_fun, NULL) == 0) {
            printf("Thread %d created successfully with id %ld\n", i, t[i]);
            s++;
        }
        else {
            printf("Thread creation failed\n");
            f++;
        }
    }
    for (int i = 0; i < 5; i++) {
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



int main() {
    mThread t1, t2;
    init_threading();
    thread_create(&t1, NULL, join_fun, NULL);
    thread_create(&t2, NULL, join_fun, NULL);

    // thread_create_test();
    testJoin();
    // void **t;
    // thread_join(t2, t);
    // thread_join(t1, t);
    // printf("join done\n");

    while(1) {
        // printf("in main\n");
        sleep(1);
    }
}