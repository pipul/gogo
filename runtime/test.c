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
	msize_init();
	for (i = 0; i < NUM_SIZE_CLASSES; i++) {
		printf("sizeclass %d info: %d %d %d\n", i, class_to_size[i],
		       class_to_allocnpages[i], class_to_transfercount[i]);
	}

	int size, class;
	int maxsize = 10000;
	for (i = 0; i < maxsize; i++) {
		size = rand() % MAX_SMALL_SIZE;
		class = size_class(size);
		printf("%d of class: %d %d\n", size, class, class_to_size[class]);
	}
}

void *myalloc(int size) {
	return malloc(size);
}

void myfree(void *ptr) {
	printf("free addr:%ld\n", ptr);
	free(ptr);
}

void test_mem(void *args) {
	struct mcache *mc;
	int i, size;
	void *ptr;

	mheap_init(&runtime_mheap, myalloc, myfree);
	mc = mheap_mcache_create(&runtime_mheap);
	for (i = 0; i < 1000; i++) {
		size = rand() % (MAX_SMALL_SIZE);
		ptr = mcache_alloc(mc, size, 1);
		if (!ptr) {
			printf("-ENOMEM of %d\n", size);
			continue;
		}
		printf("malloc: %ld %d\n", ptr, size);
		mcache_free(mc, ptr, size);
	}
	mheap_mcache_destroy(&runtime_mheap, mc);
	mheap_exit(&runtime_mheap);
	return;
}

int task_main(struct task_args *args) {
	gogo(test_main, NULL);
	gogo(test_msize, NULL);
	gogo(test_mem, NULL);
	return 0;
}


