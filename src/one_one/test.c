#include <stdio.h>
#include <unistd.h>
#include "one-one.h"

void emptyfun() {
}

void join_fun() {
    for(int i = 0; i < 3; i++) 
        sleep(1);
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

void testJoin()
{
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



int main() {
    mThread t1, t2, t3, t4, t5;
    init_threading();
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

    thread_create_test();
    testJoin();

    while(1) {
        // printf("in main\n");
        sleep(1);
    }
}