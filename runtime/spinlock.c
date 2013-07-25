/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */



#include "runtime.h"

/*
typedef struct spinlock {
	pthread_spinlock_t Lock;
} spinlock_t;
*/

void spinlock_init(struct spinlock *lock) {
	pthread_spin_init(&lock->rawLock, 0);
}

void spinlock_destroy(struct spinlock *lock) {
	pthread_spin_destroy(&lock->rawLock);
}

void spin_lock(struct spinlock *lock) {
	pthread_spin_lock(&lock->rawLock);
}

void spin_unlock(struct spinlock *lock) {
	pthread_spin_unlock(&lock->rawLock);
}
