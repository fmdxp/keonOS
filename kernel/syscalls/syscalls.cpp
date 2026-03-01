/*
 * keonOS - kernel/syscalls/syscalls.cpp
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


#include <kernel/syscalls/syscalls.h>
#include <kernel/arch/x86_64/thread.h>
#include <kernel/arch/x86_64/paging.h>
#include <kernel/arch/x86_64/gdt.h>
#include <kernel/panic.h>
#include <kernel/error.h>
#include <string.h>
#include <stdio.h>

static kernel_gs_data gs_ptr;
syscall_fn syscall_table[256];

void syscall_init() 
{
	syscall_table_init();

    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0xC0000080));
    low |= 1;
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(0xC0000080));

    gs_ptr.kernel_stack = kernel_tss.rsp0;

    uint64_t gs_base = (uintptr_t)&gs_ptr;
    asm volatile("wrmsr" : : "a"((uint32_t)gs_base), "d"((uint32_t)(gs_base >> 32)), "c"(0xC0000102));

    uint64_t star = ((uint64_t)0x13 << 48) | ((uint64_t)0x08 << 32);
    asm volatile("wrmsr" : : "a"((uint32_t)star), "d"((uint32_t)(star >> 32)), "c"(0xC0000081));

    uint64_t lstar = (uintptr_t)syscall_entry;
    asm volatile("wrmsr" : : "a"((uint32_t)lstar), "d"((uint32_t)(lstar >> 32)), "c"(0xC0000082));
    asm volatile("wrmsr" : : "a"(0x200), "d"(0), "c"(0xC0000084));
}

void syscall_set_kernel_stack(uint64_t stack)
{
    gs_ptr.kernel_stack = stack;
}

void syscall_table_init() 
{
    memset(syscall_table, 0, sizeof(syscall_table));

    syscall_table[0] = sys_read;
    syscall_table[1] = sys_write;
    syscall_table[2] = sys_open;
    syscall_table[3] = sys_close;
    syscall_table[4] = sys_mkdir;
    syscall_table[5] = sys_uptime;
    syscall_table[6] = sys_unlink;
    syscall_table[7] = sys_readdir;
    syscall_table[8] = sys_stat;
    syscall_table[9] = sys_fstat;
    syscall_table[10] = sys_getpid;
    syscall_table[11] = sys_sleep;
    syscall_table[12] = sys_sbrk;
    
    syscall_table[20] = sys_load_library;
    syscall_table[37] = sys_kill;
    syscall_table[60] = sys_exit;
    syscall_table[100] = sys_vga;
    syscall_table[161] = sys_reboot;
    syscall_table[200] = sys_ps;
}


extern "C" uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)
{
    if (num < 256 && syscall_table[num]) 
    {
        return syscall_table[num](a1, a2, a3, a4, a5, a6);
    }

    printf("\n[SYSCALL] Error: %llu not defined\n", num);
	return (uint64_t)-ENOSYS;
}



extern "C" uint64_t _kernel_virtual_start;
extern "C" uint64_t _kernel_end;
extern void shell_run();

void start_user_code() 
{
    // Launch the shell in User Mode (Ring 3)
    thread_add(shell_run, "shell", false);

    // This setup thread has finished its job
    sys_exit(0, 0, 0, 0, 0, 0);
}

bool copy_from_user(void* dst, const void* src, size_t size)
{
    uintptr_t u = (uintptr_t)src;
    for (size_t i = 0; i < size; i++) 
    {
        if (!paging_is_user_accessible((void*)(u + i))) 
        {
            printf("[SYSCALL] copy_from_user FAIL: 0x%lx\n", u + i);
            return false;
        }
        ((uint8_t*)dst)[i] = ((uint8_t*)src)[i];
    }
    return true;
}

bool copy_to_user(void* dst, const void* src, size_t size)
{
    uintptr_t u = (uintptr_t)dst;
    for (size_t i = 0; i < size; i++) 
    {
        if (!paging_is_user_accessible((void*)(u + i))) 
        {
            printf("[SYSCALL] copy_to_user FAIL: 0x%lx\n", u + i);
            return false;
        }
        ((uint8_t*)dst)[i] = ((uint8_t*)src)[i];
    }
    return true;
}

void user_mode_test() 
{
    const char* user_msg = (const char*)0x900000;

    asm volatile(
        "mov $1, %%rax \n"
        "mov $1, %%rdi \n"      // STDOUT
        "mov %0, %%rsi \n"      // buffer
        "mov $24, %%rdx \n"     // lunghezza stringa
        "syscall \n"
        : : "r"(user_msg) : "rax", "rdi", "rsi", "rdx", "rcx", "r11"
    );

    while(1) { asm volatile("nop"); }
}