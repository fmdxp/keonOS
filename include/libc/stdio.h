#ifndef _STDIO_H
#define _STDIO_H

#include <libc/sys/cdefs.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	int fd;
	uint32_t offset;
	uint32_t size;
	int error;
} FILE;

FILE* fopen(const char* filename, const char* mode);
int fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
void rewind(FILE* stream);

int printf(const char* format, ...);
int vprintf(const char* format, va_list arg);
int putchar(int);
int puts(const char*);

#ifdef __cplusplus
}
#endif

#endif		// _STDIO_H