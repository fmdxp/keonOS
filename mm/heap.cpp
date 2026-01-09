#include <kernel/constants.h>
#include <proc/thread.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <string.h>

static spinlock_t heap_lock = {0, 0};
static struct heap_block* heap_start = NULL;
static size_t heap_total_size = 0;

bool kheap_init(void* start_addr, size_t size) 
{
    if (start_addr == NULL || size < sizeof(struct heap_block) * 2)
        return false;
    
    heap_start = reinterpret_cast<struct heap_block*>(start_addr);
    heap_start->size = size - sizeof(struct heap_block);
    heap_start->free = true;
    heap_start->next = NULL;
    heap_total_size = size;    
    return true;
}


void* kmalloc(size_t size) 
{   
    if (!heap_start) return NULL;

    size = (size + 3) & ~3;
    spin_lock_irqsave(&heap_lock);

    struct heap_block* current = heap_start;
    struct heap_block* last = NULL;
    
    while (current) 
	{
        if (current->free && current->size >= size) 
		{   
            if (current->size > size + sizeof(struct heap_block) + 4) 
			{   
                struct heap_block* new_block = (struct heap_block*)((uint8_t*)current + sizeof(struct heap_block) + size);
                new_block->size = current->size - size - sizeof(struct heap_block);
                new_block->free = true;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            
            current->free = false;
            spin_unlock_irqrestore(&heap_lock);
            return (void*)((uint8_t*)current + sizeof(struct heap_block));
        }
        last = current;
        current = current->next;
    }
    
    size_t total_needed = size + sizeof(struct heap_block);
    struct heap_block* new_region = (struct heap_block*)VMM::sbrk(total_needed);

    if (new_region == (void*)-1 || new_region == NULL)
    {
        spin_unlock_irqrestore(&heap_lock);
        return NULL;
    }

    size_t actual_size = ((total_needed + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

    new_region->size = actual_size - sizeof(struct heap_block);
    new_region->free = false;
    new_region->next = NULL;

    if (last) last->next = new_region;
    else heap_start = new_region;

    spin_unlock_irqrestore(&heap_lock);
    return (void*)((uint8_t*)new_region + sizeof(struct heap_block));
}

void kfree(void* ptr) 
{
    if (!ptr) return;
    spin_lock_irqsave(&heap_lock);

    struct heap_block* block = (struct heap_block*)((uint8_t*)ptr - sizeof(struct heap_block));
    block->free = true;
    
    
    if (block->next && block->next->free) 
	{
        block->size += sizeof(struct heap_block) + block->next->size;
        block->next = block->next->next;
    }
    
    struct heap_block* prev = heap_start;
    while (prev && prev->next != block)
    {
        if (prev->next == block)
        {
            if (prev->free)
            {
                prev->size += sizeof(struct heap_block) + block->size;
                prev->next = block->next;
            }
            break;
        }
        prev = prev->next;
    }

    spin_unlock_irqrestore(&heap_lock);
}


void kheap_get_stats(struct heap_stats* stats) 
{
    if (!stats) return;
    
    stats->total_size = heap_total_size;
    stats->used_size = 0;
    stats->free_size = 0;
    stats->block_count = 0;
    stats->free_block_count = 0;
    
    struct heap_block* current = heap_start;
    while (current != NULL) 
	{
        stats->block_count++;
        if (current->free) 
		{
            stats->free_size += current->size;
            stats->free_block_count++;
        }
		else stats->used_size += current->size;
        current = current->next;
    }
    
    size_t overhead = stats->block_count * sizeof(struct heap_block);
    stats->used_size += overhead;
    if (stats->used_size > stats->total_size) stats->used_size = stats->total_size;
    stats->free_size = stats->total_size - stats->used_size;
}

heap_block* get_kheap_start()
{
    return heap_start;
}

void* operator new(size_t size) {
    return kmalloc(size);
}

void* operator new[](size_t size) {
    return kmalloc(size);
}

void operator delete(void* ptr) noexcept {
    kfree(ptr);
}

void operator delete[](void* ptr) noexcept {
    kfree(ptr);
}

void operator delete(void* ptr, size_t size) noexcept 
{
    (void)size;
    kfree(ptr);
}