/*
 * keonOS - user/libc/stdio/printf.c
 * Copyright (C) 2025-2026 fmdxp
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ADDITIONAL TERMS (Per Section 7 of the GNU GPLv3):
 * - Original author attributions must be preserved in all copies.
 * - Modified versions must be marked as different from the original.
 * - The name "keonOS" or "fmdxp" cannot be used for publicity without permission.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

int putchar(int c) {
    char ch = (char)c;
    write(1, &ch, 1);
    return c;
}

int puts(const char* s) {
    size_t len = strlen(s);
    write(1, s, len);
    putchar('\n');
    return (int)len + 1;
}

static void print_str(const char* str, size_t len) {
    write(1, str, len);
}

int vprintf(const char* format, va_list arg) {
    int written = 0;
    char buffer[64];

    while (*format) {
        if (*format != '%') {
            const char* start = format;
            while (*format && *format != '%') format++;
            print_str(start, format - start);
            written += (format - start);
            continue;
        }

        format++; // Skip '%'

        if (*format == '%') {
            putchar('%');
            written++;
            format++;
            continue;
        }

        if (*format == 's') {
            const char* s = va_arg(arg, const char*);
            if (!s) s = "(null)";
            size_t len = strlen(s);
            print_str(s, len);
            written += len;
        } else if (*format == 'd' || *format == 'i') {
            int val = va_arg(arg, int);
            // Handle negative manually or use itoa
            if (val < 0) {
                 putchar('-');
                 written++;
                 val = -val;
            }
            itoa(val, buffer, 10);
            size_t len = strlen(buffer);
            print_str(buffer, len);
            written += len;
        } else if (*format == 'x') {
            unsigned int val = va_arg(arg, unsigned int);
            itoa(val, buffer, 16);
            size_t len = strlen(buffer);
            print_str(buffer, len);
            written += len;
        } else if (*format == 'p') {
            unsigned long long val = (uintptr_t)va_arg(arg, void*);
            print_str("0x", 2);
            written += 2;
            ulltoa(val, buffer, 16);
            size_t len = strlen(buffer);
            print_str(buffer, len);
            written += len;
        } else if (*format == 'c') {
            char c = (char)va_arg(arg, int);
            putchar(c);
            written++;
        }

        format++;
    }
    return written;
}

int printf(const char* format, ...) {
    va_list arg;
    va_start(arg, format);
    int done = vprintf(format, arg);
    va_end(arg);
    return done;
}
