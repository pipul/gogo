/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */

#include "malloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define BUG_ON(x...) abort();

struct mheap runtime_mheap;
int max_size_class = 0;
int class_to_size[NUM_SIZE_CLASSES] = {};
int class_to_allocnpages[NUM_SIZE_CLASSES] = {};
int class_to_transfercount[NUM_SIZE_CLASSES] = {};


// The size_to_class lookup is implemented using two arrays.
// one mapping sizes <= 1024 to their class and one mapping
// sizes >= 1024 and <= MAX_SMALL_SIZE to their class.
// All objects are 8-aligned, so the first array is indexed by
// the size divided by 8 (rouded up). Objects >= 1024 bytes are
// 128-aligned, so the second array is indexed by the size
// divided by 128 (rouded up). The arrays are filled in by
// init_msize() function.


static int size_to_class[MAX_SMALL_SIZE / 8];

#define rounded_up(size, align) ((size + align - 1) & ~align)
#define rounded_up8(size) rounded_up(size, 8)

int size_class(int size) {
	if (size > MAX_SMALL_SIZE)
		return -1;
	return size_to_class[(rounded_up8(size) >> 3) - 1];
}


void msize_init(void) {
	int align, sizeclass, size;
	int allocsize, npages;

	/*
	class_to_size[0] = 0;
	class_to_allocnpages[0] = 1;
	class_to_transfercount[0] = 1;
	sizeclass = 1;
	*/
	sizeclass = 0;
	align = 8;

	for (size = align; size <= MAX_SMALL_SIZE; size += align) {
		// bump alignment once in a while
		if ((size & (size - 1)) == 0) {
			if (size >= 2048)
				align = 256;
			else if (size >= 128)
				align = size / 8;
			else if (size >= 16)
				align = 16;
		}

		fprintf(stdout, "init size class: %d %d %d\n", sizeclass, size, align);
		// todo:
		// Make the allocnpages big enough that
		// the leftover is less than 1/8 of the total.
		// so wasted space is at most 12.5%.
		
		allocsize = PAGESIZE;
		while (allocsize % size > allocsize/8)
			allocsize += PAGESIZE;
		npages = allocsize >> PAGESHIFT;

		if (sizeclass > 1 &&
		    npages == class_to_allocnpages[sizeclass - 1] &&
		    allocsize/size == allocsize/class_to_size[sizeclass - 1]) {
			class_to_size[sizeclass - 1] = size;
			continue;
		}

		class_to_size[sizeclass] = size;
		class_to_allocnpages[sizeclass] = npages;

		// todo:
		// fix the number of transfercount
		class_to_transfercount[sizeclass] = 2;
		sizeclass++;
	}
	max_size_class = sizeclass - 1;

	// initialize the size_to_class table, start from 1.
	for (sizeclass = 1; sizeclass <= max_size_class; sizeclass++) {
		size = class_to_size[sizeclass];
		do {
			size_to_class[(size >> 3) - 1] = sizeclass;
			size -= 8;
		} while (size > class_to_size[sizeclass - 1]);
	}
	
	return;
}


void size_class_info(int sizeclass, int *size, int *npages, int *nobjs) {
	if (sizeclass > max_size_class)
		goto ERROR;
	*size = class_to_size[sizeclass];
	*npages = class_to_allocnpages[sizeclass];
	*nobjs = (*npages << PAGESHIFT) / *size;
	return;
 ERROR:
	*size = -1;
	*npages = -1;
	*nobjs = -1;
	return;
}


void fixmem_init(struct fixmem *fm, int size,
		 void *(*allocator)(int), void (*free)(void *)) {
	fm->size = size;
	fm->alloc = allocator;
	fm->free = free;
	fm->freelist = NULL;
}

void fixmem_exit(struct fixmem *fm) {
	struct mlink *link, *next;

	for (link = fm->freelist; link; link = next) {
		next = link->next;
		fm->free(link);
	}
	fm->size = 0;
	fm->alloc = NULL;
	fm->free = NULL;
	return;
}


