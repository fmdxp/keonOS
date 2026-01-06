#ifndef _STDLIB_H
#define _STDLIB_H

#include <sys/cdefs.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EOVERFLOW 	75
#define EINVAL		22
#define ENOMEM		12

extern uintptr_t __stack_chk_guard;
__attribute__((noreturn)) void __stack_chk_fail(void);

__attribute__((__noreturn__))
void abort(void);

char* itoa(int value, char* str, int base);
char* utoa(unsigned int value, char* str, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);
int atoi(const char* str);


extern int errno;

#ifdef __cplusplus
}
#endif

#endif