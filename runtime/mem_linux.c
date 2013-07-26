/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */

#include <sys/mman.h>
#include "malloc.h"



// Alloc memory from kernel useing mmap
void *sys_alloc(int size) {
	void *ptr;

	ptr = mmap(NULL, size, PROT_READ|PROT_WRITE|PROT_EXEC,
		   MAP_ANON|MAP_PRIVATE|MAP_32BIT, -1, 0);
	if (ptr == (void *) -1)
		ptr = NULL;
	return ptr;
}

// Free memory
void sys_free(void *ptr, int size) {
	munmap(ptr, size);
}
