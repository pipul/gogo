/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */



#include "task.h"
#include "runtime.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef VALGRIND
#include <valgrind/valgrind.h>
#endif

struct TaskQueue {
	// Lock must be the first field
	struct spinlock Lock;
	struct list_head queue;
};

static struct TaskQueue taskqueue;

static void taskqueue_init(struct TaskQueue *tq) {
	spinlock_init(tq);
	INIT_LIST_HEAD(&tq->queue);
}

static void taskqueue_exit(struct TaskQueue *tq) {
	spinlock_destroy(tq);
}

static struct TaskQueue *taskqueue_alloc(void) {
	struct TaskQueue *tq;

	tq = malloc(sizeof(*tq));
	if (!tq)
		return NULL;
	memset(tq, 0, sizeof(*tq));
	taskqueue_init(tq);
	return tq;
}

static void taskqueue_free(struct TaskQueue *tq) {
	taskqueue_exit(tq);
	free(tq);
}

static int taskqueue_push(struct TaskQueue *tq, struct task *t) {
	spin_lock(tq);
	list_add(&t->alllink, &tq->queue);
	spin_unlock(tq);
	return 0;
}

static struct task *taskqueue_pop(struct TaskQueue *tq) {
	struct task *t;

	spin_lock(tq);
	if (list_empty(&tq->queue)) {
		spin_unlock(tq);
		return NULL;
	}
	t = list_first(&tq->queue, struct task, alllink);
	list_del(&t->alllink);
	spin_unlock(tq);
	return t;
}






struct thread {
	pthread_t pid;
	ucontext_t ucp;
	void (*mainfunc)(void *args);
	void *args;
	void *stackguard;
	int stacksize;
	struct task *task0; // current running task on this thread;

	struct list_head alllink;
};

struct ThreadQueue {
	// Lock must be the first field
	struct spinlock Lock;
	struct list_head queue;
};

struct ThreadQueue threadqueue;

static void threadqueue_init(struct ThreadQueue *tq) {
	spinlock_init(tq);
	INIT_LIST_HEAD(&tq->queue);
}


static void threadqueue_exit(struct ThreadQueue *tq) {
	spinlock_destroy(tq);
}

static struct ThreadQueue *threadqueue_alloc() {
	struct ThreadQueue *tq;

	if (!(tq = malloc(sizeof(*tq))))
		return NULL;
	memset(tq, 0, sizeof(*tq));
	threadqueue_init(tq);
	return tq;
}

static void threadqueue_free(struct ThreadQueue *tq) {
	threadqueue_exit(tq);
	free(tq);
}

static struct thread *threadqueue_pop(struct ThreadQueue *tq) {
	struct thread *t;

	spin_lock(tq);
	if (list_empty(&tq->queue)) {
	        spin_unlock(tq);
		return NULL;
	}
	t = list_first(&tq->queue, struct thread, alllink);
	list_del(&t->alllink);
	spin_unlock(tq);
	return t;
}

static void threadqueue_push(struct ThreadQueue *tq, struct thread *t) {
	spin_lock(tq);
	list_add(&t->alllink, &tq->queue);
	spin_unlock(tq);
}



static int task_idgen;
static pthread_key_t thread_key;

static struct thread *thread_alloc(void (*mainfunc)(void *args), void *args) {
	struct thread *thread;

	if (!(thread = malloc(sizeof(*thread))))
		return NULL;
	memset(thread, 0, sizeof(*thread));
	thread->mainfunc = mainfunc;
	thread->args = args;
	return thread;
}


static void thread_free(struct thread *thread) {
	free(thread);
}

static void thread_create(void (*mainfunc)(void *args), void *args, int stacksize) {

	struct thread *thread;
	pthread_attr_t pth_attr;

	if (0 != pthread_attr_init(&pth_attr)) {
		fprintf(stderr, "pthread_attr_init failed\n");
		return;
	}
	if (stacksize > 0 && 0 != pthread_attr_setstacksize(&pth_attr, stacksize)) {
		fprintf(stderr, "pthread_attr_setstacksize failed\n");
		goto STACKSIZE_ERROR;
	}
	// pthread_attr_setstack(&pth_attr, stack, PTHREAD_STACK_MIN);
	thread = thread_alloc(mainfunc, args);
	if (!thread) {
		fprintf(stderr, "thread_alloc failed\n");
		goto MEM_ERROR;
	}
	threadqueue_push(&threadqueue, thread);
	if (0 != pthread_create(&thread->pid, &pth_attr, (void *(*)(void *))mainfunc, thread)) {
		fprintf(stderr, "pthread_create failed\n");
		goto PTHREAD_ERROR;
	}
	pthread_attr_destroy(&pth_attr);
	return;

 PTHREAD_ERROR:
	thread_free(thread);
 MEM_ERROR:
 STACKSIZE_ERROR:
	pthread_attr_destroy(&pth_attr);
	return;
}





