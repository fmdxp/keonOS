#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <kernel/constants.h>

struct page_table_entry 
{
    uint32_t present    : 1;   
    uint32_t rw         : 1;   
    uint32_t user       : 1;   
    uint32_t pwt        : 1;   
    uint32_t pcd        : 1;   
    uint32_t accessed   : 1;   
    uint32_t dirty      : 1;   
    uint32_t pat        : 1;   
    uint32_t global     : 1;   
    uint32_t unused     : 3;   
    uint32_t frame      : 20;  
} __attribute__((packed));


struct page_directory_entry 
{
    uint32_t present    : 1;   
    uint32_t rw         : 1;   
    uint32_t user       : 1;   
    uint32_t pwt        : 1;   
    uint32_t pcd        : 1;   
    uint32_t accessed   : 1;   
    uint32_t reserved   : 1;   
    uint32_t page_size  : 1;   
    uint32_t global     : 1;   
    uint32_t unused     : 3;   
    uint32_t frame      : 20;  
} __attribute__((packed));


struct paging_stats 
{
    uint32_t total_frames;
    uint32_t used_frames;
    uint32_t free_frames;
    uint32_t mapped_pages;
};


void pfa_init(uint32_t total_ram_bytes);
void* pfa_alloc_frame();
void pfa_free_frame(void* frame);
uint32_t pfa_get_free_count();
void pfa_mark_used(uint32_t frame_start, uint32_t frame_count);


void paging_init();
void* paging_create_page_directory();
void paging_map_page(void* virtual_addr, void* physical_addr, uint32_t flags, bool mark_as_used);
void paging_unmap_page(void* virtual_addr);
void* paging_get_physical_address(void* virtual_addr);
void paging_identity_map(void* addr, uint32_t size, uint32_t flags, bool mark_as_used);


#ifdef __cplusplus
extern "C" 
{
#endif
void paging_load_directory(void* page_directory);
void paging_enable();
void paging_disable();
#ifdef __cplusplus
}
#endif


void* paging_get_current_directory();
void paging_get_stats(struct paging_stats* stats);
void* paging_get_first_free_virtual_address();


#endif		// PAGING_H