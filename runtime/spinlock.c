/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */



#include "runtime.h"


void spinlock_init(void *lock) {
	pthread_spin_init(&((struct spinlock *)lock)->rawLock, 0);
}

void spinlock_destroy(void *lock) {
	pthread_spin_destroy(&((struct spinlock *)lock)->rawLock);
}

void spin_lock(void *lock) {
	pthread_spin_lock(&((struct spinlock *)lock)->rawLock);
}

void spin_unlock(void *lock) {
	pthread_spin_unlock(&((struct spinlock *)lock)->rawLock);
}
