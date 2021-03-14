#include <stdint.h>
#include <stddef.h>
#include "memory.h"
#include "terminal.h"
#include "cpu.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define MALLOC_MAGIC_USED UINT64_C(0x6465737564657375)

#define DEFAULT_ALIGNMENT 8

struct malloc_free_region {
	size_t size; /* includes this tag */
	struct malloc_free_region *next;
	struct malloc_free_region *prev;
};

struct malloc_tag {
	uint64_t magic;
	size_t size;
};


struct malloc_free_region *kernel_allocation_chain;

void initalize_malloc(void) {
	if (kernel_allocation_chain)
		panic("initalize_malloc(): can only be called once");

	kernel_allocation_chain = (struct malloc_free_region *) KERNEL_HEAP_BOTTOM;
	kernel_allocation_chain->size = PAGESIZE;
	kernel_allocation_chain->next = NULL;
	kernel_allocation_chain->prev = NULL;
}

void *allocate_entire_region(struct malloc_free_region **region) {
	size_t size;
	struct malloc_tag *alloc;
	struct malloc_free_region *hanger;

	hanger = *region;
	size = hanger->size;

	*region = hanger->next;
	if (hanger->next)
		hanger->next->prev = hanger->prev;

	alloc = (struct malloc_tag *) hanger;
	alloc->magic = MALLOC_MAGIC_USED;
	alloc->size = size;

	return alloc + 1;
}

void *allocate_partial_region(struct malloc_free_region **region, size_t allocsize) {
	struct malloc_tag *alloc;
	struct malloc_free_region *next, *prev;
	size_t size;

	next = (*region)->next;
	prev = (*region)->prev;
	size = (*region)->size;

	alloc = (struct malloc_tag *) *region;
	alloc->magic = MALLOC_MAGIC_USED;
	alloc->size = allocsize;

	*region = (struct malloc_free_region *) ((uint8_t *) *region + allocsize);
	(*region)->next = next;
	(*region)->prev = prev;
	(*region)->size = size - allocsize;
	if (next)
		next->prev = *region;
	if (prev)
		prev->next = *region;

	return alloc + 1;
}

/* ensures region and region->next do not overlap, panicing if they do, and */
/* then merges them if they are consecutive. */
void check_and_merge_regions(struct malloc_free_region *region) {
	size_t diff;

	if (!region->next)
		return;
	
	diff = (size_t) region->next - (size_t) region;
	if (diff < region->size) {
		/* overlap, panic */
		panic("check_and_merge_regions(): region %p (size %zu)"
			" overlaps with region %p (size %zu)",
			region, region->size, region->next, region->next->size);
	} else if (diff == region->size) {
		/* merge */
		region->size += region->next->size;
		region->next->next->prev = region;
		region->next = region->next->next;
	}
}

void insert_free_region(struct malloc_free_region *region) {
	struct malloc_free_region **entry, *prev = NULL;
	region->next = region->prev = NULL;

	for (entry = &kernel_allocation_chain; *entry != NULL; prev = *entry, entry = &(*entry)->next) {
		if (*entry > region)
			break;
	}

	region->next = *entry;
	region->prev = prev;
	*entry = region;

	if (region->next) {
		region->next->prev = region;
		check_and_merge_regions(region);
	}

	if (region->prev) {
		region->prev->next = region;
		check_and_merge_regions(region->prev);
	}
}

int claim_new_memory(size_t sz) {
	size_t count;
	uint64_t vaddr, frame, offset;
	struct malloc_free_region *region;

	count = (sz + PAGESIZE - 1) / PAGESIZE; 
	vaddr = allocate_virtual_pages(count);
	if (!vaddr)
		return 1;
	if (allocate_physical_pages(&frame, count, PHYS_PAGE_ALLOC_CONSECUTIVE)) {
		free_virtual_pages(vaddr, count);
		return 1;
	}

	for (offset = 0; offset < count * PAGESIZE; offset += PAGESIZE) {
		map_page(vaddr + offset, frame + offset);
		flush_page(vaddr + offset);
	}
	
	region = (struct malloc_free_region *) vaddr;
	region->size = count * PAGESIZE;
	region->next = region->prev = NULL;

	insert_free_region(region);

	return 0;
}


void *malloc_aligned(size_t size, size_t alignment) {
	size_t allocsize;
	struct malloc_tag *allocation;
	struct malloc_free_region **region;
	struct malloc_free_region *newregion;
	(void) alignment;

	if (!kernel_allocation_chain)
		panic("malloc_aligned(): Kernel allocation never primed");
	if (size == 0)
		return NULL;

	allocsize = sizeof(struct malloc_tag) + size;
	allocsize = MAX(allocsize, sizeof(struct malloc_free_region));

	/*align allocsize to sizeof malloc_free_region */
	if (allocsize % sizeof(struct malloc_free_region))
		allocsize += sizeof(struct malloc_free_region) - allocsize % sizeof(struct malloc_free_region);

	for (region = &kernel_allocation_chain; *region != NULL; region = &(*region)->next)
		if ((*region)->size >= allocsize)
			break;

	if (!*region) {
		if (claim_new_memory(allocsize))
			return NULL;
		return malloc_aligned(size, alignment);
	}

	if ((*region)->size < allocsize + sizeof(struct malloc_free_region))
		return allocate_entire_region(region);
	else
		return allocate_partial_region(region, allocsize);
}

void *malloc(size_t size) {
	return malloc_aligned(size, DEFAULT_ALIGNMENT);
}

void free(void *ptr) {
	struct malloc_tag *tag;
	struct malloc_free_region *region;
	size_t size;

	tag = (struct malloc_tag *) ptr - 1;

	if (tag->magic != MALLOC_MAGIC_USED)
		panic("free(): pointer %p freed, and had a magic value of %llx, but %llx was expected",
			ptr, tag->magic, MALLOC_MAGIC_USED);

	size = tag->size;
	region = (void *) tag;
	region->size = size;

	insert_free_region(region);
}
