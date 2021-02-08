[BITS 16]
[ORG 0x7c00]

entry:
cli
mov ax, 0x1300
mov bx, 0x000f
mov cx, len
xor dx, dx
mov es, dx
mov ds, dx
lea bp, msg
int 0x10

hang:
hlt
jmp hang

msg:
db "Booted in legacy BIOS mode. Please boot with UEFI.", 0
len equ $ - msg

fill:
times 0x200 db 0 
