.code64
.section .text

.global flush_gdt
flush_gdt:
	pushq %rcx
	pushw %dx
	lgdtq (%rsp)
	addq $10, %rsp

	pushq $0x8
	call flush_cs

	movw $0x10, %dx
	movw %dx, %ds
	movw %dx, %es
	movw %dx, %fs
	movw %dx, %gs
	movw %dx, %ss
	movw $0x18, %dx
	ltr %dx
	ret
flush_cs:
	lretq

.global load_idt
load_idt:
	pushq %rcx
	pushw %dx
	lidt (%rsp)
	addq $10, %rsp
	ret

.global read_cr3
read_cr3:
	movq %cr3, %rax
	ret
.global write_cr3
write_cr3:
	movq %rcx, %cr3
	ret
.global flush_page
flush_page:
	invlpg (%rcx)
	ret

.extern __kernel_start
.equ KERNEL_HIGHER_HALF_BASE, 0xffffffffc0000000
.global return_to_high_kernel
return_to_high_kernel:
	popq %rax
	movabsq $__kernel_start, %rcx
	subq %rcx, %rax
	movabsq $KERNEL_HIGHER_HALF_BASE, %rcx
	addq %rcx, %rax
	jmp *%rax

.global disable_interrupts
disable_interrupts:
	cli
	ret

.global enable_interrupts
enable_interrupts:
	sti
	ret

.equ BOOTSTRAP_STACK_SIZE, 0x2000
.equ KERNEL_STACK_TOP, 0xffffffffe0000000
.global switch_to_high_stack
switch_to_high_stack:
	popq %rax
	addq $BOOTSTRAP_STACK_SIZE, %rcx
	subq %rsp, %rcx
	movabsq $KERNEL_STACK_TOP, %rdx
	subq %rcx, %rdx
	movq %rdx, %rsp
	jmp *%rax

.global call_kentry
.extern kentry
call_kentry:
	addq $BOOTSTRAP_STACK_SIZE, %rcx
	movq %rcx, %rsp
	jmp kentry

