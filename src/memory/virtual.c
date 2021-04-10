#include <stdint.h>
#include "kernel.h"
#include "memory.h"
#include "terminal.h"
#include "cpu.h"
#include "util.h"
#include "bootstrap.h"

static pml4e_t *pml4;
static int _physical_map_initalized;

void bootstrap_higher_half_heap_table(void) {
	pml4e_t *uefi_pml4;
	pdpte_t *heap_pdpt;
	pde_t *heap_pd;
	pte_t *heap_pt;
	pte_t heap_seed_page;
	uint64_t flags;

	memset((void *) bootstrap_info.memory.transition_pages, 0,
		PAGESIZE * BOOTSTRAP_TRANSITION_PAGE_COUNT);

	pml4 = (pml4e_t *) bootstrap_info.memory.transition_pages;
	heap_pdpt = (pdpte_t *) (bootstrap_info.memory.transition_pages + PAGESIZE);
	heap_pd = (pde_t *) (bootstrap_info.memory.transition_pages + PAGESIZE * 2);
	heap_pt = (pte_t *) (bootstrap_info.memory.transition_pages + PAGESIZE * 3);
	heap_seed_page = (pte_t) (bootstrap_info.memory.transition_pages + PAGESIZE * 4);

	uefi_pml4 = (pml4e_t *) (read_cr3() & PAGEMASK);
	memcpy(pml4, uefi_pml4, 0x80 * sizeof(pml4e_t));

	flags = PAGE_PRESENT | PAGE_WRITABLE;
	pml4[(KERNEL_HEAP_BOTTOM >> 39) & 0x1ff] = (pml4e_t) heap_pdpt | flags;
	heap_pdpt[(KERNEL_HEAP_BOTTOM >> 30) & 0x1ff] = (pdpte_t) heap_pd | flags;
	heap_pd[(KERNEL_HEAP_BOTTOM >> 21) & 0x1ff] = (pde_t) heap_pt | flags;
	heap_pt[(KERNEL_HEAP_BOTTOM >> 12) & 0x1ff] = (pte_t) heap_seed_page | flags;
	write_cr3((uint64_t) pml4);
}

struct virtual_map_entry {
	uint64_t base;
	size_t count;
	struct virtual_map_entry *next;
	struct virtual_map_entry *prev;
};

struct virtual_map_entry *virtual_map;

/* Walks the paging structures and finds the entry of table at index. If */
/* there is no entry, it will allocate the frame and zero it. This returns */
/* a pointer, but it is into physical memory, hence the name. */
static uint64_t *_walk_paging_autoalloc_physical(uint64_t *table, uint16_t index) {
	uint64_t frame;
	if (!(table[index] & PAGE_PRESENT)) {
		if (allocate_physical_pages(&frame, 1, 0))
			panic("unable to allocate memory paging structure");
		table[index] = frame | PAGE_PRESENT | PAGE_WRITABLE;
		if (_physical_map_initalized)
			memset(P2VADDR(frame), 0, PAGESIZE);
		else
			memset((void *) frame, 0, PAGESIZE);
	}

	return (uint64_t *) (table[index] & PAGEMASK);
}


void map_page_autoalloc(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
	uint16_t pml4i, pdpti, pdi, pti;
	pdpte_t *pdpt;
	pde_t *pd;
	pte_t *pt;

	paddr &= PAGEMASK;

	pml4i = vaddr >> 39 & 0x1ff;
	pdpti = vaddr >> 30 & 0x1ff;
	pdi = vaddr >> 21 & 0x1ff;
	pti = vaddr >> 12 & 0x1ff;

	pdpt = P2VADDR(_walk_paging_autoalloc_physical(pml4, pml4i));
	pd = P2VADDR(_walk_paging_autoalloc_physical(pdpt, pdpti));
	pt = P2VADDR(_walk_paging_autoalloc_physical(pd, pdi));

	pt[pti] = paddr | flags;
	flush_page(vaddr);
}

