# threadLib : Multi-Threading Library

## Overview:
threadLib is a multithreading library with three types of mapping models.</br>
**1. One-One**</br>
**2. Many-One**</br>
**3. Many-Many**</br>
threadLib can be used to achieve parllel flow of execution of user program. It is available for Unix-like POSIX operating systems.


## 1. One-One Model:
    Each user thread maps to one kernel thread. Kernel thread is created with clone system call.
    Newly created thread is inserted in the linked list of threads. 
    Each node of the linked list contains the metadata of corresponding thread.

<p align="center">
  <img 
    width="450"
    height="450"
    src="https://user-images.githubusercontent.com/61696982/167251262-0fa23612-5db8-4e1a-9fea-f6c9c21444a4.png"
  >
</p>


## 2. Many-One Model:
    All the user threads created by user will run on only one kernel thread.
    Scheduler is use to schedule the many user threads on one kernel thread. 
    Scheduler has its own context. Here main is also treated like other threads.

<p align="center">
  <img 
    align="center"
    width="450"
    height="450"
    src="https://user-images.githubusercontent.com/61696982/167251431-e1ead0ea-c334-43fa-8823-59a7de440e68.png"
  >
</p>

### Flow of execution:
        1. Suppose thread T1 is executing, after completing its time quantum it would receive an Timer Interrupt.
        2. User defined signal handler of corresponding thread T1 will be invoked.
        3. In signal handler the context of current thread is saved and we jump to Scheduler context.
        4. Scheduler finds a RUNNABLE process and jumps to its context.
        5. If newly selected thread has executed before for some time then the thread would resume execution in the 
           signal handler where context was stored previously in step 3. 
           Otherwise thread will start execution in wrapper function which is an argument to clone.
        6. This thread will execute for its time quantum and the same process will be repeated.

        Wrapper Function(execute_me in code):
                Helps in identifying when thread is over. 
                It simply calls the user function corresponding to thread.

<p align="center">
  <img 
    align="center"
    src="https://user-images.githubusercontent.com/61696982/167254634-64a9ec32-aee4-4fb6-872f-791a25db91a9.png"
  >
</p>


## 3. Many-Many Model:
        In this model 'm' user threads run on 'n' kernel threads.
        Each kernel thread has its own Scheduler i.e. there are 'n' instance of scheduler stored in the
        array (scheduler_node_array[] in code).
        There is single linked list of threads. Each contain metadata of corresponding thread. 
        If any of the kernel thread gets Timer Interrupt then that kernel thread will pick its next thread 
        from this linked list for execution.

<p align="center">
  <img 
    align="center"
    width="450"
    height="450"
    src="https://user-images.githubusercontent.com/61696982/167254772-a567c1b1-cd0f-4786-b580-33a372374499.png"
  >
</p>

### Flow of execution:
        1. If user creates new thread then each user thread will be created on seperate Kernel Thread 
           till NO_OF_KTHREADS(max k-threads) exceeds.
        2. If this limit exceeds, newly created thread will be inserted in the thread linked list for further execution.
        3. On Timer Interrupt(SIGALARM) user defined signal_handler gets invoked.
        NOTE: Here SIGALARM signal can interrupt any Kernel Thread(may not be the same by whome SIGALARM was set).
              To resolve this constraint, SIGALARM handler will take next Kernel Thread in round-robin fashion and 
              SIGVTALARM signal will be delivered to that specific Kernel Thread, 
              Now right Kernel Thread got interrupted to pick the next thread for its execution.
        4. Next flow of execution for each Kernel Thread will be the same as in Many-One Model flow of execution. 


