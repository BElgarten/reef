#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include "bootstrap.h"

void plot_pixel(size_t x, size_t y, uint32_t color) {
	size_t idx;
	idx = y * bootstrap_info.framebuffer.pitch + x;
	bootstrap_info.framebuffer.buffer[idx] = color;
}

uint32_t create_color(uint8_t r, uint8_t g, uint8_t b) {
	if (bootstrap_info.framebuffer.format == RGBX)
		return (uint32_t) r | (uint32_t) g << 8 | (uint32_t) b << 16;
	else
		return (uint32_t) b | (uint32_t) g << 8 | (uint32_t) r << 16;
}

void plot_glyph(size_t x, size_t y, unsigned char glyph, uint32_t fg, uint32_t bg) {
	size_t xoff, yoff;
	uint8_t *bitmap;
	size_t height, width, charsize;
	uint32_t color;
	size_t bitnum, bytenum;

	height = bootstrap_info.framebuffer.font.height;
	width = bootstrap_info.framebuffer.font.width;
	charsize = bootstrap_info.framebuffer.font.charsize;

	bitmap = bootstrap_info.framebuffer.font.bitmaps + (size_t) glyph * charsize;

	for (yoff = 0; yoff < height; yoff++) {
		for (xoff = 0; xoff < width; xoff++) {
			bytenum = (width * yoff + (width - xoff)) / 8;
			bitnum = (width * yoff + (width - xoff)) % 8;
			color = bitmap[bytenum] & (1 << bitnum) ? fg : bg;
			plot_pixel(x + xoff, y + yoff, color);
		}
	}

}

size_t get_console_width(void) {
	return bootstrap_info.framebuffer.width / bootstrap_info.framebuffer.font.width;
}

size_t get_console_height(void) {
	return bootstrap_info.framebuffer.height / bootstrap_info.framebuffer.font.height;
}

struct {
	uint32_t foreground;
	uint32_t background;
	size_t x, y;
} console_status = { 0xffffff, 0x000000, 0, 0 };

static void console_scroll(void) {
	console_status.y = 0;
}

static void console_newline(void) {
	size_t fontheight;
	size_t fbheight;

	fontheight = bootstrap_info.framebuffer.font.height;
	fbheight = bootstrap_info.framebuffer.height;

	console_status.x = 0;
	console_status.y++;
	if (console_status.y * fontheight + fontheight - 1 > fbheight)
		console_scroll();
}

void putchar_literal(unsigned char c) {
	size_t fontheight, fontwidth;
	size_t fbwidth, fbheight;
	size_t x, y;

	fbwidth = bootstrap_info.framebuffer.width;
	fbheight = bootstrap_info.framebuffer.height;
	fontwidth = bootstrap_info.framebuffer.font.width;
	fontheight = bootstrap_info.framebuffer.font.height;

	x = console_status.x * fontwidth;
	y = console_status.y * fontheight;
	plot_glyph(x, y, c, console_status.foreground, console_status.background);

	console_status.x++;
	if (console_status.x * fontwidth + fontwidth - 1 > fbwidth)
		console_newline();
}


#define TABSTOP 8
static void console_tab(void) {
	size_t nspaces;
	nspaces = TABSTOP - console_status.x % TABSTOP;
	while (nspaces--)
		putchar_literal(' ');
}

void putchar(int c) {
	unsigned char uc;

	uc = c;
	switch (uc) {
		case '\n':
			console_newline();
			break;
		case '\t':
			console_tab();
			break;
		default:
			putchar_literal(uc);
			break;
	} 
}

void puts(const char *s) {
	for (; *s; s++)
		putchar(*s);
}

void clear_screen(uint32_t color) {
	size_t fbwidth, fbheight;
	size_t x,y;

	fbwidth = bootstrap_info.framebuffer.width;
	fbheight = bootstrap_info.framebuffer.height;
	for (y = 0; y < fbheight; y++)
		for (x = 0; x < fbwidth; x++)
			plot_pixel(x, y, color);
}

static char *digits = "0123456789abcdef";
/* 2 <= base <= 16*/
static void print_unsigned_base(uintmax_t n, uint8_t base) {
	uintmax_t divisor;
	if (n == 0) {
		putchar('0');
		return;
	}
	if (base < 2 || base > 16) {
		printf("print_unsigned_base(): Invalid base %d, must be betweeen 2 and 16\n", base);
		return;
	}

	divisor = 1;
	while (n / divisor >= base)
		divisor *= base;
	while (divisor) {
		putchar(digits[n / divisor]);
		n %= divisor;
		divisor /= base;
	}
}

static void print_integer_base(intmax_t n, uint8_t base) {
	if (n < 0)
		putchar('-');
	print_unsigned_base(-n, base);
}

static void print_pointer(void *p) {
	uintptr_t n;
	int shift;

	puts("0x");
	n = (uintptr_t) p;
	for (shift = sizeof(uintptr_t) * 8 - 4; shift >= 0; shift -= 4)
		putchar(digits[(n >> shift) & 0xf]);
}

enum printf_length {
	PRINTF_LENGTH_DEFAULT,
	PRINTF_LENGTH_HALF,
	PRINTF_LENGTH_HALF_HALF,
	PRINTF_LENGTH_LONG,
	PRINTF_LENGTH_LONG_LONG,
	PRINTF_LENGTH_INTMAX_T,
	PRINTF_LENGTH_SIZE_T,
	PRINTF_LENGTH_PTRDIFF_T,
	PRINTF_LENGTH_LONG_DOUBLE
};

