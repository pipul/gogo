/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */


#ifndef _TASK_H_
#define _TASK_H_

#include <unistd.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <ucontext.h>

#define TASK_RUNNING 0x0001
#define TASK_STOPPED 0x0002
#define TASK_WAITING 0x0004

struct task_args {
	int c;
	char **v;
};

typedef struct task {
	int status;
	int tid;
	ucontext_t ucp;
	void (*mainfunc)(void *args);
	void *args;
	void *stackguard;
	int stacksize;
	LIST_ENTRY(task) alllink;   // on all coroutine
} task_t;

#define BUG_ON(x...) abort()

#define TASK_STACK_DEFAULT 8192

int task_create(void (*mainfunc)(void *args), void *args, int stacksize);
int task_yield(void);
int task_main(struct task_args *args);


#define gogo(func, arg) do {\
	task_create(func, arg, TASK_STACK_DEFAULT); \
} while (0)




#endif /* _TASK_H_ */
