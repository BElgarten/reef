LOG_PAGE_SIZE = 12
PAGE_SIZE = 1 << LOG_PAGE_SIZE
STACK_SIZE = 4096

MaxMemoryType = 14
MaxEfiErrno = 36
BootServices = 96
GetMemoryMap = 56
AllocatePages = 40
ExitBootServices = 232

AllocateAnyPages = 0
EfiLoaderData = 2

ConOut = 64
OutputString = 8
ClearScreen = 48

.section text
.code64
.global kentry
kentry:
/* Calling Convention */
/* right to left: rcx, rdx, r8, r9 */
/* EFI_STATUS kentry(EFI_HANDLE *, EFI_SYSTEM_TABLE *) */

movq $stack_top, %rsp
movq %rsp, %rbp

movq %rcx, image_handle
movq %rdx, system_table

call clear_screen
movq $boot_message, %rcx
call print_string

call get_memory_map_size

leaq 0x60(%rax), %rbx

leaq 0x20(%rbx), %rcx

call uefi_malloc

movq %rax, %rcx
movq %rbx, (%rcx)
movq %rcx, %rdi
call get_memory_map

movq %rdi, %rcx
call print_memory_map

movq %rdi, %rcx
movq -16(%rdi), %rcx
subq $32, %rcx
movq %rcx, (%rdi)
movq %rdi, %rcx
call get_memory_map

movq %rdi, %rcx
call exit_uefi_boot_services

hang:
cli
hlt
jmp hang

exit_uefi_boot_services:
	movq 16(%rcx), %rdx
	movq image_handle, %rcx
	movq system_table, %rax
	movq BootServices(%rax), %rax
	movq ExitBootServices(%rax), %rax
	jmp *%rax
	

print_newline:
	movq $print_newline.buffer, %rcx
	jmp print_wide_string

print_memory_map_entry:
	pushq %rbx
	movq %rcx, %rbx

	movq 8(%rbx), %rcx
	call print_hex

	movq $memory_map_print_seperator_string, %rcx
	call print_string

	movq 16(%rbx), %rcx
	call print_hex

	movq $memory_map_print_seperator_string, %rcx
	call print_string

	movq 24(%rbx), %rcx
	call print_hex

	movq $memory_map_print_seperator_string, %rcx
	call print_string

	movq (%rbx), %rcx
	cmp $MaxMemoryType, %rcx 
	jle print_memory_map_entry.valid_memory_type

	movq $invalid_memory_type_string, %rcx
	jmp print_memory_map_entry.set_memory_type_string
	print_memory_map_entry.valid_memory_type:

	movq memory_type_strings(,%rcx, 8), %rcx
	print_memory_map_entry.set_memory_type_string:

	call print_string

	call print_newline
	popq %rbx
	ret
print_memory_map:
	pushq %rbx
	movq %rcx, %rbx

	addq $32, %rcx
	movq %rcx, %rdx
	addq (%rbx), %rdx

	print_memory_map.loop:
		cmpq %rdx, %rcx
		je print_memory_map.exit
		pushq %rdx
		pushq %rcx
		call print_memory_map_entry
		popq %rcx
		popq %rdx
		addq 8(%rbx), %rcx
		jmp print_memory_map.loop


	print_memory_map.exit:

	popq %rbx
	ret
/* void check_error_and_die(const char *, EFI_STATUS); */
check_error_and_die:
	btr $63, %rdx
	jc check_error_and_die.and_die
	ret
	check_error_and_die.and_die:
	pushq %rdx
	call print_string
	movb $':', %cl
	call putchar
	movb $' ', %cl
	call putchar
	movq (%rsp), %rdx
	cmpq $MaxEfiErrno, %rdx
	jle check_error_and_die.valid_errno
	movq $check_error_and_die.invalid_errno_msg, %rcx
	jmp check_error_and_die.msg_set

	check_error_and_die.valid_errno:
	movq check_error_and_die.error_strings(,%rdx, 8), %rcx

	check_error_and_die.msg_set:
	call print_string
	movb $' ', %cl
	call putchar
	movb $'(', %cl
	call putchar
	movq (%rsp), %rcx
	call print_number
	movb $')', %cl
	call putchar
	jmp hang

get_memory_map:
	leaq 32(%rcx), %rdx
	leaq 16(%rcx), %r8
	leaq 8(%rcx), %r9
	pushq %rbx
	leaq 24(%rcx), %rbx

	pushq %rbx
	subq $24, %rsp /*alignment*/
	movq system_table, %rax
	movq BootServices(%rax), %rax
	movq GetMemoryMap(%rax), %rax
	call *%rax

	addq $32, %rsp
	movq %rax, %rdx
	movq $get_memory_map_error_str, %rcx
	call check_error_and_die
	popq %rbx
	ret

