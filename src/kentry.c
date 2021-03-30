#include <stdint.h>
#include <stddef.h>
#include "kernel.h"
#include "bootstrap.h"
#include "terminal.h"
#include "memory.h"
#include "cpu.h"

void print_video_info(void) {
	printf("Framebuffer: %u x %u - %s\n",
		(unsigned) bootstrap_info.framebuffer.width,
		(unsigned) bootstrap_info.framebuffer.height,
		bootstrap_info.framebuffer.format == RGBX ?
		"RGBX" : "BGRX");
}

void print_memory_info(void) {
	size_t bytes, freebytes, usablebytes;
	size_t i;
	uint64_t base, size;
	enum phys_mem_type type;

	bytes = freebytes = usablebytes = 0;
	for (i = 0; i < bootstrap_info.memory.count; i++) {
		base = bootstrap_info.memory.map[i].base;
		size = bootstrap_info.memory.map[i].size;
		type = bootstrap_info.memory.map[i].type;

/*
		printf("Base: %p | Size: %p | Type: %s\n",
			base, size,
			type == PHYS_MEM_FREE ? "Free" : 
			type == PHYS_MEM_USED ? "Used" :
			type == PHYS_MEM_RESERVED ? "Reserved":
			type == PHYS_MEM_BOOTSTRAP_USED ? "Bootstrap Used" :
			type == PHYS_MEM_ACPI_RECLAIMABLE ? "ACPI Reclaimable" :
			type == PHYS_MEM_NONVOLATILE ? "Non-Volatile" :
			"Unknown");
*/
		switch (bootstrap_info.memory.map[i].type) {
			case PHYS_MEM_FREE:
				freebytes += bootstrap_info.memory.map[i].size;
			case PHYS_MEM_USED:
				usablebytes += bootstrap_info.memory.map[i].size;
			default:
				bytes += bootstrap_info.memory.map[i].size;
				break;
		}
	}

	printf("Memory: 0x%zx / 0x%zx (Total 0x%zx)\n",
		freebytes, usablebytes, bytes);
}

void kentry(void) {
	disable_interrupts();
	clear_screen(create_color(0, 0, 0));


	print_video_info();
	print_memory_info();


	initalize_memory();
	return_to_high_kernel();
	printf("Memory intialized\n");

	initalize_gdt();
	printf("GDT Initalized\n");
	unmap_lower_memory();

	initalize_idt();
	printf("IDT Initalized\n");

	for (;;);
}
