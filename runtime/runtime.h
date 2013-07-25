/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */


// spinlock
struct spinlock;

extern void spinlock_init(struct spinlock *lock);
extern void spin_lock(struct spinlock *lock);
extern void spin_unlock(struct spinlock *lock);
