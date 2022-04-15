#ifndef LOCK_H
#define LOCK_H

// Mutual exclusion lock.
typedef struct spinlock {
	int locked;       // Is the lock held?
	thread_id tid;
}spinlock;

void initlock(struct spinlock *lk);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);

#endif
