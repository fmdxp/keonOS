/*
 * keonOS - user/libc/sys/kex_syscalls.c
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

#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

// Defined in syscall.asm
extern int64_t syscall0(uint64_t num);
extern int64_t syscall1(uint64_t num, uint64_t a1);
extern int64_t syscall2(uint64_t num, uint64_t a1, uint64_t a2);

// Syscall Numbers
#define SYS_STAT    8
#define SYS_FSTAT   9
#define SYS_GETPID  10
#define SYS_SLEEP   11
#define SYS_LOAD_LIBRARY 20

int stat(const char *path, struct stat *buf) {
    return (int)syscall2(SYS_STAT, (uint64_t)path, (uint64_t)buf);
}

int fstat(int fd, struct stat *buf) {
    return (int)syscall2(SYS_FSTAT, (uint64_t)fd, (uint64_t)buf);
}

int getpid() {
    return (int)syscall0(SYS_GETPID);
}

unsigned int sleep(unsigned int seconds) {
    syscall1(SYS_SLEEP, (uint64_t)(seconds * 1000));
    return 0;
}

// Custom KeonOS extension
void msleep(unsigned int ms) {
    syscall1(SYS_SLEEP, (uint64_t)ms);
}

void* load_library(const char* path) {
    return (void*)syscall1(SYS_LOAD_LIBRARY, (uint64_t)path);
}
