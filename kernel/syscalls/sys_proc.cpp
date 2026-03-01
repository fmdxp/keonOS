/*
 * keonOS - kernel/syscalls/sys_proc.cpp
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


#include <kernel/arch/x86_64/thread.h>
#include <kernel/syscalls/syscalls.h>
#include <kernel/arch/x86_64/paging.h>
#include <exec/kex_loader.h>
#include <mm/heap.h>
#include <drivers/timer.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>
#include <stdio.h>
#include <string.h>

uint64_t sys_exit(uint64_t status, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) 
{
    int exit_status = (int)status;
    thread_t* current = thread_get_current();

    if (current && current->is_user) 
        asm volatile("swapgs");

    thread_exit(exit_status);
    __builtin_unreachable();
    return 0;
}

uint64_t sys_uptime(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
    return timer_get_ticks();
}

uint64_t sys_ps(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
    thread_print_list();
    return 0;
}

uint64_t sys_kill(uint64_t id_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
    char kbuf[64];
    if (!copy_from_user(kbuf, (const void*)id_ptr, 63)) return -1;
    kbuf[63] = '\0';
    return (uint64_t)thread_kill_by_string(kbuf);
}

uint64_t sys_reboot(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
    asm volatile("cli");
    
    // 8042 reset
    outb(0x64, 0xFE);
    
    // PS/2 reset (fallback)
    for (volatile int i = 0; i < 1000000; i++);
    outb(0xCF9, 0x06);
    
    // Triple fault (final fallback)
    struct 
    {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) invalid_idt = {0, 0};
    
    asm volatile("lidt %0" : : "m" (invalid_idt));
    asm volatile("int $0x00");
    
    while (1) asm volatile("hlt");
    return 0;
}

uint64_t sys_getpid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) 
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    thread_t* current = thread_get_current();
    if (!current) return -1;
    return current->id;
}

uint64_t sys_sleep(uint64_t ms, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) 
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    thread_sleep(ms);
    return 0;
}

uint64_t sys_load_library(uint64_t path_ptr, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    
    char path[256];
    if (!copy_from_user(path, (const void*)path_ptr, 255)) return 0;
    path[255] = '\0';
    
    thread_t* current = thread_get_current();
    if (!current) return 0;

    return (uint64_t)kdl_load(path, current);
}