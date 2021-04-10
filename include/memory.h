#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stdint.h>
#include <stddef.h>

typedef uint64_t pml4e_t;
typedef uint64_t pdpte_t;
typedef uint64_t pde_t;
typedef uint64_t pte_t;

#define PAGESIZE 4096
#define PAGEMASK UINT64_C(0xfffffffffffff000)
#define PHYSICAL_PAGE_MAP_BASE UINT64_C(0xffff800000000000)
#define P2VADDR(p) ((void *) ((uint64_t) (p) | PHYSICAL_PAGE_MAP_BASE))

#define PAGE_PRESENT 1
#define PAGE_WRITABLE 2
#define PAGE_USER 4
#define PAGE_WRITE_THROUGH 8
#define PAGE_CACHE_DISABLE 0x10
#define PAGE_ACCESSED 0x20
#define PAGE_NO_EXECUTE (UINT64_C(1) << 63)

void initalize_memory(void);
void initalize_malloc(void);
void initalize_virtual_memory(void);

#define PHYS_PAGE_ALLOC_CONSECUTIVE 1
/* Allocates count physical pages and returns the pages in *pages. Accepts */
/* a bitwise or of flags of the form PHYS_PAGE_ALLOC_*. By default count */
/* pages are allocated and placed in the array pages. Currently, the only */
/* flag is PHYS_PAGE_ALLOC_CONSECUTIVE which allocates count consecutive */
/* pages, and returns the base of these pages in *pages,  rather than */
/* writing all the values to an array. On success, 0 is returned and pages */
/* points to an array of count pages, (unless PHYS_PAGE_ALLOC_CONSECUTIVE */
/* is used). On error, 1 is returned and the region of pages through pages */
/* + count is undefined. */
int allocate_physical_pages(uint64_t *pages, size_t count, unsigned flags);
void free_consecutive_physical_pages(uint64_t page, size_t count);
uint64_t allocate_virtual_pages(size_t count);
void free_virtual_pages(uint64_t base, size_t count);

void relocate_bootstrap_data(void);
void unmap_lower_memory(void);
void map_page_autoalloc(uint64_t vaddr, uint64_t paddr, uint64_t flags);
void bootstrap_higher_half_heap_table(void);
void map_high_physical_memory(void);

#define KERNEL_HEAP_BOTTOM UINT64_C(0xffffc00000000000)
#define KERNEL_HEAP_TOP UINT64_C(0xffffd00000000000)

#define KERNEL_HIGHER_HALF_BASE UINT64_C(0xffffffffc0000000)

void *malloc(size_t size);
void free(void *ptr);

#endif/*_MEMORY_H_*/
