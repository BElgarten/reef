#include <stdint.h>
#include "kernel.h"
#include "cpu.h"
#include "util.h"

#define GDT_FLAG_PAGE_GRANULARITY 0x80
#define GDT_FLAG_DEFAULT_OPERAND_32 0x40
#define GDT_FLAG_LONG_CODE 0x20

#define GDT_TYPE_PRESENT 0x80
#define GDT_TYPE_DPL(n) (((n) & 3) << 5)
#define GDT_TYPE_NON_SYSTEM 0x10
#define GDT_TYPE_CODE 8
#define GDT_TYPE_EXPAND_DOWN 4
#define GDT_TYPE_CONFORMING 4
#define GDT_TYPE_RW 2
#define GDT_TYPE_XR 2
#define GDT_TYPE_ACCESSED 1
#define GDT_TYPE_TSS 9
#define GDT_TYPE_TSS_BUSY 2


void create_gdt_entry(gdte_t *entry, uint64_t base, uint8_t flags, uint8_t type) {
	*entry = 0;

	*entry |= (base & 0x00ffffff) << 16;
	*entry |= (base & 0xff000000) << 56;

	*entry |= (uint64_t) flags << 48;
	*entry |= (uint64_t) type << 40;
}

#define GDT_ENTRY_COUNT 4
gdte_t gdt[GDT_ENTRY_COUNT];
void initalize_gdt(void) {
	uint8_t type, flags;
	uint16_t limit;

	memset(&gdt[0], 0, sizeof(gdte_t));

	type = GDT_TYPE_PRESENT | GDT_TYPE_DPL(0) | GDT_TYPE_NON_SYSTEM | GDT_TYPE_CODE | GDT_TYPE_XR;
	flags = GDT_FLAG_PAGE_GRANULARITY | GDT_FLAG_LONG_CODE;
	create_gdt_entry(&gdt[1], 0, flags, type);

	type = GDT_TYPE_PRESENT | GDT_TYPE_DPL(0) | GDT_TYPE_NON_SYSTEM | GDT_TYPE_RW;
	flags = GDT_FLAG_PAGE_GRANULARITY;
	create_gdt_entry(&gdt[2], 0, flags, type);

	/* clear memory for TSS, but don't install it */
	memset(&gdt[3], 0, sizeof(gdte_t) * 2);

	limit = sizeof(gdte_t) * GDT_ENTRY_COUNT - 1;
	flush_gdt(&gdt[0], limit);
}

#define IDT_TYPE_PRESENT 0x8000
#define IDT_TYPE_DPL(n) (((n) & 3) << 13)
#define IDT_TYPE_TASK 0x500
/* See Intel Manual 3 Section 6.12.1.2 on Interrupt vs Trap gates. */
/* Essentially, Interrupt gates cannot be interrupted but traps can be. */
#define IDT_TYPE_INTERRUPT 0x600
#define IDT_TYPE_TRAP 0x700
#define IDT_TYPE_IST(n) ((n) & 7)

void create_idt_entry(idte_t *entry, uint64_t offset, uint16_t segment, uint16_t type) {
	(*entry)[1] = offset >> 32;
	(*entry)[0] = (offset & 0xffff0000) << 32;
	(*entry)[0] |= (uint64_t) type << 32;
	(*entry)[0] |= (uint64_t) segment << 16;
	(*entry)[0] |= offset & 0xffff;
}

#define IDT_ENTRY_COUNT 32
idte_t idt[IDT_ENTRY_COUNT];
extern void *idt_stubs[];
void initalize_idt(void) {
	uint16_t type;
	uint16_t limit;
	size_t i;

	type = IDT_TYPE_PRESENT | IDT_TYPE_DPL(0) | IDT_TYPE_INTERRUPT | IDT_TYPE_IST(0);
	for (i = 0; i < 32; i++)
		create_idt_entry(&idt[i], (uint64_t) idt_stubs[0], 8, type);

	limit = sizeof(idte_t) * IDT_ENTRY_COUNT - 1;
	load_idt(&idt[0], limit);
}
