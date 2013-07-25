/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */



#include <pthread.h>


typedef struct spinlock {
	pthread_spinlock_t Lock;
} spinlock_t;

void spinlock_init(struct spinlock *lock) {
	pthread_spin_init(&lock->Lock, 0);
}

void spin_lock(struct spinlock *lock) {
	pthread_spin_lock(&lock->Lock);
}

void spin_unlock(struct spinlock *lock) {
	pthread_spin_unlock(&lock->Lock);
}
