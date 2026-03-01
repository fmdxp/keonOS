/*
 * keonOS - user/libc/include/stdlib.h
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

#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

void exit(int status);
void* malloc(size_t size);
void free(void* ptr);

char* itoa(unsigned long long value, char* str, int base);
char* ulltoa(unsigned long long value, char* str, int base);
long strtol(const char* str, char** endptr, int base);

#endif
