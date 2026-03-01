 /*
 * keonOS - include/exec/kex_loader.h
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

#ifndef KEX_LOADER_H
#define KEX_LOADER_H

#include <stdint.h>
#include <exec/elf.h>
#include <kernel/arch/x86_64/thread.h>

// Loads and executes a KEX file
// Returns true on success (does not return ideally as it jumps to user mode), 
// false on failure.
// Returns thread ID on success, -1 on failure
int kex_load(const char* path, int argc, char** argv);
uintptr_t kdl_load(const char* path, thread_t* t);

#endif // KEX_LOADER_H