print_hex:
	pushq %rdi
	movq $print_number.buffer_end, %rdi
	xorq %rdx, %rdx
	movb $16, %al

	print_hex.loop:
	dec %rdi
	dec %rdi
	movb %cl, %dl
	andb $0x0f, %dl
	movb hex_digits(%rdx), %dl
	movw %dx, (%rdi)
	shrq $4, %rcx
	dec %al
	jnz print_hex.loop

	movq %rdi, %rcx
	popq %rdi
	jmp print_wide_string
	
print_number:
	testq %rcx, %rcx
	jns print_unsigned_number 
	negq %rcx
	pushq %rcx
	movb $'-', %cl
	call putchar
	pop %rcx
	jmp print_unsigned_number
print_unsigned_number:
	pushq %rdi

	movq %rcx, %rax
	movq $10, %rcx
	movq $print_number.buffer_end, %rdi

	print_number.loop:
	dec %rdi
	dec %rdi
	xorq %rdx, %rdx
	divq %rcx
	addb $48, %dl
	movw %dx, (%rdi)
	testq %rax, %rax
	jnz print_number.loop

	movq %rdi, %rcx
	popq %rdi
	jmp print_wide_string

/* returns pointer to uefi allocated memory allocated with alloc_pages */
/* ie allocations will be rounded up to the page size */
uefi_malloc:
	pushq %rbx
	subq $0x10, %rsp

	leaq (PAGE_SIZE - 1 + 0x10)(%rcx), %r8
	shrq $LOG_PAGE_SIZE, %r8
	movq %r8, %rbx
	shlq $LOG_PAGE_SIZE, %rbx
	subq $0x10, %rbx

	movq $AllocateAnyPages, %rcx
	movq $EfiLoaderData, %rdx
	movq %rsp, %r9

	movq system_table, %rax
	movq BootServices(%rax), %rax
	movq AllocatePages(%rax), %rax
	call *%rax

	movq %rax, %rdx
	movq $uefi_malloc_err_str, %rcx
	call check_error_and_die

	uefi_malloc.success:
	movq (%rsp), %rax
	movq %rbx, (%rax)
	addq $0x10, %rax

	uefi_malloc.epilogue:
	addq $0x10, %rsp
	popq %rbx
	ret
	

get_memory_map_size:
	movq system_table, %rax
	movq BootServices(%rax), %rax
	movq GetMemoryMap(%rax), %rax
	
	subq $56, %rsp
	leaq 24(%rsp), %rcx
	xorq %rdx, %rdx
	leaq 16(%rsp), %r8
	leaq 8(%rsp), %r9
	pushq %rsp
	subq $24, %rsp

	movq %rdx, (%rcx)
	call *%rax

	addq $56, %rsp
	popq %rax
	addq $24, %rsp
	ret

print_wide_string:
	movq %rcx, %rdx
	movq system_table, %rcx
	movq ConOut(%rcx), %rcx
	movq OutputString(%rcx), %rax
	jmp *%rax

clear_screen:
	movq system_table, %rcx
	movq ConOut(%rcx), %rcx
	movq ClearScreen(%rcx), %rax
	jmp *%rax

putchar:
	movb %cl, putchar.buffer
	movq $putchar.buffer, %rcx
	jmp print_wide_string
	
print_string:
	pushq %rsi
	pushq %rbx
	subq $8, %rsp
	movq %rcx, %rsi
	jmp print_string.comparator

	print_string.loop:
	movb %bl, %cl
	subq $32, %rsp
	call putchar
	addq $32, %rsp
	inc %rsi
	print_string.comparator:
	movb (%rsi), %bl
	testb %bl, %bl
	jnz print_string.loop

	addq $8, %rsp
	popq %rbx
	popq %rsi
	ret
.section data
boot_message:
.ascii "Booted\r\n"
.asciz "Detecting Memory...\r\n"
hex_digits: .ascii "0123456789abcdef"
uefi_malloc_err_str: .asciz "AllocatePages()"
get_memory_map_error_str: .asciz "GetMemoryMap()"
print_newline.buffer: .byte '\r', 0, '\n', 0, 0, 0
memory_map_print_seperator_string: .asciz " | "
invalid_memory_type_string: .asciz "Unknown"

debug_str: .asciz "Debug Msg"

memory_type_strings: 
.quad memory_type_string_0
.quad memory_type_string_1
.quad memory_type_string_2
.quad memory_type_string_3
.quad memory_type_string_4
.quad memory_type_string_5
.quad memory_type_string_6
.quad memory_type_string_7
.quad memory_type_string_8
.quad memory_type_string_9
.quad memory_type_string_10
.quad memory_type_string_11
.quad memory_type_string_12
.quad memory_type_string_13
.quad memory_type_string_14

