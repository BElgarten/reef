#include <stdint.h>
#include "kernel.h"
#include "memory.h"
#include "terminal.h"
#include "cpu.h"
#include "util.h"
#include "bootstrap.h"

struct virtual_map_entry {
	uint64_t base;
	size_t count;
	struct virtual_map_entry *next;
	struct virtual_map_entry *prev;
};

struct virtual_map_entry *virtual_map;

static pte_t *_calculate_table_address(uint64_t vaddr) {
	uint64_t tables_base = 0xffffffffffe00000;
	uint16_t pdi;

	pdi = (vaddr >> 21) & 0x1ff;
	return (pte_t *) (tables_base | (uint32_t) pdi << 12);
}

/* assumes vaddr is under kernel page directory */
void map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
	uint16_t pdi, pti;
	uint64_t tables_base = 0xffffffffffe00000;
	uint64_t table_page;
	pde_t *directory = (pde_t *) 0xfffffffffffff000;
	pte_t *table;

	pdi = (vaddr >> 21) & 0x1ff;
	pti = (vaddr >> 12) & 0x1ff;

	table = (pte_t *) (tables_base | (uint32_t) pdi << 12);

	if ((directory[pdi] & PAGE_PRESENT) == 0) {
		if (allocate_physical_pages(&table_page, 1, 0))
			panic("map_page(): unable to allocate memory for page table");

		map_page((uint64_t) table, table_page, PAGE_PRESENT | PAGE_WRITABLE);
		memset(table, 0, PAGESIZE);
	}

	paddr &= PAGEMASK;
	table[pti] = paddr | flags;
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
		map_page(vaddr + offset, framebuffer + offset, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NO_EXECUTE);

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

	/* uefi memory map */
	sz = bootstrap_info.memory.count * sizeof(struct bootstrap_memory_map_entry);
	src = bootstrap_info.memory.map;
	dst = malloc(sz);
	if (!dst)
		panic("relocate_bootstrap_data(): could not allocate space for memory map");
	memcpy(dst, src, sz);

	bootstrap_info.memory.map = dst;
	free_consecutive_physical_pages((uint64_t) src, (sz + PAGESIZE - 1) / PAGESIZE);

	/* framebuffer */
	relocate_framebuffer();
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

void relocate_kernel(void) {
	uint64_t paddr, vaddr;
	uint64_t start = (uint64_t) &__kernel_start;
	uint64_t end = (uint64_t) & __kernel_end;
	uint64_t flags;
	uint64_t *stackpages;
	size_t count;
	uint8_t *oldstack, *newstack;

	vaddr = KERNEL_HIGHER_HALF_BASE;
	for (paddr = start; paddr <= end; paddr += PAGESIZE, vaddr += PAGESIZE) {
		if ((uint64_t) &__kernel_text_start <= paddr && paddr < (uint64_t) &__kernel_text_end)
			flags = PAGE_PRESENT;
		else
			flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_NO_EXECUTE;
		map_page(vaddr, paddr, flags);
	}

	/* relocate stack */
	count = (KERNEL_STACK_SIZE + PAGESIZE - 1) / PAGESIZE;

	stackpages = malloc(count * sizeof(uint64_t));
	if (!stackpages)
		panic("relocate_kernel(): could not allocate memory to hold stack pages");

	if (allocate_physical_pages(stackpages, count, 0))
		panic("relocate_kernel(): could not allocate physical memory for stack");

	for (vaddr = KERNEL_STACK_TOP; count > 0; count--, vaddr -= PAGESIZE)
		/* vaddr - 1 because vaddr points to the top of the page we want to map */
		map_page(vaddr - 1, stackpages[count - 1], PAGE_PRESENT | PAGE_WRITABLE | PAGE_NO_EXECUTE);

	free(stackpages);

	write_cr3(read_cr3());

	memcpy((void *) (KERNEL_STACK_TOP - BOOTSTRAP_STACK_SIZE),
		(void *) bootstrap_info.memory.stack, BOOTSTRAP_STACK_SIZE);

	/* I think it may be important that this call is the last thing in this funciton */
	switch_to_high_stack(bootstrap_info.memory.stack);
}

void unmap_lower_memory(void) {
	uint64_t vaddr, pml4_frame, pdpt_frame;
	pml4e_t *pml4;
	pdpte_t *pdpt;

	vaddr = allocate_virtual_pages(1);
	if (!vaddr)
		panic("unmap_lower_memory(): could not allocate virtual memory temporary mapping");

	pml4 = (pml4e_t *) vaddr;
	pdpt = (pdpte_t *) vaddr;

	pml4_frame = read_cr3() & PAGEMASK;
	map_page(vaddr, pml4_frame, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NO_EXECUTE);
	flush_page(vaddr);

	pdpt_frame = pml4[511] & PAGEMASK;
	memset(pml4, 0, 511 * sizeof(pml4e_t));

	map_page(vaddr, pdpt_frame, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NO_EXECUTE);
	flush_page(vaddr);

	memset(pdpt, 0, 511 * sizeof(pdpte_t));
	free_virtual_pages(vaddr, 1);
}