void initalize_virtual_memory(void) {
	virtual_map = malloc(sizeof(struct virtual_map_entry));
	if (!virtual_map)
		panic("Unable to allocate memory for virtual map");

	virtual_map->base = KERNEL_HEAP_BOTTOM + PAGESIZE;
	virtual_map->count = (KERNEL_HEAP_TOP - (KERNEL_HEAP_BOTTOM + PAGESIZE)) / PAGESIZE;
	virtual_map->next = NULL;
}

static void relocate_framebuffer(void) {
	uint64_t framebuffer, vaddr, offset;
	size_t sz;

	sz = 4 * bootstrap_info.framebuffer.pitch * bootstrap_info.framebuffer.height;
	sz = (sz + PAGESIZE - 1) / PAGESIZE;
	
	vaddr = allocate_virtual_pages(sz);
	if (!vaddr)
		panic("relocate_bootstrap_data(): could not allocate space for framebuffer");

	framebuffer = (uint64_t) bootstrap_info.framebuffer.buffer;
	for (offset = 0; sz > 0; offset += PAGESIZE, sz--)
		map_page_autoalloc(vaddr + offset, framebuffer + offset, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NO_EXECUTE);

	bootstrap_info.framebuffer.buffer = (uint32_t *) vaddr;
}

void relocate_bootstrap_data(void) {
	void *dst, *src;
	size_t sz;

	/* console font */
	sz = bootstrap_info.framebuffer.font.charsize * 256;
	src = bootstrap_info.framebuffer.font.bitmaps;
	dst = malloc(sz);
	if (!dst)
		panic("relocate_bootstrap_data(): could not allocate space for font");
	memcpy(dst, src, sz);

	bootstrap_info.framebuffer.font.bitmaps = dst;
	free_consecutive_physical_pages((uint64_t) src, (sz + PAGESIZE - 1) / PAGESIZE);
	printf("console font relocated\n");

	/* uefi memory map */
	sz = bootstrap_info.memory.count * sizeof(struct bootstrap_memory_map_entry);
	src = bootstrap_info.memory.map;
	dst = malloc(sz);
	if (!dst)
		panic("relocate_bootstrap_data(): could not allocate space for memory map");
	memcpy(dst, src, sz);

	bootstrap_info.memory.map = dst;
	free_consecutive_physical_pages((uint64_t) src, (sz + PAGESIZE - 1) / PAGESIZE);

	sz = bootstrap_info.init.size;
	src = bootstrap_info.init.data;
	dst = malloc(sz);
	if (!dst)
		panic("relocate_bootstrap_data(): could not allocate space for init image");
	memcpy(dst, src, sz);

	bootstrap_info.init.data = dst;
	free_consecutive_physical_pages((uint64_t) src, (sz + PAGESIZE - 1) / PAGESIZE);
	printf("uefi memory map relocated\n");

	/* framebuffer */
	relocate_framebuffer();
	printf("framebuffer relocated\n");
}

uint64_t allocate_virtual_pages(size_t count) {
	struct virtual_map_entry *entry;
	uint64_t base = 0;
	
	/* TODO: Try to find best fit instead of first fit to avoid fragmentation */
	for (entry = virtual_map; entry != NULL; entry = entry->next) {
		if (entry->count < count)
			continue;

		base = entry->base;
		entry->base += PAGESIZE * count;
		entry->count -= count;

		if (entry->count == 0) {
			if (entry->next)
				entry->next->prev = entry->prev;
			if (entry->prev)
				entry->prev->next = entry->next;
			if (entry == virtual_map)
				virtual_map = entry->next;
			free(entry);
		}
	}

	return base;
}

static int _regions_overlap(uint64_t base1, size_t count1, uint64_t base2, size_t count2) {
	uint64_t end1, end2;
	end1 = base1 + count1 * PAGESIZE;
	end2 = base2 + count2 * PAGESIZE;

	return base1 < base2 && base2 < end1 || base2 < base1 && base1 < end2;
}