memory_type_string_0: .asciz "Reserved"
memory_type_string_1: .asciz "Loader Code"
memory_type_string_2: .asciz "Loader Data"
memory_type_string_3: .asciz "Boot Services Code"
memory_type_string_4: .asciz "Boot Services Data"
memory_type_string_5: .asciz "Runtime Services Code"
memory_type_string_6: .asciz "Runtime Services Data"
memory_type_string_7: .asciz "Conventional"
memory_type_string_8: .asciz "Unusable"
memory_type_string_9: .asciz "ACPI Reclaimable"
memory_type_string_10: .asciz "ACPI NVS"
memory_type_string_11: .asciz "MMIO"
memory_type_string_12: .asciz "MMIO Port Space"
memory_type_string_13: .asciz "Pal Code"
memory_type_string_14: .asciz "Persistent"

check_error_and_die.invalid_errno_msg: .asciz "Invalid Error Number"
check_error_and_die.error_strings:
.quad uefi_error_string_0
.quad uefi_error_string_1
.quad uefi_error_string_2
.quad uefi_error_string_3
.quad uefi_error_string_4
.quad uefi_error_string_5
.quad uefi_error_string_6
.quad uefi_error_string_7
.quad uefi_error_string_8
.quad uefi_error_string_9
.quad uefi_error_string_10
.quad uefi_error_string_11
.quad uefi_error_string_12
.quad uefi_error_string_13
.quad uefi_error_string_14
.quad uefi_error_string_15
.quad uefi_error_string_16
.quad uefi_error_string_17
.quad uefi_error_string_18
.quad uefi_error_string_19
.quad uefi_error_string_20
.quad uefi_error_string_21
.quad uefi_error_string_22
.quad uefi_error_string_23
.quad uefi_error_string_24
.quad uefi_error_string_25
.quad uefi_error_string_26
.quad uefi_error_string_27
.quad uefi_error_string_28
.quad uefi_error_string_29
.quad uefi_error_string_30
.quad uefi_error_string_31
.quad uefi_error_string_32
.quad uefi_error_string_33
.quad uefi_error_string_34
.quad uefi_error_string_35

uefi_error_string_0: .asciz "Success"
uefi_error_string_1: .asciz "Imaged Failed to Load"
uefi_error_string_2: .asciz "Incorrect Parameter"
uefi_error_string_3: .asciz "Unsupported Operation"
uefi_error_string_4: .asciz "Improper buffer size for operation"
uefi_error_string_5: .asciz "Buffer too small"
uefi_error_string_6: .asciz "No data pending"
uefi_error_string_7: .asciz "Device Error"
uefi_error_string_8: .asciz "Device cannot be written to"
uefi_error_string_9: .asciz "Out of Resources"
uefi_error_string_10: .asciz "The file system was corrupted"
uefi_error_string_11: .asciz "File System Full"
uefi_error_string_12: .asciz "No Media"
uefi_error_string_13: .asciz "The media has changed since last access"
uefi_error_string_14: .asciz "The item was not found"
uefi_error_string_15: .asciz "Access denied"
uefi_error_string_16: .asciz "Server not found or did not respond"
uefi_error_string_17: .asciz "Device has no mapping"
uefi_error_string_18: .asciz "The device has timed out"
uefi_error_string_19: .asciz "The protocol has not started"
uefi_error_string_20: .asciz "The protocol has already started"
uefi_error_string_21: .asciz "The operation was aborted"
uefi_error_string_22: .asciz "An ICMP error occured during the network operation"
uefi_error_string_23: .asciz "A TFTP error occured during the network operation"
uefi_error_string_24: .asciz "A protocol error occured during the network operation"
uefi_error_string_25: .asciz "The function's internal version is incompatable with the requested version"
uefi_error_string_26: .asciz "The function was not performed due to a security violation"
uefi_error_string_27: .asciz "CRC error detected"
uefi_error_string_28: .asciz "Beginning or end of media reached"
uefi_error_string_29: .asciz "Invalid Error"
uefi_error_string_30: .asciz "Invalid Error"
uefi_error_string_31: .asciz "End of file reached"
uefi_error_string_32: .asciz "Invalid languaged specified"
uefi_error_string_33: .asciz "Security status of data compromised"
uefi_error_string_34: .asciz "Address conflict address allocation"
uefi_error_string_35: .asciz "An HTTP error occured during the network operation"


.section bss
image_handle: .skip 8
system_table: .skip 8
putchar.buffer: .skip 4
/* struct {
	uint64_t buffer_size in bytes
	uint64_t descriptor_size in bytes
	uint64_t map_key
	uint64_t map_version
	struct {
		uint32_t type
		uint64_t phys_addr
		uint64_t virt_addr
		uint64_t number_of_pages (4k pages)
		uint64_t attributes
	} descriptors []
} *memory_map_ptr */
memory_map_ptr: .skip 8

.skip 40
print_number.buffer_end:
.skip 2

.align 16
stack_bottom:
.skip STACK_SIZE
stack_top:
