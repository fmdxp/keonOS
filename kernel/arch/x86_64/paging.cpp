/*
 * keonOS - kernel/arch/x86_64/paging.cpp
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
#include <kernel/arch/x86_64/paging.h>
#include <drivers/multiboot2.h>
#include <kernel/constants.h>
#include <kernel/panic.h>
#include <stdint.h>
#include <string.h>

static pt_entry* pml4 = nullptr;
static uint64_t total_frames = 0;
static uint64_t used_frames = 0;
static uint64_t mapped_pages = 0;

static uint32_t* frame_bitmap = nullptr;
static uint64_t frame_bitmap_size = 0;

static spinlock_t paging_lock = {0};
static spinlock_t pfa_lock = {0};

static inline void* phys_to_virt(uintptr_t phys) { return (void*)(phys + KERNEL_VIRT_OFFSET); }
static inline uintptr_t virt_to_phys(void* virt) { return (uintptr_t)virt - KERNEL_VIRT_OFFSET; }


void pfa_mark_used(uintptr_t frame_start, uint64_t frame_count) 
{
    spin_lock(&pfa_lock);
    for (uint64_t i = 0; i < frame_count; i++) 
	{
        uint64_t frame = (frame_start / PAGE_SIZE) + i;
        if (frame < total_frames) 
		{
            if (!(frame_bitmap[frame / 32] & (1 << (frame % 32)))) 
			{
                frame_bitmap[frame / 32] |= (1 << (frame % 32));
                used_frames++;
            }
        }
    }
    spin_unlock(&pfa_lock);
}

void pfa_free_frame(void* frame) 
{
    uint64_t f = (uintptr_t)frame / PAGE_SIZE;
    spin_lock(&pfa_lock);
    if (f < total_frames) 
	{
        if (frame_bitmap[f / 32] & (1 << (f % 32))) 
		{
            frame_bitmap[f / 32] &= ~(1 << (f % 32));
            used_frames--;
        }
    }
    spin_unlock(&pfa_lock);
}

void* pfa_alloc_frame() 
{
    spin_lock(&pfa_lock);
    for (uint64_t i = 0; i < frame_bitmap_size; i++) 
	{
        if (frame_bitmap[i] != 0xFFFFFFFF) 
		{
            uint32_t bit = __builtin_ctz(~frame_bitmap[i]);
            uint64_t frame = i * 32 + bit;
            if (frame < total_frames)
			{
                frame_bitmap[i] |= (1 << bit);
                used_frames++;
                spin_unlock(&pfa_lock);
                return (void*)(frame * PAGE_SIZE);
            }
        }
    }
    spin_unlock(&pfa_lock);
    return nullptr; // Out of Memory
}

void pfa_init_from_multiboot2(void* mb2_ptr) 
{
    struct multiboot_tag *tag;
    uintptr_t mem_upper = 0;
    
    for (tag = (struct multiboot_tag *)((uint8_t*)mb2_ptr + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7))) 
		{
			if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) 
			{
				multiboot_tag_mmap *mmap = (multiboot_tag_mmap *)tag;
				for (multiboot_mmap_entry *entry = mmap->entries;
					(uint8_t *)entry < (uint8_t *)tag + tag->size;
					entry = (multiboot_mmap_entry *)((uintptr_t)entry + mmap->entry_size)) 
					{
						if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) 
						{
							uintptr_t end = entry->addr + entry->len;
							if (end > mem_upper) mem_upper = end;
						}
					}
			}
    	}

    total_frames = mem_upper / PAGE_SIZE;
    frame_bitmap_size = (total_frames / 32) + 1;

    extern uint64_t _kernel_end; 
    uintptr_t kernel_end_phys = virt_to_phys(&_kernel_end);
    frame_bitmap = (uint32_t*)phys_to_virt(kernel_end_phys + 0x2000);
    
    memset(frame_bitmap, 0xFF, frame_bitmap_size * 4);
    used_frames = total_frames;

    for (tag = (struct multiboot_tag *)((uint8_t*)mb2_ptr + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7))) 
		{
			if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) 
			{
				multiboot_tag_mmap *mmap = (multiboot_tag_mmap *)tag;
				for (multiboot_mmap_entry *entry = mmap->entries;
					(uint8_t *)entry < (uint8_t *)tag + tag->size;
					entry = (multiboot_mmap_entry *)((uintptr_t)entry + mmap->entry_size)) 
					{
						if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) 
						{
							uintptr_t start = (entry->addr + 4095) & ~4095;
							uintptr_t end = (entry->addr + entry->len) & ~4095;
							for (uintptr_t a = start; a < end; a += PAGE_SIZE)
							{
								uint64_t frame = a / PAGE_SIZE;
								frame_bitmap[frame / 32] &= ~(1 << (frame % 32));
								used_frames--;
							}
						}
					}
			}
    	}

    pfa_mark_used(0, 256); // BIOS/EBDA/Video
    pfa_mark_used(kernel_end_phys, (frame_bitmap_size * 4 / PAGE_SIZE) + 4);
    pfa_mark_used(virt_to_phys(mb2_ptr), 16);
}

static pt_entry* get_pte(void* virtual_addr, bool create) 
{
    pt_entry* table = pml4;
    uintptr_t addr = (uintptr_t)virtual_addr;
    uintptr_t indices[3] = { PML4_IDX(addr), PDPT_IDX(addr), PD_IDX(addr) };

    for (int i = 0; i < 3; i++) 
	{
        if (!(table[indices[i]] & PTE_PRESENT)) 
		{
            if (!create) return nullptr;
            void* new_tab_phys = pfa_alloc_frame();
            if (!new_tab_phys) return nullptr;
            
            memset(phys_to_virt((uintptr_t)new_tab_phys), 0, PAGE_SIZE);
            table[indices[i]] = (uintptr_t)new_tab_phys | PTE_PRESENT | PTE_RW | PTE_USER;
        }
        table = (pt_entry*)phys_to_virt(table[indices[i]] & ~0xFFFULL);
    }
    return &table[PT_IDX(addr)];
}

void paging_map_page(void* virt, void* phys, uint64_t flags) 
{
    spin_lock(&paging_lock);
    pt_entry* pte = get_pte(virt, true);
    if (pte) 
	{
        *pte = ((uintptr_t)phys & ~0xFFFULL) | flags | PTE_PRESENT;
        mapped_pages++;
        asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
    }
    spin_unlock(&paging_lock);
}

void paging_unmap_page(void* virt) 
{
    spin_lock(&paging_lock);
    pt_entry* pte = get_pte(virt, false);
    if (pte && (*pte & PTE_PRESENT)) 
	{
        *pte = 0;
        mapped_pages--;
        asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
    }
    spin_unlock(&paging_lock);
}

void* paging_get_physical_address(void* virt) 
{
    spin_lock(&paging_lock);
    pt_entry* pte = get_pte(virt, false);
    void* phys = nullptr;
    if (pte && (*pte & PTE_PRESENT)) phys = (void*)((*pte & ~0xFFFULL) + ((uintptr_t)virt & 0xFFF));
    spin_unlock(&paging_lock);
    return phys;
}

void paging_identity_map(uintptr_t start, uintptr_t size, uint64_t flags) 
{
    uintptr_t addr = start & ~0xFFFULL;
    uintptr_t end = (start + size + 0xFFF) & ~0xFFFULL;

    for (; addr < end; addr += PAGE_SIZE)
        paging_map_page((void*)addr, (void*)addr, flags);
}

void paging_get_stats(struct paging_stats* stats) 
{
    if (!stats) return;
    stats->total_frames = total_frames;
    stats->used_frames = used_frames;
    stats->free_frames = total_frames - used_frames;
    stats->mapped_pages = mapped_pages;
}

void paging_init() 
{
    uintptr_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    pml4 = (pt_entry*)phys_to_virt(cr3 & ~0xFFFULL);

    for (uintptr_t p = 0; p < 512 * 1024 * 1024; p += PAGE_SIZE)
        paging_map_page(phys_to_virt(p), (void*)p, PTE_PRESENT | PTE_RW);

    printf("[PAGING] Physic PML4 at 0x%lx\n", cr3 & ~0xFFFULL);
}