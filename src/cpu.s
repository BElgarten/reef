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
.equ PHYSICAL_PAGE_MAP_BASE, 0xffff800000000000
.global return_to_high_kernel
return_to_high_kernel:
	popq %rax
	movabsq $PHYSICAL_PAGE_MAP_BASE, %rcx
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
.global switch_to_high_stack
switch_to_high_stack:
	movq $PHYSICAL_PAGE_MAP_BASE, %rax
	addq %rax, %rsp	
	ret

.global call_kentry
.extern kentry
call_kentry:
	addq $BOOTSTRAP_STACK_SIZE, %rcx
	movq %rcx, %rsp
	jmp kentry

.global cpuid
cpuid:
	pushq %rbx
	movq %rcx, %rax
	movq %rdx, %rcx
	cpuid
	movl %eax, (%r8)
	movl %ebx, 4(%r8)
	movl %ecx, 8(%r8)
	movl %edx, 12(%r8)
	popq %rbx
	ret
.global write_msr
write_msr:
	movl %edx, %eax
	shrq $32, %rdx
	wrmsr
	ret

.global read_msr
read_msr:
	rdmsr
	subq $8, %rsp
	movl %edx, 4(%rsp)
	movl %eax, (%rsp)
	popq %rax
	ret
