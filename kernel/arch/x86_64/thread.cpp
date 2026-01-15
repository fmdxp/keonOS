/*
 * keonOS - kernel/arch/x86_64/thread.cpp
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
#include <kernel/constants.h>
#include <kernel/panic.h>
#include <kernel/error.h>
#include <mm/heap.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static thread_t* current_thread = nullptr;
static thread_t* idle_thread_ptr = nullptr;
static uint32_t next_thread_id = 0;

extern "C" void switch_context(uint64_t** old_rsp, uint64_t* new_rsp);


void cleanup_zombies() 
{
    if (!current_thread) return;

    thread_t* prev = idle_thread_ptr;
    thread_t* curr = idle_thread_ptr->next;
    uint32_t started_id = idle_thread_ptr->id;

    do 
    {
        if (curr->state == THREAD_ZOMBIE) 
        {
            prev->next = curr->next;
            
            printf("[reaper] Thread %d (%s) cleaned up. Exit code: %d\n", 
                   curr->id, curr->name, curr->exit_code);

            if (curr->stack_start) kfree(curr->stack_start);
            thread_t* to_free = curr;
            curr = curr->next; 
            kfree(to_free);
            continue; 
        }
        prev = curr;
        curr = curr->next;
    } while (curr != idle_thread_ptr);
}


void thread_init()
{
    current_thread = (thread_t*)kmalloc(sizeof(thread_t));
    memset(current_thread, 0, sizeof(thread_t));
    
    current_thread->id = next_thread_id++;
    current_thread->state = THREAD_READY;
    current_thread->next = current_thread;
    current_thread->stack_start = nullptr; 

    strcpy(current_thread->name, "kernel");
    
    idle_thread_ptr = thread_create(idle_task, "sys_idle");
    if (idle_thread_ptr)
    {
        idle_thread_ptr->next = current_thread->next;
        current_thread->next = idle_thread_ptr;
    }
}

void spin_lock_irqsave(spinlock_t* lock)
{
    uint64_t rflags;
    asm volatile("pushfq; popq %0; cli" : "=rm"(rflags));

    while (__sync_lock_test_and_set(&lock->locked, 1))
        asm volatile("pause");
    
    lock->rflags = rflags;
}

void spin_unlock_irqrestore(spinlock_t* lock) 
{
    uint64_t rflags = lock->rflags;
    __sync_lock_release(&lock->locked);
    asm volatile("pushq %0; popfq" : : "rm"(rflags));
}

void spin_lock(spinlock_t* lock) 
{
    while (__sync_lock_test_and_set(&lock->locked, 1)) asm volatile("pause");
}

void spin_unlock(spinlock_t* lock) 
{
    __sync_lock_release(&lock->locked);
}


thread_t* thread_add(void(*entry_point)(), const char* name)
{
    asm volatile("cli");
    thread_t* t = thread_create(entry_point, name);
    if (t)
    {
        t->id = next_thread_id++;
        t->next = current_thread->next;
        current_thread->next = t;
    }
    asm volatile("sti");
    return t;
}

extern "C" void yield()
{
    if (!current_thread) return;

    asm volatile("cli");
    
    thread_t* prev = current_thread;
    thread_t* next_to_run = nullptr;
    
    thread_t* scan = prev->next;
    do 
    {
        if (scan->state == THREAD_SLEEPING && scan->sleep_ticks == 0)
            scan->state = THREAD_READY;
        scan = scan->next;
    } while (scan != prev->next);
    
    scan = prev->next;
    do 
    {
        if (scan->state == THREAD_READY && scan != idle_thread_ptr) 
        {
            next_to_run = scan;
            break;
        }
        scan = scan->next;
    } while (scan != prev->next);

    if (!next_to_run) 
    {
        if (prev->state == THREAD_READY) next_to_run = prev;
        else next_to_run = idle_thread_ptr;
    }
    
    if (next_to_run != prev) 
    {
        current_thread = next_to_run;
        switch_context(&(prev->rsp), next_to_run->rsp);
    }

    asm volatile("sti");
}

void thread_sleep(uint32_t ms)
{
    uint32_t ticks = ms / 10;
    if (ticks == 0) ticks = 1;

    asm volatile("cli");
    current_thread->sleep_ticks = ticks;
    current_thread->state = THREAD_SLEEPING;
    asm volatile("sti");

    yield();
}

thread_t* thread_create(void (*entry_point)(), const char* name) 
{
    thread_t* t = (thread_t*)kmalloc(sizeof(thread_t));
    uint64_t* stack = (uint64_t*)kmalloc(16384);
    
    if (!stack || !t) return nullptr;
    
    memset(t, 0, sizeof(thread_t));
    t->stack_start = stack;
    t->state = THREAD_READY;
    t->sleep_ticks = 0;
    t->exit_code = 0;

    if (name) strncpy(t->name, name, 15);
    else strcpy(t->name, "unk");
    
    uint64_t* stack_ptr = (uint64_t*)((uintptr_t)stack + 16384);

    *(--stack_ptr) = (uint64_t)thread_exit; 
    *(--stack_ptr) = (uint64_t)entry_point;

    *(--stack_ptr) = 0x202; // RFLAGS
    *(--stack_ptr) = 0;     // R15
    *(--stack_ptr) = 0;     // R14
    *(--stack_ptr) = 0;     // R13
    *(--stack_ptr) = 0;     // R12
    *(--stack_ptr) = 0;     // RBX
    *(--stack_ptr) = 0;     // RBP
    
    t->rsp = stack_ptr;
    return t;
}

void idle_task() 
{
    while (1) 
    {
        asm volatile("cli");
        cleanup_zombies();
        asm volatile("sti");
        asm volatile("hlt");
    }
}

thread_t* thread_get_current() { return current_thread; }
thread_t* get_idle_thread_ptr() { return idle_thread_ptr; }

uint32_t thread_get_id_by_name(const char* name) 
{
    if (!current_thread || !name) return THREAD_NOT_FOUND;
    thread_t* temp = current_thread;
    uint32_t found_id = THREAD_NOT_FOUND;
    int count = 0;

    do 
	{
        if (strcmp(temp->name, name) == 0) 
		{
            found_id = temp->id;
            count++;
        }
        temp = temp->next;
    } while (temp != current_thread);

    if (count > 1) return THREAD_AMBIGUOUS;
    return found_id;
}

bool thread_kill(uint32_t id) 
{
    if (id == current_thread->id || id == idle_thread_ptr->id || id == 0) return false; 

    uint64_t rflags;
    asm volatile("pushfq; popq %0; cli" : "=rm"(rflags));

    thread_t* prev = current_thread;
    thread_t* curr = current_thread->next;
    bool found = false;

    do 
	{
        if (curr->id == id) 
		{
            prev->next = curr->next;
            if (curr->stack_start) kfree(curr->stack_start);
            kfree(curr);
            found = true;
            break;
        }
        prev = curr;
        curr = curr->next;
    } while (curr != current_thread);

    asm volatile("pushq %0; popfq" : : "rm"(rflags));
    return found;
}

void thread_print_list() 
{
    if (!current_thread) return;
    printf("  ID    %-15s %-10s %s\n", "NAME", "STATE", "RSP");
    printf("------------------------------------------------------------\n");

    thread_t* t = current_thread;
    do 
	{
        const char* state_str;
        switch (t->state) 
		{
            case THREAD_READY:    state_str = "READY"; break;
            case THREAD_RUNNING:  state_str = "RUNN "; break;
            case THREAD_SLEEPING: state_str = "SLEEP"; break;
            case THREAD_BLOCKED:  state_str = "BLOCK"; break;
            default:              state_str = "UNKN "; break;
        }
        printf("  %d    %-15s %-10s 0x%lx\n", (int)t->id, t->name, state_str, (uint64_t)t->rsp);
        t = t->next;
    } while (t != current_thread);
}

void thread_exit(int code)
{
    asm volatile("cli");
    thread_t* self = current_thread;

    self->exit_code = code;
    self->state = THREAD_ZOMBIE;

    if (self->id == 0 || self == idle_thread_ptr) 
        panic(KernelError::K_ERR_SYSTEM_THREAD_EXIT_ATTEMPT);
    

    yield();
    __builtin_unreachable();
}

void thread_wakeup_blocked()
{
	if (!current_thread) return;

	thread_t* temp = current_thread;
	do
	{
		if (temp->state == THREAD_BLOCKED)
			temp->state = THREAD_READY;
		temp = temp->next;
	} while (temp != current_thread);
}