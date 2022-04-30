#ifndef LOCK_H
#define LOCK_H

// Mutual exclusion lock.
typedef struct spinlock {
	int locked;       // Is the lock held?
	thread_id tid;
}spinlock;

// Long-term locks for processes
typedef struct sleeplock {
  	int locked;       // Is the lock held?
	spinlock lk; // spinlock protecting this sleep lock
	thread_id tid;  //tid of thread holding this lock
}sleeplock;

void initlock(struct spinlock *lk);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);

void initsleeplock(struct sleeplock *lk);
void acquiresleep(struct sleeplock *lk);
void releasesleep(struct sleeplock *lk);

#endif
