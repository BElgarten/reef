#ifndef _CPU_H_
#define _CPU_H_

#include <stdint.h>

#define IA32_EFER 0xC0000080
#define IA32_EFER_SCE 1
#define IA32_EFER_NXE (1 << 11)

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

struct registers {
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;

	uint64_t cr2;
};

void flush_gdt(gdte_t *gdt, uint16_t limit);

void load_idt(idte_t *idt, uint16_t limit);

uint64_t read_cr3(void);
void write_cr3(uint64_t v);

void flush_page(uint64_t virt);

void disable_interrupts(void);
void enable_interrupts(void);

void return_to_high_kernel(void);

void switch_to_high_stack(uint64_t old_stack_bottom);

struct cpuid_result {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
};
void cpuid(uint32_t eax, uint32_t ecx, struct cpuid_result *res);

void write_msr(uint32_t msr, uint64_t value);
uint64_t read_msr(uint32_t msr);

#endif/*_CPU_H_*/
