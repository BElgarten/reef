#ifndef _TERMINAL_H_
#define _TERMINAL_H_
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

void plot_pixel(size_t x, size_t y, uint32_t color);
uint32_t create_color(uint8_t r, uint8_t g, uint8_t b);
void clear_screen(uint32_t color);
void plot_glyph(size_t x, size_t y, unsigned char glyph, uint32_t fg, uint32_t bg);

size_t get_console_width(void);
size_t get_console_height(void);
void set_console_foreground(uint32_t color);
void set_console_background(uint32_t color);

void putchar_literal(unsigned char c);
void putchar(int c);
void puts(const char *s);

void vprintf(const char *fmt, va_list args);
void printf(const char *fmt, ...);

void panic(const char *msg, ...);

#endif/*_TERMINAL_H_*/
