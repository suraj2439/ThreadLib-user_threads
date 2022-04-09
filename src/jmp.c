#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

jmp_buf buf, buf2;

int flag = 1;

void myFun() {
   flag = 0;
   printf("hello inside function \n");
   siglongjmp(buf, 2);
   return;
}

void exitHandler() {
   printf("hello from exit handler.\n");
}


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

int main() {
   atexit(exitHandler);
   int x = 1;
   // printf("sigset %d\n", sigsetjmp(buf2, 1)); 
   printf("sigset %d\n", sigsetjmp(buf, 1)); 
   // sigsetjmp(buf, 1);
   
   // buf2->__jmpbuf[7] = mangle((long int)myFun);
   // if(flag)
   //    siglongjmp(buf2, 2);
   printf("5\n"); 
   x++;
   if (x <= 4)
      siglongjmp(buf, 33);
   return 0;
}