#include <drivers/multiboot.h>
#include <kernel/constants.h>
#include <kernel/error.h>
#include <kernel/panic.h>
#include <proc/thread.h>
#include <mm/paging.h>
#include <mm/heap.h>
#include <string.h>
#include <stdio.h>

#define VIRT_OFFSET 0xC0000000

extern multiboot_info_t* multiboot_info_ptr;
static spinlock_t pfa_lock = {0, 0};
static spinlock_t paging_lock = {0, 0};

static uint32_t* frame_bitmap = nullptr;
static uint32_t total_frames = 0;
static uint32_t used_frames = 0;
static uint32_t mapped_pages = 0;
static volatile uint32_t frame_bitmap_size;

static struct page_directory_entry* current_page_directory = nullptr;

extern "C" void paging_load_directory(void*);
extern "C" void paging_enable();

extern "C" uint32_t _kernel_physical_start;
extern "C" uint32_t _kernel_end;


extern uint32_t _stack_low_bottom;
extern uint32_t _stack_low_top;


static inline uint32_t addr_to_frame(void* addr) { return ((uintptr_t)addr) / PAGE_SIZE; }
static inline void* frame_to_addr(uint32_t frame) { return (void*)(frame * PAGE_SIZE); }

static void set_frame_bit(uint32_t frame)
{ 
    if (frame >= total_frames) return;
    frame_bitmap[frame/32] |= (1 << (frame%32)); 
}

static void clear_frame_bit(uint32_t frame) { frame_bitmap[frame/32] &= ~(1 << (frame%32)); }
static bool is_frame_used(uint32_t frame) { return (frame_bitmap[frame/32] & (1 << (frame%32))) != 0; }



void pfa_init_from_multiboot() 
{   
    if (!(multiboot_info_ptr->flags & (1 << 6)))
        panic(KernelError::K_ERR_GENERAL_PROTECTION, "No multiboot memory map!");
    

    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)multiboot_info_ptr->mmap_addr;
    uint32_t mmap_end = multiboot_info_ptr->mmap_addr + multiboot_info_ptr->mmap_length;

    extern uint32_t _kernel_physical_end;
    uintptr_t phys_end = (uintptr_t)&_kernel_physical_end;

    uintptr_t max_used_phys = (phys_end + PAGE_SIZE - 1) & ~0xFFF;
   

    if (multiboot_info_ptr->flags & (1<<3)) 
    {
        multiboot_module_t* mods = (multiboot_module_t*)multiboot_info_ptr->mods_addr;
        for(uint32_t i = 0; i<multiboot_info_ptr->mods_count; i++)
        {
            uintptr_t mod_end = (mods[i].mod_end + PAGE_SIZE-1) & ~0xFFF;
            if (mod_end > max_used_phys) max_used_phys = mod_end;
        }
    }


    total_frames = 0;

    for(uintptr_t addr = (uintptr_t)mmap; addr < mmap_end; )
    {
        multiboot_memory_map_t* entry = (multiboot_memory_map_t*)addr;
        uintptr_t end = (uintptr_t)(entry->addr + entry->len);

        if(end / PAGE_SIZE > total_frames)
            total_frames = end / PAGE_SIZE;
    
        addr += entry->size + sizeof(entry->size);
    }

    frame_bitmap_size = (total_frames + 31) / 32;
    frame_bitmap = (uint32_t*)(max_used_phys + VIRT_OFFSET);

    memset(frame_bitmap, 0, frame_bitmap_size * 4);
    used_frames = 0;

    pfa_mark_used(0, 1024);
    

    if(multiboot_info_ptr->flags & (1<<3))
    {
        multiboot_module_t* mods = (multiboot_module_t*)multiboot_info_ptr->mods_addr;
        for(uint32_t i=0;i<multiboot_info_ptr->mods_count;i++)
        {
            uint32_t start_frame = addr_to_frame((void*)mods[i].mod_start);
            uint32_t frame_count = (mods[i].mod_end - mods[i].mod_start + PAGE_SIZE-1)/PAGE_SIZE;
            pfa_mark_used(start_frame, frame_count);
        }
    }

    pfa_mark_used(addr_to_frame(multiboot_info_ptr), (multiboot_info_ptr->mmap_length + PAGE_SIZE-1)/PAGE_SIZE);
    pfa_mark_used(addr_to_frame((void*)multiboot_info_ptr->mods_addr), (multiboot_info_ptr->mods_count * sizeof(multiboot_module_t) + PAGE_SIZE-1)/PAGE_SIZE);

    for(uintptr_t addr = (uintptr_t)mmap; addr < mmap_end;)
    {
        multiboot_memory_map_t* entry = (multiboot_memory_map_t*)addr;
        if(entry->type != 1)
        {
            uint32_t start_frame = addr_to_frame((void*)entry->addr);
            uint32_t frame_count = (entry->len + PAGE_SIZE-1)/PAGE_SIZE;
            pfa_mark_used(start_frame, frame_count);
        }
        addr += entry->size + sizeof(entry->size);
    }
}


void pfa_mark_used(uint32_t frame_start, uint32_t frame_count)
{
    for(uint32_t i=0;i<frame_count;i++)
    {
        if(!is_frame_used(frame_start+i)) { set_frame_bit(frame_start+i); used_frames++; }
    }
    
}


