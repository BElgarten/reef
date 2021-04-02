#include <stdint.h>
#include "cpu.h"
#include "kernel.h"
#include "terminal.h"

#define SYSCALL_SUPERVISOR_SEGMENTS 8
#define SYSCALL_USER_SEGMENTS 0x28

extern void syscall_stub(void);

void initalize_syscall(void) {
	uint64_t star;
	/* set IOPL to 0 on syscall */
	write_msr(IA32_FMASK, 0x3000);

	star = (uint64_t) SYSCALL_USER_SEGMENTS << 48;
	star |= (uint64_t) SYSCALL_SUPERVISOR_SEGMENTS << 32;

	write_msr(IA32_STAR, star);

	write_msr(IA32_LSTAR, (uint64_t) &syscall_stub);
}

void syscall_handler(struct registers *regs){
	printf("Recieved syscall\n");
	printf("rax: %p rbx: %p\n", regs->rax, regs->rbx);
	printf("rcx: %p rdx: %p\n", regs->rcx, regs->rdx);
	printf("rsi: %p rdi: %p\n", regs->rsi, regs->rdi);
	printf("rsp: %p rbp: %p\n", regs->rsp, regs->rbp);
	printf("r8:  %p r9:  %p\n", regs->r8, regs->r9);
	printf("r10: %p r11: %p\n", regs->r10, regs->r11);
	printf("r12: %p r13: %p\n", regs->r12, regs->r13);
	printf("r14: %p r15: %p\n", regs->r14, regs->r15);
	printf("cr2: %p\n", regs->cr2);
	panic("syscall not currently handled, hanging...");

}
