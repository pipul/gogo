/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */

// Memory allocator, base on jemalloc and tcmalloc
// http://goog-perftools.sourceforge.net/doc/tcmalloc.html
// http://www.canonware.com/jemalloc/

// The allocator's data structures are:
//     mheap_t: the malloc heap, managed at page(4098-byte) granularity.
//     mspan_t: a run of pages managed by the mheap_t.
//     mcentral_t: a shared free list for a given size class.
//     mcache_t: a per-thread cache for small objects.
//     mstat_t: allocation statistics, for performance tuning.


// Allocating a small object proceeds up a hierarchy of caches:
//
//	1. Round the size up to one of the small size classes
//	   and look in the corresponding mcache_t free list.
//	   If the list is not empty, allocate an object from it.
//	   This can all be done without acquiring a lock.
//
//	2. If the mcache_t free list is empty, replenish it by
//	   taking a bunch of objects from the mcentral free list.
//	   Moving a bunch amortizes the cost of acquiring the marena
//         lock.
//
//	3. If the marena_t free list is empty, replenish it by
//	   allocating a run of pages from the mheap and then
//	   chopping that memory into a objects of the given size.
//	   Allocating many objects amortizes the cost of locking
//	   the heap.
//
//	4. If the mheap is empty or has no page runs large enough,
//	   allocate a new group of pages (at least 1MB) from the
//	   operating system.  Allocating a large run of pages
//	   amortizes the cost of talking to the operating system.
//
// Freeing a small object proceeds up the same hierarchy:
//
//	1. Look up the size class for the object and add it to
//	   the mcache_t free list.
//
//	2. If the mcache_t free list is too long or the mcache_t has
//	   too much memory, return some to the marena free lists.
//
//	3. If all the objects in a given span have returned to
//	   the marena list, return that span to the page heap.
//
// Allocating and freeing a large object uses the page heap
// directly, bypassing the mcache_t and marena free lists.
//
// The small objects on the mcache_t and marena free lists
// may or may not be zeroed.  They are zeroed if and only if
// the second word of the object is zero.  The spans in the
// page heap are always zeroed.  When a span full of objects
// is returned to the page heap, the objects that need to be
// are zeroed first.  There are two main benefits to delaying the
// zeroing this way:
//
//	1. stack frames allocated from the small object lists
//	   can avoid zeroing altogether.
//	2. the cost of zeroing when reusing a small object is
//	   charged to the mutator, not the garbage collector.
//

#include "runtime.h"
#include "list.h"
#include <string.h>
#include <stdio.h>

enum {
	PAGESHIFT = 12,
	PAGESIZE = 1 << PAGESHIFT,
	PAGEMASK = PAGESIZE - 1,
};


enum {
	// Computed constant.  The definition of MaxSmallSize and the
	// algorithm in msize.c produce some number of different allocation
	// size classes.  NumSizeClasses is that number.  It's needed here
	// because there are static arrays of this length; when msize runs its
	// size choosing algorithm it double-checks that NumSizeClasses agrees.
	NUM_SIZE_CLASSES = 61,

	// Tunable constants.
	MAX_SMALL_SIZE = 32<<10, // 32k

	// Default size = 256
	// Maximum page length for fixed-size list in mheap
	MAX_MHEAP_LIST = 1<<(20 - PAGESHIFT),

	// Default size = 1m (1ULL << 20)
	// Chunk size for heap growth.
	MHEAP_CHUNK_GROW = MAX_MHEAP_LIST,


	// todo: at most use 4G memory now...
	MHEAPMAP_BITS = 32 - PAGESHIFT,


};

#if defined (__LP64__) || defined (__64BIT__) || defined (_LP64) || (__WORDSIZE == 64)
#define	CACHE_LINE_SIZE 64
#else
#define	CACHE_LINE_SIZE 32
#endif



// Size classes. Computed and initialized by init_msize()
//
// size_to_class(0 <= n <= MAX_SMALL_SIZE) returns the size class.
//     1 <= sizeclass < NUM_SIZE_CLASS, for n.
//     size class 0 is reserved to mean "not small"
// class_to_size[i] = largest size in class i
// class_to_allocnpages[i] = number of pages to allocate when makeing
//     new objects in class i.
// class_to_transfercount[i] = number of objects to move when taking
//     a bunch of objects out of the marena lists and putting them in
//     the thread local cache list.


void msize_init(void);

