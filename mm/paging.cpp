#include <kernel/constants.h>
#include <drivers/multiboot.h>
#include <proc/thread.h>
#include <mm/paging.h>
#include <mm/heap.h>
#include <string.h>
#include <stdio.h>

extern multiboot_info_t* multiboot_info_ptr;

static spinlock_t pfa_lock = {0, 0};
static spinlock_t paging_lock = {0, 0};
static uint8_t* frame_bitmap = nullptr;
static uint32_t total_frames = 0;
static uint32_t used_frames = 0;
static uint32_t mapped_pages = 0;
static volatile uint32_t frame_bitmap_size;

static struct page_directory_entry* current_page_directory = NULL;

extern "C" void paging_load_directory(void*);
extern "C" void paging_enable();
    

static uint32_t addr_to_frame(void* addr) 
{
    return (reinterpret_cast<uint32_t>(addr)) / PAGE_SIZE;
}


static void* frame_to_addr(uint32_t frame) 
{
    return reinterpret_cast<void*>((frame * PAGE_SIZE));
}


static void set_frame_bit(uint32_t frame) 
{
    if (frame >= total_frames) return;
    frame_bitmap[frame / 8] |= (1 << (frame % 8));
}


static void clear_frame_bit(uint32_t frame) 
{
    if (frame >= total_frames) return;
    frame_bitmap[frame / 8] &= ~(1 << (frame % 8));
}


static bool is_frame_used(uint32_t frame) 
{
    if (frame >= total_frames) return true;
    return (frame_bitmap[frame / 8] & (1 << (frame % 8))) != 0;
}


void pfa_init(uint32_t total_ram_bytes) 
{   
    total_frames = total_ram_bytes / PAGE_SIZE;
    frame_bitmap_size = total_frames / 8;

    extern uint32_t _kernel_end;
    frame_bitmap = reinterpret_cast<uint8_t*>((reinterpret_cast<uint32_t>(&_kernel_end) + 4095) & ~4095);

    memset(frame_bitmap, 0, frame_bitmap_size);
    used_frames = 0;

    uint32_t frames_to_protect = (addr_to_frame(frame_bitmap) + (frame_bitmap_size / PAGE_SIZE) + 1);
    pfa_mark_used(0, frames_to_protect);
}


void pfa_mark_used(uint32_t frame_start, uint32_t frame_count) 
{
    for (uint32_t i = 0; i < frame_count && (frame_start + i) < total_frames; i++) 
	{
        if (!is_frame_used(frame_start + i)) 
		{
            set_frame_bit(frame_start + i);
            used_frames++;
        }
    }
}


void* pfa_alloc_frame() 
{
    spin_lock_irqsave(&pfa_lock);
    for (uint32_t i = 0; i < total_frames; i++) 
	{
        if (!is_frame_used(i)) 
		{
            set_frame_bit(i);
            used_frames++;
            spin_unlock_irqrestore(&pfa_lock);
            return frame_to_addr(i);
        }
    }
    spin_unlock_irqrestore(&pfa_lock);
    return NULL;
}


void pfa_free_frame(void* frame) 
{
    if (frame == NULL) return;
    spin_lock_irqsave(&pfa_lock);
    uint32_t frame_num = addr_to_frame(frame);
    if (frame_num >= total_frames) return;
    
    if (is_frame_used(frame_num)) 
	{
        clear_frame_bit(frame_num);
        used_frames--;
    }
    spin_unlock_irqrestore(&pfa_lock);
}


uint32_t pfa_get_free_count() 
{
    return total_frames - used_frames;
}


static struct page_table_entry* get_page_table_entry(void* virtual_addr, bool create) 
{
    uint32_t addr = (uint32_t)virtual_addr;
    uint32_t pd_idx = (addr >> 22) & 0x3FF;  
    uint32_t pt_idx = (addr >> 12) & 0x3FF;
    
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    bool paging_active (cr0 & 0x80000000);

    if (!paging_active)
    {
        if (!current_page_directory[pd_idx].present)
        {
            if (!create) return NULL;
            void* frame = pfa_alloc_frame();
            // printf("[DEBUG] Allocated PT frame at: %x for PD index: %d\n", (uint32_t)frame, pd_idx); // troppi print
            memset(frame, 0, PAGE_SIZE);
            current_page_directory[pd_idx].frame = addr_to_frame(frame);
            current_page_directory[pd_idx].present = 1;
            current_page_directory[pd_idx].rw = 1;
        }
        struct page_table_entry* pt = (struct page_table_entry*)frame_to_addr(current_page_directory[pd_idx].frame);
        return &pt[pt_idx];
    }

    else
    {
        struct page_directory_entry* pde = (struct page_directory_entry*)0xFFFFF000;
        if (!pde[pd_idx].present) 
        {
            if (!create) return NULL;
            void* pt_frame = pfa_alloc_frame();
            
            pde[pd_idx].frame = addr_to_frame(pt_frame);
            pde[pd_idx].present = 1;
            pde[pd_idx].rw = 1;

            void* pt_vaddr = (void*)(0xFFC00000 + (pd_idx * PAGE_SIZE));
            asm volatile("invlpg (%0)" ::"r"(pt_vaddr) : "memory");
            memset(pt_vaddr, 0, PAGE_SIZE);
        }
        struct page_table_entry* pt_base = (struct page_table_entry*)0xFFC00000;
        return &pt_base[(pd_idx * 1024) + pt_idx];
    }
}


