#ifndef _BOOTSTRAP_H_
#define _BOOTSTRAP_H_
#include <stdint.h>
#include <stddef.h>

struct framebuffer {
	uint32_t *buffer;
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	enum {
		RGBX,
		BGRX
	} format;
	struct {
		unsigned width, height;
		unsigned charsize;
		uint8_t *bitmaps;
	} font;
};

enum phys_mem_type {
	PHYS_MEM_RESERVED,
	PHYS_MEM_FREE,
	/* for memory that can't be used until uefi page tables are removed */
	PHYS_MEM_BOOTSTRAP_USED,
	PHYS_MEM_USED,
	PHYS_MEM_ACPI_RECLAIMABLE,
	PHYS_MEM_NONVOLATILE
};

struct bootstrap_memory_map_entry {
	enum phys_mem_type type;
	uint64_t base;
	uint64_t size; /* bytes */
};


#define BOOTSTRAP_STACK_SIZE 0x2000
struct bootstrap_memory_info {
	size_t count;
	struct bootstrap_memory_map_entry *map;
	uint64_t transition_pages;
	uint64_t stack;
};

struct init_file {
	size_t size;
	void *data;
};

/* in the event this structure is edited: */
/* update `relocate_bootstrap_data()` in memory/virtual.c */
struct bootstrap_info {
	struct framebuffer framebuffer;
	struct bootstrap_memory_info memory;
	struct init_file init;
};

extern struct bootstrap_info bootstrap_info;

extern void call_kentry(uint64_t stack);

extern void printf(const char *fmt, ...);

#endif/*_BOOTSTRAP_H_*/
