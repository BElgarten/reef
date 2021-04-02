#ifndef _UTIL_H_
#define _UTIL_H_
#include <stddef.h>

void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, void *src, size_t n);
int memcmp(void *a, void *b, size_t n);

#endif/*_UTIL_H_*/
