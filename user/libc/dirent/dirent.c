/*
 * keonOS - user/libc/dirent/dirent.c
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

#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

extern long syscall3(long n, long a1, long a2, long a3);

DIR* opendir(const char* name) {
    int fd = open(name, 0);
    if (fd < 0) return NULL;

    DIR* dir = malloc(sizeof(DIR));
    if (!dir) {
        close(fd);
        return NULL;
    }

    dir->fd = fd;
    dir->index = 0;
    return dir;
}

struct dirent* readdir(DIR* dir) {
    if (!dir) return NULL;

    // sys_readdir(fd, index, dirent_ptr)
    long ret = syscall3(SYS_READDIR, (long)dir->fd, (long)dir->index, (long)&dir->entry);
    
    if (ret <= 0) return NULL;

    dir->index++;
    return &dir->entry;
}

int closedir(DIR* dir) {
    if (!dir) return -1;
    int ret = close(dir->fd);
    free(dir);
    return ret;
}
