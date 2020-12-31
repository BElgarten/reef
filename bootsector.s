.code16
.global entry

entry:
cli
movw $0x1300, %ax
movw $0x000f, %bx
movw $len, %cx
xorw %dx, %dx
movw %dx, %es
movw %dx, %ds
movw $msg, %bp
int $0x10

hang:
hlt
jmp hang

msg:
.asciz "Booted in legacy BIOS mode. Please boot with UEFI."
len = . - msg
. = 0x200