static void task_switch(ucontext_t *from, ucontext_t *to) {
	if (swapcontext(from, to) < 0) {
		fprintf(stderr, "swapcontext failed\n");
		BUG_ON();
	}
}


static void task_exit(struct task *t) {
	struct thread *thread;

	t->status = TASK_STOPPED;
	if (!(thread = pthread_getspecific(thread_key)))
		BUG_ON();
	task_switch(&t->ucp, &thread->ucp);
}

static void task_start(void *arg) {
	struct task *t = arg;

	t->mainfunc(t->args);
	task_exit(t);
}


static struct task *task_alloc(void (*mainfunc)(void *args), void *args, int stacksize) {
	struct task *t;

	t = malloc(sizeof(*t) + stacksize);
	if (!t) {
		fprintf(stderr, "coroutine alloc error\n");
		return NULL;
	}
	memset(t, 0, sizeof(*t));
	memset(&t->ucp, 0, sizeof(t->ucp));

	if (getcontext(&t->ucp) < 0) {
		fprintf(stderr, "coroutine getcontext error\n");
		free(t);
		return NULL;
	}

	t->tid = ++task_idgen;
	t->mainfunc = mainfunc;
	t->args = args;

	t->ucp.uc_stack.ss_sp = t->stackguard = (void *)(t + 1);
	t->ucp.uc_stack.ss_size = t->stacksize = stacksize;
#ifdef VALGRIND
	VALGRIND_STACK_REGISTER(t->stackguard, t->stackguard + t->stacksize);
#endif
	makecontext(&t->ucp, (void (*)())task_start, 1, t);

	return t;
}

static void task_free(struct task *t) {
	free(t);
}


int task_create(void (*mainfunc)(void *arg), void *arg, int stacksize) {
	struct task *t = task_alloc(mainfunc,
				    arg, stacksize == 0 ? TASK_STACK_DEFAULT : stacksize);

	if (!t)
		return -1;
	return taskqueue_push(&taskqueue, t);
}


int task_yield(void) {
	struct task *t;
	struct thread *thread;

	if (!(thread = pthread_getspecific(thread_key))) {
		BUG_ON();
	}
	t = thread->task0;
	thread->task0 = NULL;
	t->status = TASK_WAITING;
	
	taskqueue_push(&taskqueue, t);
	task_switch(&t->ucp, &thread->ucp);

	return 0;
}


static void task_schedule(void) {
	struct task *t;
	struct thread *thread;

	if (!(thread = pthread_getspecific(thread_key)))
		BUG_ON();

	for (;;) {
		t = taskqueue_pop(&taskqueue);
		if (!t) {
			fprintf(stderr, "no runnable tasks!\n");
			return;
		}
		fprintf(stdout, "runnning task: %lu %d\n", thread->pid, t->tid);
		t->status = TASK_RUNNING;
		thread->task0 = t;
		task_switch(&thread->ucp, &t->ucp);

		// back in scheduler
		if (t->status == TASK_STOPPED) {
			task_free(t);
		}
	}
}

static void thread_start(void *args) {
	struct thread *thread = args;

	fprintf(stdout, "thread %lu start\n", thread->pid);
	pthread_setspecific(thread_key, thread);
	task_schedule();
	fprintf(stdout, "thread %lu exit\n", thread->pid);
	pthread_exit(NULL);
	return;
}


int main(int argc, char **argv) {

	int ret;
	void *status;
	struct thread *thread;
	struct task_args args = {argc, argv};


	// initial the global taskqueue and threadqueue
	taskqueue_init(&taskqueue);
	threadqueue_init(&threadqueue);

	// initialized mheap
	mheap_init();
	
	if ((ret = pthread_key_create(&thread_key, NULL)) != 0) {
		fprintf(stderr, "pthread_key_create failed\n");
		exit(1);
	}

	// Warning! must create the first task for task_main function
	// before startup the other threads.
	if ((ret = task_create((void (*)())task_main, &args, 8096)) != 0) {
		fprintf(stderr, "mainfunc_task_create failed\n");
		exit(1);
	}

	// here, fine!
	// is ok to start up more backend threads to process the task
	thread_create(thread_start, NULL, PTHREAD_STACK_MIN);

	// init the mock pthread environment for current process
	// take participate in the task_schedule and then free the
	// mock data if done.
	thread = thread_alloc(thread_start, NULL);
	pthread_setspecific(thread_key, thread);
	task_schedule();
	thread_free(thread);


	// wait for all other threads exit
	for (;;) {
		thread = threadqueue_pop(&threadqueue);
		if (!thread) {
			fprintf(stdout, "no runnable threads\n");
			break;
		}
		pthread_join(thread->pid, &status);
		thread_free(thread);
	}

	pthread_exit(NULL);
	taskqueue_exit(&taskqueue);
	threadqueue_exit(&threadqueue);
	
	return 0;
}
