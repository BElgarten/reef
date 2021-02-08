.global flush_gdt
.code64
.section .text

flush_cs:
	lretq
flush_gdt:
	pushq %rcx
	pushw %dx
	lgdtq (%rsp)
	addq $10, %rsp

	pushq $0x8
	call flush_cs

	flush_selectors:
	movw $0x10, %dx
	movw %dx, %ds
	movw %dx, %es
	movw %dx, %fs
	movw %dx, %gs
	movw %dx, %ss
	ret

.global load_idt
load_idt:
	pushq %rcx
	pushw %dx
	lidt (%rsp)
	addq $10, %rsp
	ret

.global test_interrupt
test_interrupt:
	jmp .
	int $10