// todo: cache the freed mem back into freelist and
// then reused later wheh fixmem_alloc is called.
void *fixmem_alloc(struct fixmem *fm) {
	struct mlink *list;
	
	if (fm->freelist) {
		list = fm->freelist;
		fm->freelist = list->next;
		return list;
	}
	return fm->alloc(fm->size);
}

void fixmem_free(struct fixmem *fm, void *ptr) {
	struct mlink *list = ptr;

	list->next = fm->freelist;
	fm->freelist = list;
}


// mspan

void mspan_init(struct mspan *span, long pageid, int npages) {

	// fix for LIST_ENTRY_INIT()
	memset(span, 0, sizeof(*span));

	span->pageid = pageid;
	span->npages = npages;
	span->freelist = NULL;
	span->ref = 0;
	//span->sizeclass = NULL;
	//span->elemsize = 0;
}



// marena

static int marena_grow(struct marena *arena);
//static void *marena_alloc(struct marena *arena);
static void marena_free(struct marena *arena, void *ptr);

// Initialize a simgle arena free list
void marena_init(struct marena *arena, int sizeclass) {
	spinlock_init(arena);
	arena->sizeclass = sizeclass;
	arena->elemsize = class_to_size[sizeclass];
	INIT_LIST_HEAD(&arena->empty);
	INIT_LIST_HEAD(&arena->nonempty);
}


// Allocate up to n objects from the arena free list.
// Return the number of objects allocated. the objects are linked
// together by their first words.
int marena_alloclist(struct marena *arena, int n, struct mlink **pfirst) {
	struct mspan *span;
	struct mlink *first, *last;
	int cap, avail, i;

	spin_lock(arena);
	if (list_empty(&arena->nonempty)) {
		// allocate more memory from heap
		if (marena_grow(arena)) {
			spin_unlock(arena);
			*pfirst = NULL;
			return 0;
		}
	}

	span = list_entry(list_head(&arena->nonempty), struct mspan, alllink);
	cap = (span->npages << PAGESHIFT) / arena->elemsize;
	if ((avail = cap - span->ref) < n)
		n = avail;

	// First one is guaranteed to work, because we just grew the list.
	first = span->freelist;
	last = first;
	for (i = 1; i < n; i++) {
		last = last->next;
	}
	span->freelist = last->next;
	last->next = NULL;
	span->ref += n;

	// Maybe this span was empty if all avail is inused
	if (n == avail) {
		if (span->freelist != NULL || span->ref != cap) {
			// invalid mspan
			BUG_ON();
		}
		list_move(&span->alllink, &arena->empty);
	}

	spin_unlock(arena);

	*pfirst = first;
	return n;
}

// Helper function for free one object back into the arena fre list.
static void marena_free(struct marena *arena, void *ptr) {
	struct mspan *span;
	struct mlink *plink;

	// Find span for ptr
	span = mheap_lookup(&runtime_mheap, ptr);
	if (span == NULL || span->ref == 0)
		BUG_ON(); // invalid free

	// Move to nonempty if necessary
	if (span->freelist == NULL) {
		list_move(&span->alllink, &arena->nonempty);
	}

	plink = ptr;
	plink->next = span->freelist;
	span->freelist = plink;

	// Move span back to heap if it is completely freed.
	if (--span->ref == 0) {
		list_del(&span->alllink);
		span->freelist = NULL;
		mheap_free(&runtime_mheap, span);
	}
}

void marena_freelist(struct marena *arena, struct mlink *first) {
	struct mlink *v, *next;
	
	spin_lock(arena);
	for (v = first; v; v = next) {
		next = v->next;
		marena_free(arena, v);
	}
	spin_unlock(arena);
}

void marena_freespan(struct marena *arena, struct mspan *span,
		     int n, struct mlink *start, struct mlink *end) {
	spin_lock(arena);

	// move to nonempty if necessary
	if (!span->freelist) {
		list_move(&span->alllink, &arena->nonempty);
	}

	end->next = span->freelist;
	span->freelist = start;
	span->ref -= n;


	// If span is completely freed, return it to heap
	if (span->ref == 0) {
		list_del(&span->alllink);
		spin_unlock(arena);
		
		span->freelist = NULL;
		mheap_free(&runtime_mheap, span);
		return;
	}
	
	spin_unlock(arena);
	return;
}