int size_class(int size);
extern int max_size_class; // index from 0
extern int max_small_size;
extern int class_to_size[NUM_SIZE_CLASSES];
extern int class_to_allocnpages[NUM_SIZE_CLASSES];
extern int class_to_transfercount[NUM_SIZE_CLASSES];
void size_class_info(int sizeclass, int *size, int *npages, int *nobjs);




#define MaxMem (1ULL<<(MHEAPMAP_BITS))


// A generic linked list of blocks
struct mlink {
	struct mlink *next;
};


// mem interface of os_arch dependent
void *sys_alloc(int size);
void *sys_alloc2(void *ptr, int size);
void sys_free(void *ptr, int size);


// fixmem is a simple free-list allocator for fixed size objects
// mheap uses a fixmem wrapped around sys_alloc to manages its
// mcache and mspan objects.

// Warning! memory returned by fixmem is not zerod.
struct fixmem {
	int size;
	void *(*alloc)(int);
	void (*free)(void *);
	struct mlink *freelist;
};

void fixmem_init(struct fixmem *fm, int size,
		 void *(*allocator)(int), void (*free)(void *));
void fixmem_exit(struct fixmem *fm);
void *fixmem_alloc(struct fixmem *fm);
void fixmem_free(struct fixmem *fm, void *ptr);



struct mspan {
	long pageid;                 // starting page number
	int npages;                  // number of pages in span
	int ref;                     // number of allocated objects in this span
	struct mlink *freelist;      // list of free objects
	struct list_head alllink;    // in a span linked list
};

void mspan_init(struct mspan *span, long pageid, int npages);


/*

int address_space_init(struct address_space *space, void *low, int npage);
void address_space_exit(struct address_space *space);

// helper function for mmap
struct mspan *address_space_alloc(struct address_space *space, int npages);
void address_space_free(struct address_space *space, struct mspan *span);

// split one mspan into two, the origin span contain npage page
// and the new mspan contain all the rest page
struct mspan *address_space_split(struct address_space *space, struct mspan *span, int npage);
static inline struct mspan *address_space_lookup(struct address_space *space, void *ptr) {
	int idx;

	idx = (ptr - space->low) >> PAGESHIFT;
	return space->map.data[idx];
}

static inline void address_space_stat(struct address_space *space) {
	fprintf(stdout, "mempage statistics %d/%d\n", space->allocpages, space->freepages);
}

*/

struct marena {
	// Lock must be the first field
	struct spinlock Lock;
	int sizeclass;
	int elemsize;

	struct list_head empty;
	struct list_head nonempty;


	// for statistics
	int cachemiss;
	int cachehit;
};

void marena_init(struct marena *arena, int sizeclass);
int marena_alloclist(struct marena *arena, int n, struct mlink **first);
void marena_freelist(struct marena *arena, struct mlink *first);
void marena_freespan(struct marena *arena, struct mspan *span,
		     int n, struct mlink *start, struct mlink *end);

struct mheap {
	// Lock must be the first field
	struct spinlock Lock;
	struct list_head free[MAX_MHEAP_LIST];
	struct list_head large;

	//struct mspan *map[1<<MHEAPMAP_BITS];
	struct mspan **map;
	//struct address_space map;
	union {
		struct marena __raw;
		char pad[CACHE_LINE_SIZE];
	} arenas[NUM_SIZE_CLASSES];

	struct fixmem mspancache;    // allocator for mspan*
	struct fixmem mcachecache;   // allocator for mcache*

	// for statistics
	int cachemiss;
	int cachehit;
};


extern struct mheap runtime_mheap;

void mheap_init(struct mheap *heap,
		void *(*allocator)(int), void (*free)(void *));
void mheap_exit(struct mheap *heap);
struct mspan *mheap_alloc(struct mheap *heap, int npages, int zerod);
void mheap_free(struct mheap *heap, struct mspan *span);
struct mspan *mheap_lookup(struct mheap *heap, void *ptr);
void mheap_stat(struct mheap *heap);









// Per-thread cache for small objects. no locking needed
// because it is per-thread data.

struct mcache {
	int nelem[NUM_SIZE_CLASSES];
	struct mlink *list[NUM_SIZE_CLASSES];
};

void mcache_init(struct mcache *mc);
void *mcache_alloc(struct mcache *mc, int size, int zeroed);
void mcache_free(struct mcache *mc, void *p, int size);


struct mcache *mheap_mcache_create(struct mheap *heap);
void mheap_mcache_destroy(struct mheap *heap, struct mcache *mc);
