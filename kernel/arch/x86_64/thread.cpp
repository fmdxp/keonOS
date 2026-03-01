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
#include <kernel/arch/x86_64/gdt.h>
#include <kernel/arch/x86_64/paging.h>
#include <kernel/constants.h>
#include <kernel/panic.h>
#include <kernel/error.h>
#include <kernel/syscalls/syscalls.h>
#include <mm/heap.h>
#include <sys/errno.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static thread_t* current_thread = nullptr;
static thread_t* idle_thread_ptr = nullptr;
static uint32_t next_thread_id = 0;

spinlock_t thread_list_lock = {0, 0};
spinlock_t zombie_lock = {0, 0};
thread_t* zombie_list_head = nullptr;

extern "C" void switch_context(uint64_t** old_rsp, uint64_t* new_rsp);
extern "C" void user_thread_entry();
extern "C" tss_entry kernel_tss;

 
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

void cleanup_zombies() 
{
    spin_lock_irqsave((spinlock_t*)&zombie_lock);
    thread_t* curr = zombie_list_head;
    zombie_list_head = nullptr;
    spin_unlock_irqrestore((spinlock_t*)&zombie_lock);

    while (curr) 
    {
        thread_t* next = curr->next;
        if (curr->stack_start) {
            kfree(curr->stack_start);
        }
        
        if (curr->is_user) 
        {
             
             // 1. Free User Stack (Fixed 16KB at 0x0000700000000000)
             // Note: t->user_stack points to TOP. Base is top - 16384.
             // But we know the fixed virtual address is 0x0000700000000000
            uintptr_t stack_base = 0x0000700000000000;
            for (int i = 0; i < 4; i++) 
            {
                uintptr_t addr = stack_base + i * 4096;
                void* phys = paging_get_physical_address((void*)addr);
                if (phys) 
                {
                    pfa_free_frame(phys);
                    paging_unmap_page((void*)addr);
                }
            }

             // 2. Free User Code/Data (Image)
             if (curr->user_image_end > curr->user_image_start) 
             {
                 for (uintptr_t addr = curr->user_image_start; addr < curr->user_image_end; addr += 4096)
                 {
                     void* phys = paging_get_physical_address((void*)addr);
                     if (phys) 
                     {
                         pfa_free_frame(phys);
                         paging_unmap_page((void*)addr);
                     }
                 }
             }

             // 3. Free User Heap (Standard start 1GB)
             // This covers both sbrk growth and huge ELFs overlaps (safe due to checks)
             uintptr_t heap_start = 0x40000000;
             if (curr->user_heap_break > heap_start) 
             {
                 for (uintptr_t addr = heap_start; addr < curr->user_heap_break; addr += 4096)
                 {
                     void* phys = paging_get_physical_address((void*)addr);
                     if (phys) 
                     {
                         pfa_free_frame(phys);
                         paging_unmap_page((void*)addr);
                     }
                 }
             }
         }

        kfree(curr);
        
        curr = next;
    }
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

thread_t* thread_add(void(*entry_point)(), const char* name, bool is_user)
{
    spin_lock_irqsave((spinlock_t*)&thread_list_lock);

    thread_t* t = is_user ? thread_create_user(entry_point, name) : thread_create(entry_point, name);
    if (t)
    {
        t->id = next_thread_id++;
        t->next = current_thread->next;
        current_thread->next = t;
    }
    spin_unlock_irqrestore((spinlock_t*)&thread_list_lock);
    
    return t;
}

extern "C" void yield()
{
    
    if (!current_thread || !idle_thread_ptr) return;

    asm volatile("cli");
    
    thread_t* prev = current_thread;
    thread_t* next_to_run = nullptr;
    
    thread_t* start_node = (prev->state == THREAD_ZOMBIE) ? idle_thread_ptr : prev;
    thread_t* scan = start_node->next;

    do 
    {
        if (scan->state == THREAD_SLEEPING && scan->sleep_ticks == 0)
            scan->state = THREAD_READY;
        scan = scan->next;
    } while (scan != start_node->next);
    
    scan = start_node->next;

    
    scan = start_node->next;

    do 
    {
        if (scan->state == THREAD_READY && scan != idle_thread_ptr) 
        {
            next_to_run = scan;
            break;
        }
        scan = scan->next;
    } while (scan != start_node->next);

    if (!next_to_run) 
    {
        if (prev->state == THREAD_READY) next_to_run = prev;
        else next_to_run = idle_thread_ptr;
    }
    
    if (next_to_run != prev) 
    {
        current_thread = next_to_run;

        // If it's a user thread, we must update RSP0 in TSS so that
        // interrupts in Ring 3 can correctly return to the kernel stack.
        // We also update the GS base used by 'syscall' instruction.
        uint64_t kstack = (uint64_t)next_to_run->stack_start + 16384;
        
        if (next_to_run->is_user)
        {
            kernel_tss.rsp0 = kstack;
        }
            
        syscall_set_kernel_stack(kstack);
        
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
    t->is_user = false;
    t->state = THREAD_READY;
    t->sleep_ticks = 0;
    t->exit_code = 0;
    t->user_heap_break = 0;

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

thread_t* thread_create_user(void (*entry_point)(), const char* name) 
{
    thread_t* t = (thread_t*)kmalloc(sizeof(thread_t));
    uint64_t* k_stack = (uint64_t*)kmalloc(16384); // Stack Kernel (Ring 0)
    
    // Increase user stack to 16KB (4 pages)
    uintptr_t u_stack_virt = 0x0000700000000000;
    
    for (int i = 0; i < 4; i++) 
    {
        void* u_stack_phys = pfa_alloc_frame();
        if (!u_stack_phys) 
        {
             kfree(k_stack);
             kfree(t);
             return nullptr;
        }
        paging_map_page((void*)(u_stack_virt + i * 4096), u_stack_phys, PTE_PRESENT | PTE_RW | PTE_USER);
    }
    uintptr_t u_stack_top = u_stack_virt + 16384;

    memset(t, 0, sizeof(thread_t));
    t->is_user = true;
    t->state = THREAD_READY;
    t->stack_start = k_stack;
    t->user_stack = (uint64_t*)u_stack_top;
    t->user_heap_break = 0x600000;
    strncpy(t->name, name, 15);
    t->user_image_start = 0;
    t->user_image_end = 0;

    uint64_t* sp = (uint64_t*)((uintptr_t)k_stack + 16384);

    *(--sp) = 0x1B;                     // SS (User Data + RPL 3)
    *(--sp) = u_stack_top;              // RSP Utente
    *(--sp) = 0x202;                    // RFLAGS (Interrupt abilitati in Ring 3)
    *(--sp) = 0x23;                     // CS (User Code + RPL 3)
    *(--sp) = (uintptr_t)entry_point;   // RIP

    *(--sp) = (uint64_t)user_thread_entry;

    *(--sp) = 0x202; // RFLAGS (Quello che verrà estratto da popfq)
    *(--sp) = 0;     // R15
    *(--sp) = 0;     // R14
    *(--sp) = 0;     // R13
    *(--sp) = 0;     // R12
    *(--sp) = 0;     // RBX
    *(--sp) = 0;     // RBP


    t->rsp = sp;
    return t;
}

void user_test_thread() 
{
    uint16_t cs;
    asm volatile("mov %%cs, %0" : "=r"(cs));
    
    if ((cs & 0x3) == 3) {} else {}

    while(1) asm volatile("nop");
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

    spin_lock_irqsave((spinlock_t*)&thread_list_lock);

    thread_t* prev = current_thread;
    thread_t* curr = current_thread->next;
    bool found = false;

    do 
	{
        if (curr->id == id) 
		{
            prev->next = curr->next;

            curr->state = THREAD_ZOMBIE;
            curr->exit_code = -1;

            spin_lock(&zombie_lock);
            curr->next = zombie_list_head;
            zombie_list_head = curr;
            spin_unlock(&zombie_lock);

            found = true;
            break;
        }
        prev = curr;
        curr = curr->next;
    } while (curr != current_thread);

    spin_unlock_irqrestore((spinlock_t*)&thread_list_lock);
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
            case THREAD_ZOMBIE:   state_str = "ZOMB "; break; 
            default:              state_str = "UNKN "; break;
        }
        printf("  %d    %-15s %-10s 0x%lx\n", (int)t->id, t->name, state_str, (uint64_t)t->rsp);
        t = t->next;
    } while (t != current_thread);
}

void thread_exit(int code)
{
    spin_lock_irqsave((spinlock_t*)&thread_list_lock);

    thread_t* self = current_thread;
    self->exit_code = code;
    self->state = THREAD_ZOMBIE;

    if (self->id == 0 || self == idle_thread_ptr) 
        panic(KernelError::K_ERR_SYSTEM_THREAD_EXIT_ATTEMPT);

    thread_t* prev = self;
    while (prev->next != self)
        prev = prev->next;
    
    prev->next = self->next;
    
    spin_lock(&zombie_lock);
    self->next = zombie_list_head;
    zombie_list_head = self;
    spin_unlock(&zombie_lock);

    spin_unlock_irqrestore((spinlock_t*)&thread_list_lock);

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

int64_t thread_kill_by_string(const char* input) 
{
    if (!input || input[0] == '\0') return -EINVAL;

    uint32_t id;

    if (input[0] >= '0' && input[0] <= '9') id = (uint32_t)atoi(input);
    else 
    {
        id = thread_get_id_by_name(input);
        if (id == THREAD_NOT_FOUND) return -ESRCH;
        if (id == THREAD_AMBIGUOUS) return -E2BIG;
    }

    if (thread_kill(id)) return 0;
    return -EPERM;
}

thread_t* thread_get_by_id(uint32_t id)
{
    if (!current_thread) return nullptr;
    
    thread_t* temp = current_thread;
    do {
        if (temp->id == id) return temp;
        temp = temp->next;
    } while (temp != current_thread);
    
    return nullptr;
}