void paging_map_page(void* virtual_addr, void* physical_addr, uint32_t flags, bool mark_as_used) 
{
    spin_lock_irqsave(&paging_lock);
    struct page_table_entry* pte = get_page_table_entry(virtual_addr, true);
    
    if (!pte) 
    { 
        spin_unlock_irqrestore(&paging_lock); 
        return; 
    }
    
    if (pte->present) 
	{    
        uint32_t old_frame = pte->frame;
        uint32_t new_frame = addr_to_frame(physical_addr);
        if (old_frame != new_frame) pfa_free_frame(frame_to_addr(old_frame));
        mapped_pages--;
    }
    
    pte->present = (flags & PTE_PRESENT) ? 1 : 0;
    pte->rw = (flags & PTE_RW) ? 1 : 0;
    pte->user = (flags & PTE_USER) ? 1 : 0;
    pte->frame = addr_to_frame(physical_addr);
    
    
    uint32_t frame = addr_to_frame(physical_addr);
    if (mark_as_used && !is_frame_used(frame)) 
	{
        set_frame_bit(frame);
        used_frames++;
    }
    mapped_pages++;

    asm volatile("invlpg (%0)" ::"r"(virtual_addr) : "memory");
    spin_unlock_irqrestore(&paging_lock);
}


void paging_unmap_page(void* virtual_addr) 
{
    spin_lock_irqsave(&paging_lock);
    struct page_table_entry* pte = get_page_table_entry(virtual_addr, false);
    if (pte && pte->present)
    {   
        void* physical_frame = frame_to_addr(pte->frame);
        pfa_free_frame(physical_frame);
        
        pte->present = 0;
        pte->frame = 0;
        mapped_pages--;
        asm volatile("invlpg (%0)" ::"r"(virtual_addr) : "memory");
    }
    spin_unlock_irqrestore(&paging_lock);
}


void* paging_get_physical_address(void* virtual_addr) 
{
    struct page_table_entry* pte = get_page_table_entry(virtual_addr, false);
    if (!pte || !pte->present) return NULL;
    
    uint32_t offset = ((uint32_t)virtual_addr) & 0xFFF;  
    return (void*)((pte->frame * PAGE_SIZE) + offset);
}


void* paging_create_page_directory() 
{    
    void* pd_frame = pfa_alloc_frame();
    if (!pd_frame) return NULL;   

    struct page_directory_entry* pd = (struct page_directory_entry*)pd_frame;
    memset(pd, 0, PAGE_SIZE);
    return pd;
}


void paging_identity_map(void* addr, uint32_t size, uint32_t flags, bool mark_as_used) 
{
    uint32_t start_addr = (uint32_t)addr;
    uint32_t end_addr = start_addr + size;    
    
    start_addr = start_addr & ~0xFFF;  
    end_addr = (end_addr + 0xFFF) & ~0xFFF;  

    printf("[PAGING] Start Identity Map: addr=%x, size=%x, mark=%d\n", start_addr, size, mark_as_used);
    
    for (uintptr_t addr = start_addr; addr < end_addr; addr += PAGE_SIZE)
        paging_map_page((void*)addr, (void*)addr, flags, mark_as_used);
    
    printf("[PAGING] Map finished successfully.\n");
}


void paging_init() 
{    
    uint32_t total_ram = (multiboot_info_ptr->mem_lower + multiboot_info_ptr->mem_upper) * 1024;
    pfa_init(total_ram);   

    pfa_mark_used(0, 256);

    current_page_directory = (struct page_directory_entry*)paging_create_page_directory();
    if (!current_page_directory) return;

    uint32_t protect_size = 0x1000000;

    paging_identity_map(0, protect_size, PTE_PRESENT | PTE_RW, true);

    if (total_ram > protect_size)
        paging_identity_map((void*)protect_size, total_ram - protect_size, PTE_PRESENT | PTE_RW, false);

    current_page_directory[1023].frame = addr_to_frame(current_page_directory);
    current_page_directory[1023].present = 1;
    current_page_directory[1023].rw = 1;
    
    uint32_t pd_addr = (uint32_t)current_page_directory;
    
    printf("DEBUG: PD Physical Address: %x\n", (uint32_t)current_page_directory);

    asm volatile(
        "mov %0, %%cr3\n\t"
        "mov %%cr0, %%ebx\n\t"
        "or $0x80000000, %%ebx\n\t"
        "mov %%ebx, %%cr0\n\t"
        "jmp 1f\n\t"
        "1:\n\t"
        :
        : "r"(pd_addr)
        : "ebx", "memory"
    );
    printf("PAGING IS ACTIVE\n\n");
}

void* paging_get_current_directory() 
{
    return current_page_directory;
}


void paging_get_stats(struct paging_stats* stats) 
{
    if (!stats) return;
    
    stats->total_frames = total_frames;
    stats->used_frames = used_frames;
    stats->free_frames = pfa_get_free_count();
    stats->mapped_pages = mapped_pages;
}

void* paging_get_first_free_virtual_address() 
{
    uintptr_t bitmap_end = reinterpret_cast<uintptr_t>(frame_bitmap) + frame_bitmap_size;    
    return reinterpret_cast<void*>((bitmap_end + 4095) & ~4095);
}