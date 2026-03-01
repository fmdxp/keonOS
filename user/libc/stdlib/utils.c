/*
 * keonOS - user/libc/stdlib/utils.c
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

#include <stdlib.h>
#include <stdint.h>

char* itoa(unsigned long long value, char* str, int base) {
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    unsigned long long tmp_value;

    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return str;
    }

    while (value != 0) {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value % base];
    }

    *ptr-- = '\0';
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    return str;
}

char* ulltoa(unsigned long long value, char* str, int base) {
    return itoa(value, str, base);
}

long strtol(const char* str, char** endptr, int base) {
    long result = 0;
    int sign = 1;
    
    while (*str == ' ' || *str == '\t' || *str == '\n') str++;
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    if (base == 0) {
        if (*str == '0') {
            if (*(str+1) == 'x' || *(str+1) == 'X') {
                base = 16;
                str += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (*str == '0' && (*(str+1) == 'x' || *(str+1) == 'X')) {
            str += 2;
        }
    }
    
    while (*str) {
        int v;
        if (*str >= '0' && *str <= '9') v = *str - '0';
        else if (*str >= 'a' && *str <= 'z') v = *str - 'a' + 10;
        else if (*str >= 'A' && *str <= 'Z') v = *str - 'A' + 10;
        else break;
        
        if (v >= base) break;
        
        result = result * base + v;
        str++;
    }
    
    if (endptr) *endptr = (char*)str;
    return result * sign;
}
