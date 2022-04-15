#include <stdio.h>
#include <unistd.h>
#include "many_many/many-many.h"

void ff() {
	    printf("inside 1st fun.\n");
}

void f2f() {
	    printf("inside 2nd fun.\n");

    while(1){
        sleep(1);
	    printf("inside 2nd fun.\n");
    }
}

void f3f() {
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

void f4f() {
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
    thread_create(&t1, NULL, ff, NULL);
    void **a;
    thread_join(t1, a);
}
