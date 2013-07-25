/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */


// spinlock
#include <pthread.h>

struct spinlock {
	pthread_spinlock_t rawLock;
};

extern void spinlock_init(struct spinlock *lock);
extern void spinlock_destroy(struct spinlock *lock);
extern void spin_lock(struct spinlock *lock);
extern void spin_unlock(struct spinlock *lock);
