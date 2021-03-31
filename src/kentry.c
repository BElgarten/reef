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

void enable_cpu_features(void) {
	struct cpuid_result cpuid_res;
	uint32_t max_ext_cpuid;
	uint64_t efer, tmp;

	cpuid(0x80000000, 0, &cpuid_res);
	max_ext_cpuid = cpuid_res.eax;

	if (max_ext_cpuid < 0x80000001)
		panic("enable_cpu_features(): required features not supported");

	cpuid(0x80000001, 0, &cpuid_res);
	if (!(cpuid_res.edx & (1 << 11)))
		panic("enable_cpu_features(): syscall not supported");

	if (!(cpuid_res.edx & (1 << 20)))
		panic("enable_cpu_features(): no-execute not supported");

	tmp = efer = read_msr(IA32_EFER);
	tmp |= IA32_EFER_SCE | IA32_EFER_NXE;
	if (tmp != efer)
		write_msr(IA32_EFER, tmp);
}

void kentry(void) {
	disable_interrupts();
	clear_screen(create_color(0, 0, 0));

	enable_cpu_features();
	printf("CPU features enabled\n");

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
