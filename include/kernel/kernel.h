/*
 * keonOS - include/kernel/kernel.h
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

#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <drivers/multiboot2.h>

#if defined(__linux__)
#error "You are not using a cross-compiler, keonOS can't compile. Aborting..."
#endif

#if !defined(__x86_64__)
#error "keonOS needs to be compiled with a x86-elf (cross-)compiler. Aborting..."
#endif

void init_file_system(void* ramdisk_vaddr);
extern "C" void kernel_main(uint64_t magic, uint64_t multiboot_phys_addr);

#endif		// KERNEL_H