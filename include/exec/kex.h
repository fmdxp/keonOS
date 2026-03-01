 /*
 * keonOS - include/exec/kex.h
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

#ifndef KEX_H
#define KEX_H

#include <exec/elf.h>

// Note Segment details
#define KEX_NOTE_NAME       "KeonOS"
#define KEX_NOTE_TYPE_VERSION 0x1001

// The custom payload inside the note descriptor
struct KexHeader {
    char verify[4];      // Must be "KEX1"
    uint32_t flags;      // Feature flags
    uint32_t stack_size; // Requested stack size (0 = default 1MB)
    uint32_t heap_size;  // Requested initial heap size using brk (0 = none)
    uint32_t capabilities; // Capability bitmask
} __attribute__((packed));

// Flags
#define KEX_FLAG_STATIC     0x0001
#define KEX_FLAG_GUI        0x0002

// Capabilities
#define KEX_CAP_NET         0x0001
#define KEX_CAP_FS_READ     0x0002
#define KEX_CAP_FS_WRITE    0x0004

#endif // KEX_H
