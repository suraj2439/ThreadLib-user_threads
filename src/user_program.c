#include <stdio.h>
#include <unistd.h>
#include "one-one.h"

void f1() {
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
    mThread t1, t2, t3, t4;
    init_threading();
    thread_create(&t1, NULL, f1, NULL);

    while(1);
}