#include <stdint.h>
#include "bootstrap.h"
#include "memory.h"
#include "cpu.h"
#include "terminal.h"
#include "util.h"

#define UNINITALIZED_UEFI_PAGE_STRUCTURE 8
pml4e_t *uefi_pml4 = (pml4e_t *) UNINITALIZED_UEFI_PAGE_STRUCTURE;
pdpte_t *uefi_pdpt = (pdpte_t *) UNINITALIZED_UEFI_PAGE_STRUCTURE;
pde_t *uefi_pd = (pde_t *) UNINITALIZED_UEFI_PAGE_STRUCTURE;

/* initalizes new PML4, new PDPT, new directory and new table */
void initalize_higher_half_paging(void) {
	pml4e_t *my_pml4;
	pdpte_t *my_pdpt;
	pde_t *my_pd;
	pte_t *my_pt;

	my_pml4 = (pml4e_t *) bootstrap_info.memory.transition_pages;
	my_pdpt = (pdpte_t *) (bootstrap_info.memory.transition_pages + PAGESIZE);
	my_pd = (pde_t *) (bootstrap_info.memory.transition_pages + PAGESIZE * 2);
	my_pt = (pte_t *) (bootstrap_info.memory.transition_pages + PAGESIZE * 3);

	uefi_pml4 = (pml4e_t *) (read_cr3() & PAGEMASK);
	memcpy(my_pml4, uefi_pml4, 512 * sizeof(pml4e_t));

	if (uefi_pml4[511] & PAGE_PRESENT && !(uefi_pml4[511] & PAGE_LARGE_TABLE)) {
		uefi_pdpt = (pdpte_t *) (uefi_pml4[511] & PAGEMASK);
		memcpy(my_pdpt, uefi_pdpt, 512 * sizeof(pdpte_t));
	} else {
		memset(my_pdpt, 0, 512 * sizeof(pdpte_t));
	}

	if (my_pdpt[511] & PAGE_PRESENT && !(uefi_pml4[511] & PAGE_LARGE_TABLE))
		uefi_pd = (pde_t *) (my_pdpt[511] & PAGEMASK);

	memset(my_pd, 0, 512 * sizeof(pde_t));
	memset(my_pt, 0, 512 * sizeof(pte_t));

	my_pml4[511] = (pml4e_t) my_pdpt | PAGE_PRESENT | PAGE_WRITABLE;
	my_pdpt[511] = (pdpte_t) my_pd | PAGE_PRESENT | PAGE_WRITABLE;
	/* kernel heap begins at -512M */
	my_pd[256] = (pde_t) my_pt | PAGE_PRESENT | PAGE_WRITABLE; 
	/* recursive directory mapping */
	my_pd[511] = (pde_t) my_pd | PAGE_PRESENT | PAGE_WRITABLE;

	write_cr3((uint64_t) my_pml4);
}

uint64_t get_physical_seed_page(void) {
	enum phys_mem_type type;
	uint64_t base, size;
	size_t i;

	for (i = 0; i < bootstrap_info.memory.count; i++) {
		type = bootstrap_info.memory.map[i].type;
		base = bootstrap_info.memory.map[i].base;
		size = bootstrap_info.memory.map[i].size;

		if (type != PHYS_MEM_FREE)
			continue;

		if (base == 0) {
			base += PAGESIZE;
			size -= PAGESIZE;
		}

		if (size > PAGESIZE)
			return base;
	}

	return 0;
}

struct physical_map_entry {
	uint64_t base;
	size_t count;
	struct physical_map_entry *next;
	struct physical_map_entry *prev;
};

struct physical_map_entry *physical_map;

void initalize_physical_memory(void) {
	size_t i;
	enum phys_mem_type type;
	uint64_t base, size;
	uint64_t offset;
	int firstflag;

	firstflag = 1;

	for (i = 0; i < bootstrap_info.memory.count; i++) {
		base = bootstrap_info.memory.map[i].base;
		size = bootstrap_info.memory.map[i].size;
		type = bootstrap_info.memory.map[i].type;
		offset = 0;

		if (type != PHYS_MEM_FREE)
			continue;

		if (base == 0)
			offset += PAGESIZE;

		if (firstflag) {
			offset += PAGESIZE;
			firstflag = 0;
		}

		if (base % PAGESIZE)
			offset += PAGESIZE - base % PAGESIZE;

		if (size > offset && size - offset > PAGESIZE) {
			base += offset;
			size -= offset;
			free_consecutive_physical_pages(base, size / PAGESIZE);
		}
	}
}

