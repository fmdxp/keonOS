/*
 * keonOS - include/kernel/arch/x86_64/thread.h
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

 #ifndef THREAD_H
#define THREAD_H

#include <stdint.h>

enum thread_state_t
{
    THREAD_RUNNING,
    THREAD_READY,
    THREAD_SLEEPING,
    THREAD_BLOCKED,
    THREAD_ZOMBIE
};

#include <fs/vfs_node.h>

struct thread_t 
{
    uint64_t* rsp;
    uint32_t  id;
    char      name[16];
    uint64_t* stack_start;
    uint64_t* user_stack;
    bool      is_user;
    thread_t* next;
    thread_state_t state;
    uint32_t sleep_ticks;
    int      exit_code;
    
    // Virtual Memory Layout
    uintptr_t user_image_start;
    uintptr_t user_image_end;
    uintptr_t user_heap_break;
    uintptr_t dyn_lib_break; // Base address for next dynamic library load
    
    VFSNode* fd_table[16];
    uint32_t fd_offset[16];
};

typedef struct 
{
    volatile int locked;
    uint64_t rflags;
} spinlock_t;


extern "C" void switch_context(uint64_t** old_rsp, uint64_t* new_rsp);

void thread_init();
void idle_task();
extern "C" void yield();
void thread_exit(int code);
thread_t* thread_create(void (*entry_point)(), const char* name);
thread_t* thread_add(void(*entry_point)(), const char* name, bool is_user = false);
bool      thread_kill(uint32_t id);
void      thread_sleep(uint32_t ms);
void      thread_wakeup_blocked();
thread_t* thread_get_current();
thread_t* get_idle_thread_ptr();
void      thread_print_list();
uint32_t  thread_get_id_by_name(const char* name);
void spin_lock(spinlock_t* lock);
void spin_unlock(spinlock_t* lock);
void spin_lock_irqsave(spinlock_t* lock);
void spin_unlock_irqrestore(spinlock_t* lock);
void cleanup_zombies();
int64_t thread_kill_by_string(const char* input);
thread_t* thread_get_by_id(uint32_t id);
void user_test_thread();
thread_t* thread_create_user(void (*entry_point)(), const char* name);

#endif      // THREAD_H