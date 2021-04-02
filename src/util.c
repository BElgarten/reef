#include <stddef.h>
#include "util.h"

void *memset(void *dst, int c, size_t n) {
	char *cdst = dst;
	while (n--)
		*cdst++ = c;
	return dst;
}

void *memcpy(void *dst, void *src, size_t n) {
	char *cdst = dst;
	char *csrc = src;
	while (n--)
		*cdst++ = *csrc++;
	return dst;
}

int memcmp(void *a, void *b, size_t n) {
	unsigned char *ca = a;
	unsigned char *cb = b;
	for (; n; n--, ca++, cb++)
		if (*ca != *cb)
			return *cb - *ca;
	return 0;
}