// Fetch a new span from the heap and carve into
// objects for the free list
static int marena_grow(struct marena *arena) {
	int i, size, npages, nobjs;
	void *ptr;
	struct mlink **tailp, *v;
	struct mspan *span;
	
	// called from marena_alloclist if no nonempty span.
	// todo: fix me!
	// Maybe more thread reach here to alloc memory from
	// mheap, this is not good.
	spin_unlock(arena);

	size_class_info(arena->sizeclass, &size, &npages, &nobjs);
	span = mheap_alloc(&runtime_mheap, npages, 1);
	if (!span) {
		spin_lock(arena);
		return -1;
	}

	tailp = &span->freelist;
	ptr = (void *)(span->pageid << PAGESHIFT);

	for (i = 0; i < nobjs; i++) {
		v = (struct mlink *)ptr;
		*tailp = v;
		tailp = &v->next;
		ptr += size;
	}

	*tailp = NULL;

	spin_lock(arena);
	list_add(&span->alllink, &arena->nonempty);
	return 0;
}





// mheap

void mheap_init(struct mheap *heap,
		void *(*allocator)(int), void (*free)(void *)) {
	int i, mapsize;
	spinlock_init(heap);

	// initialized the mem size class
	msize_init();

	mapsize = (1 << MHEAPMAP_BITS) * sizeof(struct mspan *);
	heap->map = sys_alloc(mapsize);
	if (!heap->map) {
		fprintf(stderr, "can't initialize the mheap map\n");
		BUG_ON();
	}
	memset(heap->map, 0, mapsize);
	
	for (i = 0; i < MAX_MHEAP_LIST; i++) {
		INIT_LIST_HEAD(&heap->free[i]);
	}
	INIT_LIST_HEAD(&heap->large);
	for (i = 0; i < NUM_SIZE_CLASSES; i++) {
		marena_init(&heap->arenas[i], i);
	}
	fixmem_init(&heap->mspancache, sizeof(struct mspan), allocator, free);
	fixmem_init(&heap->mcachecache, sizeof(struct mcache), allocator, free);
	return;
}

void mheap_exit(struct mheap *heap) {
	int i, slot = 1ULL << MHEAPMAP_BITS;
	struct mspan *span;

	spin_lock(heap);
	// sys_free all the allocated pages
	for (i = 0; i < slot; i++) {
		if (!(span = heap->map[i]))
			continue;
		if (i != span->pageid)
			BUG_ON();
		sys_free((void *)(span->pageid << PAGESHIFT),
			 span->npages << PAGESHIFT);
		i += span->npages - 1;
		fixmem_free(&heap->mspancache, span);
	}

	// free all the fixmem
	fixmem_exit(&heap->mspancache);
	fixmem_exit(&heap->mcachecache);

	spin_unlock(heap);
	return;
}

// map a pages range into heap->map
static void mheap_map(struct mheap *heap, struct mspan *span) {
	long i;

	for (i = 0; i < span->npages; i++) {
		heap->map[span->pageid + i] = span;
	}
	return;
}

// clean mapping
static void mheap_unmap(struct mheap *heap, struct mspan *span) {
	long i;

	for (i = 0; i < span->npages; i++) {
		heap->map[span->pageid + i] = NULL;
	}
	return;
}

static void __mheap_free(struct mheap *heap, struct mspan *span) {

	// todo. back more mem into system, don't cache them
	if (span->npages > MAX_MHEAP_LIST) {
		list_add(&span->alllink, &heap->large);
		return;
	}
	list_add(&span->alllink, &heap->free[span->npages - 1]);
	return;
}

void mheap_free(struct mheap *heap, struct mspan *span) {
	spin_lock(heap);
	__mheap_free(heap, span);
	spin_unlock(heap);
}


static int mheap_grow(struct mheap *heap, int npage) {
	struct mspan *span;
	void *ptr;

	// Ask for a big chunk. to reduce the number of mappings
	// the operating system needs to track; also amortizes the
	// overhead of an operating system mapping.
	// Allocate a multiple of 64kb (16 pages).
	npage = (npage + 15) & ~15;
	if (npage < MHEAP_CHUNK_GROW)
		npage = MHEAP_CHUNK_GROW + 16;

	ptr = sys_alloc(npage << PAGESHIFT);
	if (!ptr)
		return -1;
	span = fixmem_alloc(&heap->mspancache);
	mspan_init(span, (long)ptr >> PAGESHIFT, npage);

	// map new span
	mheap_map(heap, span);
	__mheap_free(heap, span);
	return 0;
}



