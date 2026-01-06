#include <drivers/vga.h>
#include <proc/thread.h>
#include <mm/heap.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static thread_t* current_thread = nullptr;
static thread_t* idle_thread_ptr = nullptr;
static uint32_t next_thread_id = 0;

void thread_init()
{
	current_thread = (thread_t*)kmalloc(sizeof(thread_t));
	current_thread->id = next_thread_id++;
	current_thread->state = THREAD_READY;
    current_thread->next = current_thread;
	current_thread->stack_start = nullptr; 

	strcpy(current_thread->name, "kernel");
	idle_thread_ptr = thread_create(idle_task, "sys_idle");
	if (idle_thread_ptr)
	{
		idle_thread_ptr->id = next_thread_id++;
		idle_thread_ptr->next = current_thread->next;
		current_thread->next = idle_thread_ptr;
	}
}

void spin_lock_irqsave(spinlock_t* lock)
{
	uint32_t eflags;

	asm volatile("pushfl; popl %0; cli" : "=rm"(eflags));

	while (__sync_lock_test_and_set(&lock->locked, 1))
		asm volatile("pause");
	
	lock->eflags = eflags;
}

void spin_unlock_irqrestore(spinlock_t* lock) 
{
	uint32_t eflags = lock->eflags;
    __sync_lock_release(&lock->locked);
    asm volatile("pushl %0; popfl" : : "rm"(eflags));
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
	uint32_t eflags;
	asm volatile("pushfl; popl %0; cli" : "=rm"(eflags));
	
	thread_t* t = thread_create(entry_point, name);
	if (t)
	{
		t->id = next_thread_id++;
		t->next = current_thread->next;
		current_thread->next = t;
	}
	
	asm volatile("pushl %0; popfl" : : "rm"(eflags));
    return t;
}

extern "C" void yield()
{
	if (!current_thread) return;

	asm volatile("cli");
	
	thread_t* search = current_thread->next;
	thread_t* found  = nullptr;
	
	for (int i = 0; i < 256; i++)
	{
		if (search->state == THREAD_SLEEPING && search->sleep_ticks == 0) 
			search->state = THREAD_READY;

		if (search->state == THREAD_READY && search != idle_thread_ptr)
		{
			found = search; 
			break;
		}
		search = search->next;
	}
	
	if (!found) 
	{
        if (current_thread->state == THREAD_READY && current_thread != idle_thread_ptr) 
		{
            asm volatile("sti");
            return;
        }
        found = idle_thread_ptr;
    }

	// Visual debug for current thread
    // uint16_t* vga = (uint16_t*)0xB8000;
    // if (found == idle_thread_ptr) vga[79] = 0x0E00 | 'I';
    // else vga[79] = 0x0A00 | 'S';

    if (found != current_thread) 
	{
        thread_t* old = current_thread;
		current_thread = found;
		switch_context(&(old->esp), &(current_thread->esp));
    }
    
    asm volatile("sti");
}

void thread_sleep(uint32_t ms)
{
	uint32_t ticks_to_sleep = ms / 10;
	if (ticks_to_sleep == 0) ticks_to_sleep = 1;

	asm volatile("cli");
	current_thread->sleep_ticks = ticks_to_sleep;
	current_thread->state = THREAD_SLEEPING;
	asm volatile("sti");

	yield();
}

thread_t* thread_create(void (*entry_point)(), const char* name) 
{
	thread_t* t = reinterpret_cast<thread_t*>(kmalloc(sizeof(thread_t)));
    uint32_t* stack = reinterpret_cast<uint32_t*>(kmalloc(8192));
	
	if (!stack || !t) return nullptr;
	
	t->stack_start = stack;
	t->state = THREAD_READY;
	t->sleep_ticks = 0;

	if (name)
	{
		int i;
		for (i = 0; i < 15 && name[i] != '\0'; i++)
			t->name[i] = name[i];
		t->name[i] = '\0';
	}

	else
	{
		t->name[0] = 'u'; t->name[1] = 'n'; t->name[2] = 'k'; t->name[3] = '\0';
		// UNK + '\0'
	}
	
    uint32_t* stack_ptr = (uint32_t*)((uintptr_t)(stack) + 8192);

	*(--stack_ptr) = (uint32_t)thread_exit;
	*(--stack_ptr) = (uint32_t)entry_point;
	
    *(--stack_ptr) = (uint32_t)(entry_point);
    *(--stack_ptr) = 0; 	// EBP
    *(--stack_ptr) = 0; 	// EBX
    *(--stack_ptr) = 0; 	// ESI
    *(--stack_ptr) = 0; 	// EDI
	*(--stack_ptr) = 0x202; // EFLAGS
	
    t->esp = stack_ptr;
    return t;
}

void idle_task() 
{
	while (1) asm volatile("hlt");
}

thread_t* thread_get_current() 
{
	return current_thread;
}

thread_t* get_idle_thread_ptr() 
{
	return idle_thread_ptr;
}

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
    if (id == current_thread->id || id == idle_thread_ptr->id || id == 0) 
        return false; 
    

    uint32_t eflags;
    asm volatile("pushfl; popl %0; cli" : "=rm"(eflags));

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

    asm volatile("pushl %0; popfl" : : "rm"(eflags));
    return found;
}

void thread_print_list() 
{
    if (!current_thread) return;

    int count = 0;
    thread_t* temp = current_thread;
	do
	{
        count++;
        temp = temp->next;
    } while (temp != current_thread);

    if (count > 32) count = 32; 

	thread_t* sorted[32];
    temp = current_thread;
    for (int i = 0; i < count; i++) 
	{
        sorted[i] = temp;
        temp = temp->next;
    }

    for (int i = 0; i < count - 1; i++) 
	{
        for (int j = 0; j < count - i - 1; j++) 
		{
            if (sorted[j]->id > sorted[j+1]->id) 
			{
                thread_t* swap = sorted[j];
                sorted[j] = sorted[j+1];
                sorted[j+1] = swap;
            }
        }
    }

    printf("  ID    %-15s %-10s %s\n", "NAME", "STATE", "ESP");
    printf("------------------------------------------------------------\n");

    for (int i = 0; i < count; i++) 
	{
        thread_t* t = sorted[i];

        const char* state_str;
        switch (t->state) 
		{
            case THREAD_READY:    state_str = "READY"; break;
            case THREAD_RUNNING:  state_str = "RUNN "; break;
            case THREAD_SLEEPING: state_str = "SLEEP"; break;
            case THREAD_BLOCKED:  state_str = "BLOCK"; break;
            default:              state_str = "UNKN "; break;
        }

        printf("  %d    %-15s %-10s 0x%x\n", 
            (int)t->id, t->name, state_str, (uint32_t)t->esp);
    }
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

void thread_exit() 
{
    thread_t* self = thread_get_current();
    
    if (self->id == 0 || self == get_idle_thread_ptr())
        abort();
    
    thread_kill(self->id);
    yield();
	__builtin_unreachable();
}