static enum printf_length get_printf_length_modifier(const char **fmt) {
	switch (*(*fmt)++) {
		case 'h':
			if (**fmt == 'h') {
				(*fmt)++;
				return PRINTF_LENGTH_HALF_HALF;
			} else {
				return PRINTF_LENGTH_HALF;
			}
		case 'l':
			if (**fmt == 'l') {
				(*fmt)++;
				return PRINTF_LENGTH_LONG_LONG;
			} else {
				return PRINTF_LENGTH_LONG;
			}
		case 'j':
			return PRINTF_LENGTH_INTMAX_T;
		case 'z':
			return PRINTF_LENGTH_SIZE_T;
		case 't':
			return PRINTF_LENGTH_PTRDIFF_T;
		case 'L':
			return PRINTF_LENGTH_LONG_DOUBLE;
		default:
			(*fmt)--;
			return PRINTF_LENGTH_DEFAULT;
	}
}



enum printf_argument {
	PRINTF_ARGUMENT_NONE,
	PRINTF_ARGUMENT_POINTER,
	PRINTF_ARGUMENT_UNSIGNED,
	PRINTF_ARGUMENT_INTEGER
};

static enum printf_argument get_printf_argument_type(char c) {
	switch (c) {
		case 'p':
		case 's':
			return PRINTF_ARGUMENT_POINTER;
		case 'u':
		case 'x':
		case 'O':
		case 'o':
			return PRINTF_ARGUMENT_UNSIGNED;
		case 'c':
		case 'd':
		case 'i':
			return PRINTF_ARGUMENT_INTEGER;
		case '%':
		default:
			return PRINTF_ARGUMENT_NONE;
	}
}

void get_printf_argument(enum printf_length length, enum printf_argument type,
	 uintmax_t *uarg, intmax_t *iarg, void **ptr, va_list *args) {
	if (type == PRINTF_ARGUMENT_POINTER) {
		*ptr = va_arg(*args, void *);
	} else if (type == PRINTF_ARGUMENT_UNSIGNED) {
		if(length == PRINTF_LENGTH_HALF_HALF)
			*uarg = (unsigned char) va_arg(*args, unsigned);
		else if (length == PRINTF_LENGTH_HALF)
			*uarg = (unsigned short) va_arg(*args, unsigned);
		else if (length == PRINTF_LENGTH_LONG)
			*uarg = va_arg(*args, unsigned long);
		else if (length == PRINTF_LENGTH_LONG_LONG)
			*uarg = va_arg(*args, unsigned long long);
		else if (length == PRINTF_LENGTH_INTMAX_T)
			*uarg = va_arg(*args, uintmax_t);
		else if (length == PRINTF_LENGTH_SIZE_T)
			*uarg = va_arg(*args, size_t);
		else if (length == PRINTF_LENGTH_PTRDIFF_T)
			*uarg = va_arg(*args, ptrdiff_t);
		else
			*uarg = va_arg(*args, unsigned);
			
	} else if (type == PRINTF_ARGUMENT_INTEGER) {
		if(length == PRINTF_LENGTH_HALF_HALF)
			*iarg = (signed char) va_arg(*args, int);
		else if (length == PRINTF_LENGTH_HALF)
			*iarg = (short) va_arg(*args, int);
		else if (length == PRINTF_LENGTH_LONG)
			*iarg = va_arg(*args, long);
		else if (length == PRINTF_LENGTH_LONG_LONG)
			*iarg = va_arg(*args, long long);
		else if (length == PRINTF_LENGTH_INTMAX_T)
			*iarg = va_arg(*args, intmax_t);
		else if (length == PRINTF_LENGTH_SIZE_T)
			*iarg = va_arg(*args, size_t);
		else if (length == PRINTF_LENGTH_PTRDIFF_T)
			*iarg = va_arg(*args, ptrdiff_t);
		else
			*iarg = va_arg(*args, int);
	}
}

void vprintf(const char *fmt, va_list args) {
	void *ptr;
	uintmax_t uarg;
	intmax_t iarg;
	enum printf_length length;
	enum printf_argument argtype;

	for (; *fmt; fmt++) {
		if (*fmt != '%') {
			putchar(*fmt);
			continue;
		}

		fmt++;
		length = get_printf_length_modifier(&fmt);
		argtype = get_printf_argument_type(*fmt);
		get_printf_argument(length, argtype, &uarg, &iarg, &ptr, &args);

		switch (*fmt) {
			case '%':
				putchar('%');
				break;
			case 's':
				if (ptr)
					puts(ptr);
				else
					puts("(null)");
				break;
			case 'd':
			case 'i':
				print_integer_base(iarg, 10);
				break;
			case 'u':
				print_unsigned_base(uarg, 10);
				break;
			case 'x':
			case 'X':
				print_unsigned_base(uarg, 16);
				break;
			case 'o':
				print_unsigned_base(uarg, 8);
				break;
			case 'c':
				putchar(iarg);
				break;
			case 'p':
				print_pointer(ptr);
				break;
			default:
				break;
		}
	}
}

void printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

void set_console_foreground(uint32_t color) {
	console_status.foreground = color;
}

void set_console_background(uint32_t color) {
	console_status.background = color;
}

void panic(const char *msg, ...) {
	va_list args;
	set_console_foreground(create_color(255, 0, 0));
	set_console_background(create_color(0, 0, 0));

	va_start(args, msg);
	vprintf(msg, args);
	va_end(args);
	for (;;)
		;
}