static struct mspan *mheap_alloclarge(struct mheap *heap, int npage) {
	struct mspan *span, *best = NULL;

	list_for_each_entry(span, &heap->large, struct mspan, alllink) {
		if (span->npages < npage)
			continue;
		if (best == NULL
		    || span->npages < best->npages
		    || (span->npages == best->npages &&
			span->pageid < best->pageid))
			best = span;
	}
	return best;
}



struct mspan *mheap_alloc(struct mheap *heap, int npage, int zeroed) {
	struct mspan *span, *tmpspan;
	int n;
	
	spin_lock(heap);

	// First: try in fixed-size lists up to max
	for (n = npage; n < MAX_MHEAP_LIST; n++) {
		if (!list_empty(&heap->free[n - 1])) {
			span = list_entry(list_head(&heap->free[n - 1]), struct mspan, alllink);
			goto found;
		}
	}

	// Second: try in large list
	if (!(span = mheap_alloclarge(heap, npage))) {
		if (mheap_grow(heap, npage))
			goto ENOMEM;
		if (!(span = mheap_alloclarge(heap, npage)))
			goto ENOMEM;
	}

 found:

	list_del(&span->alllink);
	if (span->npages > npage) {
		// Trim extra pages back into heap
		tmpspan = fixmem_alloc(&heap->mspancache);
		mspan_init(tmpspan, span->pageid + npage, span->npages - npage);
		span->npages = npage;
		mheap_map(heap, tmpspan);

		// back into heap's list, locked!
		__mheap_free(heap, tmpspan);
	}

	mheap_map(heap, span);
	spin_unlock(heap);
	if (zeroed)
		memset((void *)(span->pageid << PAGESHIFT), 0, npage << PAGESHIFT);
	return span;
 ENOMEM:
	spin_unlock(heap);
	return NULL;
}


struct mspan *mheap_lookup(struct mheap *heap, void *ptr) {

	long pageid = (long)ptr >> PAGESHIFT;
	return heap->map[pageid];
}



// mcache


void mcache_init(struct mcache *mc) {
	int i;

	for (i = 0; i < NUM_SIZE_CLASSES; i++) {
		mc->list[i] = NULL;
		mc->nelem[i] = 0;
	}
	return;
}

void *mcache_alloc(struct mcache *mc, int size, int zeroed) {
	int sizeclass = size_class(size);
	int n;
	struct mlink *first;

	if (!mc->list[sizeclass]) {
		n = marena_alloclist(&runtime_mheap.arenas[sizeclass],
				     class_to_transfercount[sizeclass], &first);
		if (!n) {
			return NULL;
		}
		first->next = NULL;
		mc->nelem[sizeclass] += n;
		mc->list[sizeclass] = first;
	}
	first = mc->list[sizeclass];
	mc->list[sizeclass] = first->next;
	mc->nelem[sizeclass]--;
	if (zeroed)
		memset((void *)first, 0, size);
	return first;
}


void mcache_free(struct mcache *mc, void *p, int size) {
	int sizeclass = size_class(size);
	struct mlink *first;

	first = p;
	first->next = mc->list[sizeclass];
	mc->list[sizeclass] = first;
	mc->nelem[sizeclass]++;
	return;
}



struct mcache *mheap_mcache_create(struct mheap *heap) {
	struct mcache *mc;

	mc = fixmem_alloc(&heap->mcachecache);
	mcache_init(mc);
	return mc;
}


void mheap_mcache_destroy(struct mheap *heap, struct mcache *mc) {
	int i;

	for (i = 0; i < NUM_SIZE_CLASSES; i++) {
		if (!mc->list[i])
			continue;
		marena_freelist(&runtime_mheap.arenas[i], mc->list[i]);
		mc->nelem[i] = 0;
		mc->list[i] = NULL;
	}
	fixmem_free(&heap->mcachecache, mc);
}
