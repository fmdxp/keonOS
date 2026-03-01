/*
 * keonOS - user/libc/include/dirent.h
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

#ifndef _DIRENT_H
#define _DIRENT_H

#include <stdint.h>

#define DT_UNKNOWN 0
#define DT_REG     1
#define DT_DIR     2
#define DT_CHR     3

struct dirent {
    char d_name[128];
    uint32_t d_ino;
    uint32_t d_type;
};

// Simple DIR structure for opendir/readdir
typedef struct {
    int fd;
    int index;
    struct dirent entry;
} DIR;

DIR* opendir(const char* name);
struct dirent* readdir(DIR* dirp);
int closedir(DIR* dirp);

#endif
