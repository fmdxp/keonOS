#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int printf(const char* format, ...) 
{
	va_list arg;
	va_start(arg, format);
	int done = vprintf(format, arg);
	va_end(arg);
	return done;
}