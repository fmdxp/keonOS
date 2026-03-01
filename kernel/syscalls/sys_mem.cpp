/*
 * keonOS - kernel/syscalls/sys_mem.cpp
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
#include <stdint.h>
#include <stdio.h>

uint64_t sys_sbrk(uint64_t increment, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) 
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    thread_t* current = thread_get_current();
    if (!current || !current->is_user) return -1;

    uintptr_t old_break = current->user_heap_break;
    if (increment == 0) return old_break;

    uintptr_t new_break = old_break + increment;
    
    // Page-align calculations: we need to ensure all pages from old_break to new_break are USER accessible
    uintptr_t start_page = old_break & ~0xFFFULL;
    uintptr_t end_page = (new_break + 4095) & ~0xFFFULL;

    for (uintptr_t addr = start_page; addr < end_page; addr += PAGE_SIZE) 
    {
        if (!paging_is_user_accessible((void*)addr)) 
        {
            void* frame = paging_get_physical_address((void*)addr);
            if (!frame) frame = pfa_alloc_frame();
            if (!frame) return -1; 
            
            paging_map_page((void*)addr, frame, PTE_PRESENT | PTE_RW | PTE_USER);
        }
    }

    // Full TLB flush to be safe
    uintptr_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");

    current->user_heap_break = new_break;
    return old_break;
}
