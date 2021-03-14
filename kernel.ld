SECTIONS {
	. = __image_base__ + 0x1000;
	. = ALIGN(0x1000);
	__kernel_start = .;
	.text : {
		__kernel_text_start_ = .;
		*(.text)
		__kernel_text_end = .;
	}
	.data ALIGN(0x1000) : {
		__kernel_data_start_ = . ;
		*(.data)
		__kernel_data_end_ = . ;
	}
	.bss ALIGN(0x1000) : {
		__kernel_bss_start_ = . ;
		*(.bss)
		*(COMMON)
		__kernel_bss_end_ = . ;
	}
	. = ALIGN(0x1000);
	__kernel_end = .; 
}
