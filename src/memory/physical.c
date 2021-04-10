#include <stdint.h>
#include "bootstrap.h"
#include "memory.h"
#include "cpu.h"
#include "terminal.h"
#include "util.h"

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

	for (i = 0; i < bootstrap_info.memory.count; i++) {
		base = bootstrap_info.memory.map[i].base;
		size = bootstrap_info.memory.map[i].size;
		type = bootstrap_info.memory.map[i].type;
		offset = 0;

		if (type != PHYS_MEM_FREE)
			continue;

		if (base == 0)
			offset += PAGESIZE;

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
	bootstrap_higher_half_heap_table();
	prime_allocators();
	map_high_physical_memory();
	relocate_bootstrap_data();
	release_bootstrap_used_memory();
}