void free_virtual_pages(uint64_t base, size_t count) {
	struct virtual_map_entry **entry, *tmp;
	uint64_t entry_end, allocation_end;

	allocation_end = base + count * PAGESIZE;

	/* TODO: This loop is gross, but its the only way I could think */
	/* to do this in one interation. */
	for (entry = &virtual_map; *entry != NULL; entry = &(*entry)->next) {
		if (_regions_overlap((*entry)->base, (*entry)->count, base, count))
			panic("free_virtual_pages(): allocation overlaps with already free region");

		entry_end = (*entry)->base + (*entry)->count * PAGESIZE;
		if (base < (*entry)->base) {
			if (allocation_end == (*entry)->base) {
				(*entry)->base = allocation_end;
				(*entry)->count += count;
			} else {
				tmp = malloc(sizeof(struct virtual_map_entry));
				if (!tmp)
					panic("free_virtual_pages(): map entry allocation failed");
				tmp->base = base;
				tmp->count = count;
				tmp->next = *entry;
				tmp->prev = (*entry)->prev;
				(*entry)->prev = tmp;
				*entry = tmp;
			}
			return;
		}

		if (entry_end == base) {
			(*entry)->count += count;
			entry_end += count * PAGESIZE;
			if (entry_end == (*entry)->next->base) {
				tmp = (*entry)->next;
				(*entry)->count += tmp->count;
				(*entry)->next = tmp->next;
				(*entry)->next->prev = *entry;
				free(tmp);
			}
			return;
		}

		if ((*entry)->next == NULL) {
			tmp = malloc(sizeof(struct virtual_map_entry));
			if (!tmp)
				panic("free_virtual_pages(): map entry allocation failed");
			tmp->base = base;
			tmp->count = count;
			tmp->prev = (*entry);
			tmp->next = NULL;
			(*entry)->next = tmp;
			return;
		}
	}
}


/* This allocates memory for the page tables, and caches the table address */
/* from the last call, to speed up calling on consecutive pages. This */
/* function does NOT invalidate the TLB entry, this should be done by the */
/* caller, this function is specialize for map_high_physical_memory(). */
static void _map_page_autoalloc_cache(uint64_t paddr) {
	uint64_t vaddr;
	uint64_t page_index;
	pdpte_t *pdpt;
	pde_t *pd;
	pte_t *table;
	static uint64_t prev_page_index;
	static pte_t *prev_table;
	uint16_t pml4i, pdpti, pdi, pti;


	paddr &= PAGEMASK;
	vaddr = (uint64_t) P2VADDR(paddr);

	page_index = vaddr & UINT64_C(0xffffffffffe00000);
	if (!prev_page_index || prev_page_index != page_index) {
		pml4i = vaddr >> 39 & 0x1ff;	
		pdpti = vaddr >> 30 & 0x1ff;
		pdi = vaddr >> 21 & 0x1ff;

		pdpt = _walk_paging_autoalloc_physical(pml4, pml4i);
		pd = _walk_paging_autoalloc_physical(pdpt, pdpti);
		table = _walk_paging_autoalloc_physical(pd, pdi);
		
		prev_page_index = page_index;
		prev_table = table;
	}

	
	pti = vaddr >> 12 & 0x1ff;
	table[pti] = paddr | PAGE_PRESENT | PAGE_WRITABLE;
}

void map_high_physical_memory(void) {
	size_t i;
	uint64_t base;
	uint64_t end;

	for (i = 0; i < bootstrap_info.memory.count; i++) {
		base = bootstrap_info.memory.map[i].base;
		end = base + bootstrap_info.memory.map[i].size;

		for (; base < end; base += PAGESIZE)
			_map_page_autoalloc_cache(base);
	}

	write_cr3(read_cr3());
	switch_to_high_stack();
	_physical_map_initalized = 1;
}

void unmap_lower_memory(void) {
	memset(pml4, 0, 256 * 8);
	write_cr3(read_cr3());
}