void* pfa_alloc_frame()
{
    spin_lock_irqsave(&pfa_lock);
    for(uint32_t i=0;i<frame_bitmap_size;i++)
    {
        if(frame_bitmap[i] != 0xFFFFFFFF)
        {
            uint32_t bit = __builtin_ctz(~frame_bitmap[i]);
            uint32_t frame = i*32 + bit;
            set_frame_bit(frame);
            used_frames++;
            spin_unlock_irqrestore(&pfa_lock);
            return frame_to_addr(frame);
        }
    }
    spin_unlock_irqrestore(&pfa_lock);
    return nullptr;
}


void pfa_free_frame(void* frame)
{
    if(!frame) return;
    spin_lock_irqsave(&pfa_lock);
    uint32_t f = addr_to_frame(frame);
    if(is_frame_used(f)) { clear_frame_bit(f); used_frames--; }
    spin_unlock_irqrestore(&pfa_lock);
}


uint32_t pfa_get_free_count() { return total_frames - used_frames; }



static struct page_table_entry* get_page_table_entry(void* virtual_addr, bool create)
{
    uint32_t addr = (uint32_t)virtual_addr;
    uint32_t pd_idx = (addr >> 22) & 0x3FF;  
    uint32_t pt_idx = (addr >> 12) & 0x3FF;

    if (!current_page_directory[pd_idx].present)
    {
        if(!create) return nullptr;
        void* frame_phys = pfa_alloc_frame();
        
        void* frame_virt = (void*)((uintptr_t)frame_phys + VIRT_OFFSET);
        memset(frame_virt, 0, PAGE_SIZE);

        current_page_directory[pd_idx].frame = addr_to_frame(frame_phys);
        current_page_directory[pd_idx].present = 1;
        current_page_directory[pd_idx].rw = 1;
    }

    uintptr_t pt_phys = current_page_directory[pd_idx].frame * PAGE_SIZE;
    struct page_table_entry* pt = (struct page_table_entry*)(pt_phys + VIRT_OFFSET);
    return &pt[pt_idx];
}



void paging_map_page(void* virtual_addr, void* physical_addr, uint32_t flags, bool mark_as_used)
{
    spin_lock_irqsave(&paging_lock);
    struct page_table_entry* pte = get_page_table_entry(virtual_addr,true);
    if(!pte) { spin_unlock_irqrestore(&paging_lock); return; }

    if(pte->present && !mark_as_used) 
        pfa_free_frame(frame_to_addr(pte->frame)); 

    pte->present = (flags&PTE_PRESENT)?1:0;
    pte->rw = (flags&PTE_RW)?1:0;
    pte->user = (flags&PTE_USER)?1:0;
    pte->frame = addr_to_frame(physical_addr);

    if(mark_as_used && !is_frame_used(pte->frame)) { set_frame_bit(pte->frame); used_frames++; }

    mapped_pages++;
    asm volatile("invlpg (%0)"::"r"(virtual_addr):"memory");
    spin_unlock_irqrestore(&paging_lock);
}


void paging_unmap_page(void* virtual_addr)
{
    spin_lock_irqsave(&paging_lock);
    struct page_table_entry* pte = get_page_table_entry(virtual_addr,false);
    if(pte && pte->present)
    {
        void* physical_frame = frame_to_addr(pte->frame);
        pfa_free_frame(physical_frame);

        pte->present = 0;
        pte->frame = 0;
        mapped_pages--;
        asm volatile("invlpg (%0)"::"r"(virtual_addr):"memory");
    }
    spin_unlock_irqrestore(&paging_lock);
}


void* paging_get_physical_address(void* virtual_addr)
{
    struct page_table_entry* pte = get_page_table_entry(virtual_addr,false);
    if(!pte || !pte->present) return nullptr;
    uint32_t offset = ((uintptr_t)virtual_addr)&0xFFF;
    return (void*)((pte->frame*PAGE_SIZE)+offset);
}


void* paging_create_page_directory()
{
    void* pd_frame = pfa_alloc_frame();
    if(!pd_frame) return nullptr;
    struct page_directory_entry* pd = (struct page_directory_entry*)pd_frame;
    memset(pd,0,PAGE_SIZE);
    return pd;
}


void paging_identity_map(uintptr_t start, uintptr_t size, uint32_t flags, bool mark_as_used)
{
    start &= ~0xFFF;
    uintptr_t end = (start+size+0xFFF)&~0xFFF;
    for(uintptr_t addr = start; addr < end; addr+=PAGE_SIZE)
        paging_map_page((void*)addr,(void*)addr,flags,mark_as_used);
}


void paging_init()
{
    uintptr_t cr3_value;
    asm volatile("mov %%cr3, %0" : "=r"(cr3_value));
    current_page_directory = (struct page_directory_entry*)(cr3_value + VIRT_OFFSET);
    
    pfa_init_from_multiboot();
    
    current_page_directory[1023].frame = cr3_value >> 12;
    current_page_directory[1023].present = 1;
    current_page_directory[1023].rw = 1;

    asm volatile("mov %0, %%cr3" : : "r"(cr3_value));

    printf("PAGING IS SYNCHRONIZED AND UPDATED\n");
}

void* paging_get_current_directory() { return current_page_directory; }


void paging_get_stats(struct paging_stats* stats)
{
    if(!stats) return;
    stats->total_frames = total_frames;
    stats->used_frames = used_frames;
    stats->free_frames = pfa_get_free_count();
    stats->mapped_pages = mapped_pages;
}

void* paging_get_first_free_virtual_address()
{
    uintptr_t bitmap_end = (uintptr_t)frame_bitmap + frame_bitmap_size*4;
    return (void*)((bitmap_end + 0xFFF)&~0xFFF);
}