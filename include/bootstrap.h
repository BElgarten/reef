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
	PHYS_MEM_USED,
	PHYS_MEM_ACPI_RECLAIMABLE,
	PHYS_MEM_NONVOLATILE
};

struct bootstrap_memory_map_entry {
	enum phys_mem_type type;
	uint64_t base;
	uint64_t size;
};

struct bootstrap_memory_map {
	size_t count;
	struct bootstrap_memory_map_entry *map;
};

struct bootstrap_info {
	struct framebuffer framebuffer;
	struct bootstrap_memory_map memory;
};

extern struct bootstrap_info bootstrap_info;

extern void kentry(void);

extern void printf(const char *fmt, ...);

#endif/*_BOOTSTRAP_H_*/
