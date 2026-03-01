/*
 * keonOS - include/kernel/syscalls/syscalls.h
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


#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <kernel/arch/x86_64/gdt.h>
#include <sys/errno.h>
#include <stdint.h>
#include <stddef.h>

struct kernel_gs_data 
{
    uint64_t kernel_stack;      // Offset 0
	uint64_t user_stack_tmp;	// Offset 8
    uint64_t reserved;		    // Offset 16
} __attribute__((packed));


typedef uint64_t (*syscall_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

extern "C" void jump_to_user(uintptr_t entry_point, uintptr_t stack_ptr);
extern "C" void syscall_entry();
extern "C" tss_entry kernel_tss;

extern "C" uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
bool copy_from_user(void* dst, const void* src, size_t size);
bool copy_to_user(void* dst, const void* src, size_t size);

void syscall_init();
void syscall_table_init();
void syscall_set_kernel_stack(uint64_t stack);

void start_user_code();
void user_mode_test();


// SYS_FILE

uint64_t sys_read(uint64_t fd, uint64_t buf, uint64_t size, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_write(uint64_t fd, uint64_t buf, uint64_t size, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_open(uint64_t path, uint64_t flags, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_close(uint64_t fd, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_mkdir(uint64_t path, uint64_t mode, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_readdir(uint64_t fd, uint64_t index, uint64_t dirent_ptr, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_unlink(uint64_t path, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);


// SYS_PROC

uint64_t sys_exit(uint64_t status, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_uptime(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_ps(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_kill(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_sleep(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
uint64_t sys_sbrk(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
uint64_t sys_load_library(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// process
uint64_t sys_reboot(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_vga(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);

uint64_t sys_stat(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_fstat(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_getpid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t sys_sleep(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);


#endif		// SYSCALLS_H