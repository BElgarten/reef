#ifndef _CPU_H_
#define _CPU_H_

#include <stdint.h>

typedef uint64_t gdte_t;
typedef uint64_t idte_t[2];
/* to  avoid padding */
struct splitu64 {
	uint32_t low;
	uint32_t high;
};

struct tss {
	uint32_t reserved1;

	struct splitu64 rsp0;
	struct splitu64 rsp1;
	struct splitu64 rsp2;

	uint32_t reserved2;
	uint32_t reserved3;

	struct splitu64 ist1;
	struct splitu64 ist2;
	struct splitu64 ist3;
	struct splitu64 ist4;
	struct splitu64 ist5;
	struct splitu64 ist6;
	struct splitu64 ist7;

	uint32_t reserved4;
	uint32_t reserved5;
	uint16_t reserved6;
	uint16_t iomap_offset;
};

void flush_gdt(gdte_t *gdt, uint16_t limit);

void load_idt(idte_t *idt, uint16_t limit);

void test_interrupt(void);

uint64_t read_cr3(void);
void write_cr3(uint64_t v);

void flush_page(uint64_t virt);

void disable_interrupts(void);
void enable_interrupts(void);

void return_to_high_kernel(void);

void switch_to_high_stack(uint64_t old_stack_bottom);

#endif/*_CPU_H_*/