uint64_t allocate_consecutive_physical_pages(size_t count) {
	struct physical_map_entry **entry, *tmp;
	uint64_t ret;

	for (entry = &physical_map; *entry != NULL; entry = &(*entry)->next) {
		if ((*entry)->count == count) {
			tmp = *entry;
			if (tmp->next)
				tmp->next->prev = tmp->prev;
			*entry = tmp->next;
			ret = tmp->base;
			free(tmp);
			return ret;
		} else if (count < (*entry)->count) {
			ret = (*entry)->base;
			(*entry)->base += count * PAGESIZE;
			(*entry)->count -= count;
			return ret;
		}
	}

	return 0;
}

static int _regions_overlap(uint64_t base1, uint64_t end1, uint64_t base2, uint64_t end2) {
	return base1 < base2 && base2 < end1 || base2 < base1 && base1 < end2;
}

static void _merge_into_below_region(struct physical_map_entry *entry, uint64_t page, size_t count) {
	struct physical_map_entry *next;
	uint64_t end;
	entry->count += count;
	end = page + count * PAGESIZE;

	next = entry->next;
	if (next && next->base == end) {
		entry->count += next->count;
		entry->next = next->next;
		if (next->next)
			next->next->prev = entry;
		free(next);
	}
}

static void _insert_entry(struct physical_map_entry **ptr, uint64_t base, size_t count) {
	struct physical_map_entry *entry;
	uint64_t end;

	end = base + count * PAGESIZE;
	if (*ptr && end == (*ptr)->base) {
		(*ptr)->base = base;
		(*ptr)->count += count;
	} else {
		entry = malloc(sizeof(struct physical_map_entry));
		if (!entry)
			panic("_insert_entry(): could not allocate memory for physical map entry");

		entry->base = base;
		entry->count = count;
		entry->next = *ptr;
		entry->prev = (*ptr)->prev;
		(*ptr)->prev = entry;
		*ptr = entry;
	}
}

void free_consecutive_physical_pages(uint64_t page, size_t count) {
	struct physical_map_entry **entry, *tmp;
	uint64_t end, entry_end;
	size_t diff;

	end = page + count * PAGESIZE;
	for (entry = &physical_map; *entry != NULL; entry = &(*entry)->next) {
		entry_end = (*entry)->base + (*entry)->count * PAGESIZE;

		if (_regions_overlap(page, end, (*entry)->base, entry_end))
			panic("free_consecutive_physical_pages(): free'd pages overlap with already free region\n"
				"Map Entry: %p - %p\n"
				"User Free: %p - %p", (*entry)->base, entry_end, page, end);

		if (entry_end == page) {
			_merge_into_below_region(*entry, page, count);
			return;
		} else if ((*entry)->base >= page) {
			_insert_entry(entry, page, count);
			return;
		}
	}

	_insert_entry(entry, page, count);
}

int allocate_physical_pages(uint64_t *pages, size_t count, unsigned flags) {
	size_t i;

	if (flags & PHYS_PAGE_ALLOC_CONSECUTIVE) {
		*pages = allocate_consecutive_physical_pages(count);
		if (!*pages)
			return 1;

	} else {
		for (i = 0; i < count; i++) {
			pages[i] = allocate_consecutive_physical_pages(1);
			if (!pages[i]) {
				while (i--)
					free_consecutive_physical_pages(pages[i], 1);
				return 1;
			}
		}
	}

	return 0;
}

void prime_allocators(void) {
	uint64_t malloc_seed;

	malloc_seed = get_physical_seed_page();
	if (!malloc_seed)
		panic("Not enough memory to seed malloc");

	map_page(KERNEL_HEAP_BOTTOM, malloc_seed, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NO_EXECUTE);
	initalize_malloc();
	initalize_virtual_memory();
	initalize_physical_memory();
}

void release_bootstrap_used_memory(void) {
	size_t i;
	uint64_t base, size;
	enum phys_mem_type type;

	for (i = 0; i < bootstrap_info.memory.count; i++) {
		base = bootstrap_info.memory.map[i].base;
		size = bootstrap_info.memory.map[i].size;
		type = bootstrap_info.memory.map[i].type;
		if (base % PAGESIZE) {
			size -= PAGESIZE - base % PAGESIZE;
			base += PAGESIZE - base % PAGESIZE;
		}


		if (type == PHYS_MEM_BOOTSTRAP_USED && size > PAGESIZE) 
			free_consecutive_physical_pages(base, size / PAGESIZE);
	}
}

void initalize_memory(void) {
	/* TODO: Assert all executable data and allocations are below 0xffff_ffff_c000_0000*/
	disable_interrupts();
	initalize_higher_half_paging();
	prime_allocators();
	relocate_kernel();
	relocate_bootstrap_data();
	release_bootstrap_used_memory();
}
