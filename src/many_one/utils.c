#include <signal.h>
#include <stdlib.h>

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
    sigset_t signalList;
    sigemptyset(&signalList);
    sigaddset(&signalList, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &signalList, NULL);
}

void disable_alarm_signal() {
    sigset_t signalList;
    sigemptyset(&signalList);
    sigaddset(&signalList, SIGALRM);
    sigprocmask(SIG_BLOCK, &signalList, NULL);
}
