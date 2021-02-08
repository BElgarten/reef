#include <stdint.h>
#include <stddef.h>
#include "kernel.h"
#include "bootstrap.h"
#include "terminal.h"
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

	bytes = freebytes = usablebytes = 0;
	for (i = 0; i < bootstrap_info.memory.count; i++) {
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
	clear_screen(create_color(0, 0, 0));
	print_video_info();
	print_memory_info();
	initalize_gdt();
	printf("GDT Initalized\n");
	initalize_idt();
	printf("IDT Initalized\n");
}
