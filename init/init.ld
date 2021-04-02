ENTRY(_start)
SECTIONS {
	__executable_start = .;
	.text ALIGN(0x1000) : {
		__text_start_ = .;
		*(.text)
		__text_end = .;
	}
	.data ALIGN(0x1000) : {
		__data_start_ = . ;
		*(.data)
		__data_end_ = . ;
	}
	.bss ALIGN(0x1000) : {
		__bss_start_ = . ;
		*(.bss)
		*(COMMON)
		__bss_end_ = . ;
	}
	. = ALIGN(0x1000);
	__executable_end = .; 
}
