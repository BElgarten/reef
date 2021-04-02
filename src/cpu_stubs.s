.extern interrupt_handler
.extern syscall_handler

.macro store_registers
	movq %rax, registers.rax(%rip)
	movq %rbx, registers.rbx(%rip)
	movq %rcx, registers.rcx(%rip)
	movq %rdx, registers.rdx(%rip)
	movq %rsi, registers.rsi(%rip)
	movq %rdi, registers.rdi(%rip)
	movq %rsp, registers.rsp(%rip)
	movq %rbp, registers.rbp(%rip)
	movq %r8, registers.r8(%rip)
	movq %r9, registers.r9(%rip)
	movq %r10, registers.r10(%rip)
	movq %r11, registers.r11(%rip)
	movq %r12, registers.r12(%rip)
	movq %r13, registers.r13(%rip)
	movq %r14, registers.r14(%rip)
	movq %r15, registers.r15(%rip)
	movq %cr2, %rax
	movq %rax, registers.cr2(%rip)
.endm

.macro load_registers
	movq registers.rax(%rip), %rax 
	movq registers.rbx(%rip), %rbx 
	movq registers.rcx(%rip), %rcx 
	movq registers.rdx(%rip), %rdx 
	movq registers.rsi(%rip), %rsi 
	movq registers.rdi(%rip), %rdi 
	movq registers.rsp(%rip), %rsp 
	movq registers.rbp(%rip), %rbp 
	movq registers.r8(%rip), %r8 
	movq registers.r9(%rip), %r9 
	movq registers.r10(%rip), %r10 
	movq registers.r11(%rip), %r11 
	movq registers.r12(%rip), %r12 
	movq registers.r13(%rip), %r13 
	movq registers.r14(%rip), %r14 
	movq registers.r15(%rip), %r15 
.endm

.macro idt_stub n
.global idt_stub_\n
idt_stub_\n:
	store_registers

	movq $\n, %rcx
	leaq registers(%rip), %rdx
	xorq %r8, %r8
	call interrupt_handler
	load_registers
	iretq
.endm

.macro idt_stub_error_code n
.global idt_stub_\n
idt_stub_\n:
	store_registers

	movq $\n, %rcx
	leaq registers(%rip), %rdx
	popq %r8
	call interrupt_handler
	load_registers
	iretq
.endm

.section .text
.global syscall_stub
.extern syscall_handler;
syscall_stub:
	store_registers
	movabsq $0xffffffffe0000000, %rsp
	movq %rsp, %rbp
	leaq registers(%rip), %rcx
	call syscall_handler
	load_registers
	sysret

