
#include <stdio.h>
#include <stdlib.h>
#include "one_one/one-one.h"
#include <time.h>

#include <sys/time.h>

// Returns the current time in microseconds.
long getMicrotime(){
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

// maximum size of matrix
#define MAX 1000
 
// maximum number of threads
#define MAX_THREAD 1000
 
int matA[MAX][MAX];
int matB[MAX][MAX];
int matC[MAX][MAX];
int step_i = 0;
 
void* multi(void* arg)
{
    int i = step_i++; //i denotes row number of resultant matC
   
    for (int j = 0; j < MAX; j++)
      for (int k = 0; k < MAX; k++)
        matC[i][j] += matA[i][k] * matB[k][j];
}


void multiply(int isThreading) {
    for (int i = 0; i < MAX; i++) {
        for (int j = 0; j < MAX; j++) {
            matA[i][j] = rand() % 10;
            matB[i][j] = rand() % 10;
        }
    }

    // declaring four threads
    mThread threads[MAX_THREAD];
 
    // Creating four threads, each evaluating its own part
    for (int i = 0; i < MAX_THREAD; i++) {
        int* p;
        if(isThreading) thread_create(&threads[i], NULL, multi, (void*)(p));
        else multi(p);
    }
 
    if(isThreading) {
        // joining and waiting for all threads to complete
        for (int i = 0; i < MAX_THREAD; i++)
            thread_join(threads[i], NULL);   
    }
}
 
// Driver Code
int main() {
    long start, end;
    start = getMicrotime();
    // Generating random values in matA and matB
    multiply(0);
    
    end = getMicrotime();
    double diff = (end - start)/1000000.0;
    printf("Total time required for multi threading : %lf\n", diff);

    start = getMicrotime();
    // Generating random values in matA and matB
    multiply(1);
    
    end = getMicrotime();
    diff = (end - start)/1000000.0;
    printf("Total time required for multi threading : %lf\n", diff);
    
    return 0;
}