#ifndef _STDIO_H
#define _STDIO_H

#include <libc/sys/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

int printf(const char* format, ...);
int vprintf(const char* format, va_list arg);
int putchar(int);
int puts(const char*);

#ifdef __cplusplus
}
#endif

#endif		// _STDIO_H