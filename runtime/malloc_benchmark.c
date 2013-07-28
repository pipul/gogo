#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "malloc.h"

#define BILLION 1000000000ULL
static int class = 16000;

void *glibc_benchmark(void *args) {
	struct timespec start, stop;
	double avgtime;
	int i, errcnt, size, allsize;
	int *memsizes;
	void **memptrs;

	memptrs = malloc((sizeof(void *) + sizeof(int)) * class * 2);
	if (!memptrs)
		pthread_exit(NULL);
	memsizes = (int *)(memptrs + class);

	if (clock_gettime(CLOCK_REALTIME, &start) == -1) {
		goto ERROR;
	}
	
	for (i = 0, errcnt = 0, allsize = 0; i < class; i++) {
		size = rand() % 32000;
		memptrs[i] = malloc(size);
	        if (!memptrs[i]) {
			errcnt++;
			continue;
		}
		memset(memptrs[i], 0, size);
		memsizes[i] = size;
		allsize += size;
	}

	for (i = 0; i < class; i++) {
		if (memptrs[i]) {
			free(memptrs[i]);
			memptrs[i] = NULL;
		}
	}

	if (clock_gettime(CLOCK_REALTIME, &stop) == -1) {
		goto ERROR;
	}

	avgtime = ((stop.tv_sec - start.tv_sec) * BILLION +
		   (stop.tv_nsec - start.tv_nsec)) / class;
	fprintf(stdout, "allsize %dm, errcnt %d, avgtime %lf\n",
		(allsize/1024)/1024, errcnt, avgtime);
	
	pthread_exit(NULL);
 ERROR:
	free(memptrs);
	pthread_exit(NULL);
}


void *mcache_benchmark(void *args) {
	struct timespec start, stop;
	struct mcache *mc;
	int i, size, errcnt, allsize;
	int *memsizes;
	void **memptrs;
	double avgtime;

	memptrs = malloc((sizeof(void *) + sizeof(int)) * class * 2);
	if (!memptrs)
		pthread_exit(NULL);
	memsizes = (int *)(memptrs + class);

	
	mc = mheap_mcache_create(&runtime_mheap);

	if (clock_gettime(CLOCK_REALTIME, &start) == -1)
		goto ERROR;

	for (i = 0, errcnt = 0, allsize = 0; i < class; i++) {
		size = rand() % 32000;
		memptrs[i] = mcache_alloc(mc, size, 1);
		if (!memptrs[i]) {
			errcnt++;
			continue;
		}
		memsizes[i] = size;
		allsize += size;
	}

	for (i = 0; i < class; i++) {
		if (!memptrs[i])
			continue;
		mcache_free(mc, memptrs[i], memsizes[i]);
	}

	if (clock_gettime(CLOCK_REALTIME, &stop) == -1)
		goto ERROR;
	avgtime = ((stop.tv_sec - start.tv_sec) * BILLION +
		   (stop.tv_nsec - start.tv_nsec)) / class;
	fprintf(stdout, "allsize %dm, errcnt %d, avgtime %lf\n",
		(allsize/1024)/1024, errcnt, avgtime);
	
 ERROR:
	free(memptrs);
	mheap_mcache_destroy(&runtime_mheap, mc);
	pthread_exit(NULL);
}


void *myalloc(int size) {
	return malloc(size);
}

void myfree(void *ptr) {
	return free(ptr);
}

int benchmark(int argc, char **argv) {
	int i, threads = 50;
	pthread_t *thread;
	void *status;
	struct timespec seed;
	
	if (argc < 2)
		return;
	
	mheap_init(&runtime_mheap, myalloc, myfree);

	if (argc > 2)
		class = atoi(argv[2]);
	if (argc > 3)
		threads = atoi(argv[3]);
	thread = malloc(threads * sizeof(*thread));

	clock_gettime(CLOCK_REALTIME, &seed);
	srand((unsigned int)(seed.tv_sec + seed.tv_nsec));
	
	if (strcmp(argv[1], "glibc") == 0) {
		for (i = 0; i < threads; i++) {
			pthread_create(&thread[i], NULL, glibc_benchmark, NULL);
		}
	} else if (strcmp(argv[1], "mcache") == 0) {
		for (i = 0; i < threads; i++) {
			pthread_create(&thread[i], NULL, mcache_benchmark, NULL);
		}
	}

	for (i = 0; i < threads; i++) {
		pthread_join(thread[i], &status);
	}
	printf("mas sizeclass %d\n", max_size_class);
	free(thread);
	mheap_stat(&runtime_mheap);
	mheap_exit(&runtime_mheap);
}

int main(int argc, char **argv) {
	benchmark(argc, argv);
}



