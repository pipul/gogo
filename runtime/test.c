/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "task.h"
#include "malloc.h"


void test_main(void *args) {
	printf("Hello word.\n");
}

void test_msize(void *args) {

	int i;
	for (i = 0; i <= max_size_class; i++) {
		printf("sizeclass %d info: %d %d %d\n", i, class_to_size[i],
		       class_to_allocnpages[i], class_to_transfercount[i]);
	}
}

void test_sizeclass(void *args) {
	int size, class;
	int maxsize = max_small_size;
	for (size = 8; size <= maxsize; size += 8) {
		class = size_class(size);
		printf("%d of class: %d %d\n", size, class, class_to_size[class]);
	}
}

static int myallocn;
static int myfreen;
void *myalloc(int size) {
	myallocn++;
	return malloc(size);
}

void myfree(void *ptr) {
	printf("free addr:%ld\n", ptr);
	myfreen++;
	free(ptr);
}

void test_mem(void *args) {
	struct mcache *mc;
	int i, size;
	void *ptr;
	void **memmap;

	mheap_init(&runtime_mheap, myalloc, myfree);
	mc = mheap_mcache_create(&runtime_mheap);
	memmap = malloc(sizeof(void *) * max_small_size);
	if (!memmap)
		BUG_ON();
	memset(memmap, 0, sizeof(void *) * max_small_size);
	for (size = 1; size <= max_small_size; size++) {
		ptr = mcache_alloc(mc, size, 1);
		if (!ptr) {
			printf("-ENOMEM of %d\n", size);
			break;
		}
		printf("malloc: %ld %d\n", ptr, size);
		memmap[size - 1] = ptr;
	}
	for (size -= 1; size; size--) {
		mcache_free(mc, memmap[size - 1], size);
	}
	free(memmap);
	mheap_mcache_destroy(&runtime_mheap, mc);
	mheap_exit(&runtime_mheap);
	if (myallocn != myfreen) {
		fprintf(stdout, "%d alloc, %d free\n", myallocn, myfreen);
		BUG_ON();
	}
	return;
}


void test_gogo_foo(void *args) {
	args++;
	//printf("test_gogo_foo ...\n");
	return;
}

void test_gogo(void *args) {
	int i, limit = 1000000;
	double ct;
	struct timespec start, end;

	clock_gettime(CLOCK_REALTIME, &start);
	for (i = 0; i < limit; i++) {
		gogo(test_gogo_foo, NULL);
	}
	clock_gettime(CLOCK_REALTIME, &end);
	ct = (end.tv_sec - start.tv_sec) * 1000000000ULL + end.tv_nsec - start.tv_nsec;
	fprintf(stdout, "building %d thread cost: %lld\n", limit, ct);
}

int task_main(struct task_args *args) {
	gogo(test_main, NULL);
	//gogo(test_msize, NULL);
	//gogo(test_sizeclass, NULL);
	//gogo(test_mem, NULL);
	gogo(test_gogo, NULL);
	return 0;
}