idt_stub 0
idt_stub 1
idt_stub 2
idt_stub 3
idt_stub 4
idt_stub 5
idt_stub 6
idt_stub 7
idt_stub_error_code 8
idt_stub 9
idt_stub_error_code 10
idt_stub_error_code 11
idt_stub_error_code 12
idt_stub_error_code 13
idt_stub_error_code 14
idt_stub 15
idt_stub 16
idt_stub_error_code 17
idt_stub 18
idt_stub 19
idt_stub 20
idt_stub 21
idt_stub 22
idt_stub 23
idt_stub 24
idt_stub 25
idt_stub 26
idt_stub 27
idt_stub 28
idt_stub 29
idt_stub 30
idt_stub 31
idt_stub 32
idt_stub 33
idt_stub 34
idt_stub 35
idt_stub 36
idt_stub 37
idt_stub 38
idt_stub 39
idt_stub 40
idt_stub 41
idt_stub 42
idt_stub 43
idt_stub 44
idt_stub 45
idt_stub 46
idt_stub 47
idt_stub 48
idt_stub 49
idt_stub 50
idt_stub 51
idt_stub 52
idt_stub 53
idt_stub 54
idt_stub 55
idt_stub 56
idt_stub 57
idt_stub 58
idt_stub 59
idt_stub 60
idt_stub 61
idt_stub 62
idt_stub 63
idt_stub 64
idt_stub 65
idt_stub 66
idt_stub 67
idt_stub 68
idt_stub 69
idt_stub 70
idt_stub 71
idt_stub 72
idt_stub 73
idt_stub 74
idt_stub 75
idt_stub 76
idt_stub 77
idt_stub 78
idt_stub 79
idt_stub 80
idt_stub 81
idt_stub 82
idt_stub 83
idt_stub 84
idt_stub 85
idt_stub 86
idt_stub 87
idt_stub 88
idt_stub 89
idt_stub 90
idt_stub 91
idt_stub 92
idt_stub 93
idt_stub 94
idt_stub 95
idt_stub 96
idt_stub 97
idt_stub 98
idt_stub 99
idt_stub 100
idt_stub 101
idt_stub 102
idt_stub 103
idt_stub 104
idt_stub 105
idt_stub 106
idt_stub 107
idt_stub 108
idt_stub 109
idt_stub 110
idt_stub 111
idt_stub 112
idt_stub 113
idt_stub 114
idt_stub 115
idt_stub 116
idt_stub 117
idt_stub 118
idt_stub 119
idt_stub 120
idt_stub 121
idt_stub 122
idt_stub 123
idt_stub 124
idt_stub 125
idt_stub 126
idt_stub 127
idt_stub 128
idt_stub 129
idt_stub 130
idt_stub 131
idt_stub 132
idt_stub 133
idt_stub 134
idt_stub 135
idt_stub 136
idt_stub 137
idt_stub 138
idt_stub 139
idt_stub 140
idt_stub 141
idt_stub 142
idt_stub 143
idt_stub 144
idt_stub 145
idt_stub 146
idt_stub 147
idt_stub 148
idt_stub 149
idt_stub 150
idt_stub 151
idt_stub 152
idt_stub 153
idt_stub 154
idt_stub 155
idt_stub 156
idt_stub 157
idt_stub 158
idt_stub 159
idt_stub 160
idt_stub 161
idt_stub 162
idt_stub 163
idt_stub 164
idt_stub 165
idt_stub 166
idt_stub 167
idt_stub 168
idt_stub 169
idt_stub 170
idt_stub 171
idt_stub 172
idt_stub 173
idt_stub 174
idt_stub 175
idt_stub 176
idt_stub 177
idt_stub 178
idt_stub 179
idt_stub 180
idt_stub 181
idt_stub 182
idt_stub 183
idt_stub 184
idt_stub 185
idt_stub 186
idt_stub 187
idt_stub 188
idt_stub 189
idt_stub 190
idt_stub 191
idt_stub 192
idt_stub 193
idt_stub 194
idt_stub 195
idt_stub 196
idt_stub 197
idt_stub 198
idt_stub 199
idt_stub 200
idt_stub 201
idt_stub 202
idt_stub 203
idt_stub 204
idt_stub 205
idt_stub 206
idt_stub 207
idt_stub 208
idt_stub 209
idt_stub 210
idt_stub 211
idt_stub 212
idt_stub 213
idt_stub 214
idt_stub 215
idt_stub 216
idt_stub 217
idt_stub 218
idt_stub 219
idt_stub 220
idt_stub 221
idt_stub 222
idt_stub 223
idt_stub 224
idt_stub 225
idt_stub 226
idt_stub 227
idt_stub 228
idt_stub 229
idt_stub 230
idt_stub 231
idt_stub 232
idt_stub 233
idt_stub 234
idt_stub 235
idt_stub 236
idt_stub 237
idt_stub 238
idt_stub 239
idt_stub 240
idt_stub 241
idt_stub 242
idt_stub 243
idt_stub 244
idt_stub 245
idt_stub 246
idt_stub 247
idt_stub 248
idt_stub 249
idt_stub 250
idt_stub 251
idt_stub 252
idt_stub 253
idt_stub 254
idt_stub 255

.section .bss
registers:
registers.rax: .skip 8
registers.rbx: .skip 8
registers.rcx: .skip 8
registers.rdx: .skip 8
registers.rsi: .skip 8
registers.rdi: .skip 8
registers.rsp: .skip 8
registers.rbp: .skip 8
registers.r8: .skip 8
registers.r9: .skip 8
registers.r10: .skip 8
registers.r11: .skip 8
registers.r12: .skip 8
registers.r13: .skip 8
registers.r14: .skip 8
registers.r15: .skip 8
registers.cr2: .skip 8
