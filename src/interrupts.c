#include <stdint.h>
#include "kernel.h"
#include "cpu.h"
#include "terminal.h"

void interrupt_handler(uint8_t n, const struct registers *regs, uint64_t code) {
	printf("Recieved Interrupt %u, Code: %p\n", n, code);
	printf("rax: %p rbx: %p\n", regs->rax, regs->rbx);
	printf("rcx: %p rdx: %p\n", regs->rcx, regs->rdx);
	printf("rsi: %p rdi: %p\n", regs->rsi, regs->rdi);
	printf("rsp: %p rbp: %p\n", regs->rsp, regs->rbp);
	printf("r8:  %p r9:  %p\n", regs->r8, regs->r9);
	printf("r10: %p r11: %p\n", regs->r10, regs->r11);
	printf("r12: %p r13: %p\n", regs->r12, regs->r13);
	printf("r14: %p r15: %p\n", regs->r14, regs->r15);
	printf("cr2: %p\n", regs->cr2);
	panic("interrupts not currently handled, hanging...");
}
