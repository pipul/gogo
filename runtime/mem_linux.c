/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */

#include <sys/mman.h>
#include "malloc.h"



// Alloc memory from kernel useing mmap
void *sys_alloc(int size) {
	return sys_alloc2(NULL, size);
}

void *sys_alloc2(void *ptr, int size) {
	void *ptr2;
	
	ptr2 = mmap(ptr, size, PROT_READ|PROT_WRITE|PROT_EXEC,
		   MAP_ANON|MAP_PRIVATE|MAP_32BIT, -1, 0);
	if ((ptr == NULL && ptr2 == (void *)-1) || (ptr != NULL && ptr2 != ptr))
		ptr2 = NULL;
	return ptr2;
}


// Free memory
void sys_free(void *ptr, int size) {
	munmap(ptr, size);
}
