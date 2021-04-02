#ifndef _KERNEL_H_
#define _KERNEL_H_

extern char __kernel_start;
extern char __kernel_text_start;
extern char __kernel_text_end;
extern char __kernel_data_start;
extern char __kernel_data_end;
extern char __kernel_bss_start;
extern char __kernel_bss_end;
extern char __kernel_end;

extern void initalize_gdt(void);
extern void initalize_idt(void);
extern void initalize_syscall(void);

#endif/*_KERNEL_H_*/
