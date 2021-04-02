SECTIONS {
	. = __image_base__ + 0x1000;
	. = ALIGN(0x1000);
	__kernel_start = .;
	.text : {
		__kernel_text_start = .;
		*(.text)
		__kernel_text_end = .;
	}
	.data ALIGN(0x1000) : {
		__kernel_data_start = . ;
		*(.data)
		__kernel_data_end_ = . ;
	}
	.bss ALIGN(0x1000) : {
		__kernel_bss_start = . ;
		*(.bss)
		*(COMMON)
		__kernel_bss_end = . ;
	}
	. = ALIGN(0x1000);
	__kernel_end = .; 
}
