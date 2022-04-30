#define _GNU_SOURCE
#include <signal.h>
#include <stdlib.h>
#include "many-many.h"
#include <unistd.h>
#include <stdio.h>

extern int kthread_index[NO_OF_KTHREADS];

// Code reference : https://stackoverflow.com/questions/69148708/alternative-to-mangling-jmp-buf-in-c-for-a-context-switch
long int mangle(long int p) {
    long int ret;
    asm(" mov %1, %%rax;\n"
        " xor %%fs:0x30, %%rax;"
        " rol $0x11, %%rax;"
        " mov %%rax, %0;"
        : "=r"(ret)
        : "r"(p)
        : "%rax"
    );
    return ret;
}

void enable_alarm_signal() {
    sigset_t __signalList;
    sigemptyset(&__signalList);
    sigaddset(&__signalList, SIGALRM);
    sigaddset(&__signalList, SIGVTALRM);
    sigprocmask(SIG_UNBLOCK, &__signalList, NULL);
}


void disable_alarm_signal() {
    sigset_t __signalList;
    sigemptyset(&__signalList);
    sigaddset(&__signalList, SIGALRM);
    sigaddset(&__signalList, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &__signalList, NULL);
}


int get_curr_kthread_index() {
    thread_id curr_ktid = gettid();
    // printf("kernel thred id %ld\n", curr_ktid);
    for(int i = 0; i < NO_OF_KTHREADS; i++) {
        // printf("array k thred id %d\n", kthread_index[i]);

        if(curr_ktid == kthread_index[i])
            return i; 
    }
    // printf("DEBUG : get_curr_kthread_index() not found. Aborting\n");
    // exit(1);
    return -1;  // ERROR
}