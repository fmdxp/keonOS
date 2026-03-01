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

static pt_entry* kernel_pml4 = nullptr;
static uint64_t total_frames = 0;
static uint64_t used_frames = 0;
static uint64_t mapped_pages = 0;

static uint32_t* frame_bitmap = nullptr;
static uint64_t frame_bitmap_size = 0;

static spinlock_t paging_lock = {0, 0};
static spinlock_t pfa_lock = {0, 0};

extern "C" uint64_t _kernel_virtual_start;
extern "C" uint64_t _kernel_physical_start;
extern "C" uint64_t _kernel_end;


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
                void* phys_ptr = (void*)(frame * PAGE_SIZE);
                memset(phys_to_virt((uintptr_t)phys_ptr), 0, PAGE_SIZE);
                return phys_ptr;
            }
        }
    }
    spin_unlock(&pfa_lock);
    return nullptr;
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

    uintptr_t kernel_end_phys = virt_to_phys(&_kernel_end);
    frame_bitmap = (uint32_t*)phys_to_virt(kernel_end_phys + 0x1000);
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

    pfa_mark_used(0, 256);

    uintptr_t kernel_start_phys = (uintptr_t)&_kernel_physical_start;
    uint64_t kernel_pages = (kernel_end_phys - kernel_start_phys) / PAGE_SIZE + 2;
    pfa_mark_used(kernel_start_phys, kernel_pages);

    pfa_mark_used(virt_to_phys(frame_bitmap), (frame_bitmap_size * 4 / PAGE_SIZE) + 1);
    pfa_mark_used(virt_to_phys(mb2_ptr), 64);

    for (tag = (struct multiboot_tag *)((uint8_t*)mb2_ptr + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7))) 
         {
            if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) 
            {
                auto mod_tag = (multiboot_tag_module *)tag;
                uintptr_t m_start = mod_tag->mod_start;
                uint32_t m_size = mod_tag->mod_end - mod_tag->mod_start;
                uint64_t m_pages = (m_size + 4095) / 4096;
                
                pfa_mark_used(m_start, m_pages);
            }
        }
}

static pt_entry* get_pte(pt_entry* pml4_base, void* virtual_addr, bool create, uint64_t flags = 0)
{
    pt_entry* table = pml4_base;
    uintptr_t addr = (uintptr_t)virtual_addr;
    uintptr_t indices[4] = 
    {
        PML4_IDX(addr),
        PDPT_IDX(addr),
        PD_IDX(addr),
        PT_IDX(addr)
    };

    for (int i = 0; i < 3; i++) 
	{
        if (!(table[indices[i]] & PTE_PRESENT)) 
        {
            if (!create) return nullptr;
            void* new_tab_phys = pfa_alloc_frame();
            if (!new_tab_phys) return nullptr;
            
            table[indices[i]] = (uintptr_t)new_tab_phys | PTE_PRESENT | PTE_RW | (flags & PTE_USER);
        }
        else
        {
            if (flags & PTE_USER) table[indices[i]] |= PTE_USER;
            if (flags & PTE_RW) table[indices[i]] |= PTE_RW;
        }
        table = (pt_entry*)phys_to_virt(table[indices[i]] & ~0xFFFULL);
    }
    return &table[indices[3]];
}

static pt_entry* get_current_pml4_virt() 
{
    uintptr_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return (pt_entry*)phys_to_virt(cr3 & ~0xFFFULL);
}

void paging_map_page(void* virt, void* phys, uint64_t flags) 
{
    spin_lock(&paging_lock);
    pt_entry* pte = get_pte(get_current_pml4_virt(), virt, true, flags);
    if (pte) 
    {
        *pte = ((uintptr_t)phys & ~0xFFFULL) | flags | PTE_PRESENT;
        mapped_pages++;

        // Use invlpg to invalidate the TLB for this specific address
        // This is much faster than reloading CR3 (which flushes the entire TLB)
        asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
    }
    spin_unlock(&paging_lock);
}


void paging_unmap_page(void* virt) 
{
    spin_lock(&paging_lock);
    pt_entry* pte = get_pte(get_current_pml4_virt(), virt, false);
    if (pte && (*pte & PTE_PRESENT)) 
    {
        *pte = 0;
        mapped_pages--;
        asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
    }
    spin_unlock(&paging_lock);
}

void* paging_create_address_space() 
{
    void* new_pml4_phys = pfa_alloc_frame();
    pt_entry* new_pml4_virt = (pt_entry*)phys_to_virt((uintptr_t)new_pml4_phys);
    
    memset(new_pml4_virt, 0, PAGE_SIZE);

    for (int i = 256; i < 512; i++)
        new_pml4_virt[i] = kernel_pml4[i];

    return new_pml4_phys;
}

void* paging_get_physical_address(void* virt) 
{
    spin_lock(&paging_lock);
    pt_entry* pte = get_pte(get_current_pml4_virt(), virt, false);
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

void paging_make_kernel_user_accessible() 
{
    // Map the kernel address space as user-readable/executable
    // This allows Ring 3 threads to execute the shell code currently located in the kernel binary.
    uintptr_t k_start = 0xffffffff80100000;
    uintptr_t k_end = (uintptr_t)&_kernel_end + 0x40000; // 256KB extra (covers help strings)
    
    // Ensure we map in 4KB chunks
    k_start &= ~0xFFFULL;
    k_end = (k_end + 0xFFF) & ~0xFFFULL;

    for (uintptr_t p = k_start; p < k_end; p += 4096)
    {
        paging_map_page((void*)p, (void*)virt_to_phys((void*)p), PTE_PRESENT | PTE_RW | PTE_USER);
    }
}


bool paging_is_user_accessible(void* virt)
{
    pt_entry* table = get_current_pml4_virt();
    uintptr_t addr = (uintptr_t)virt;
    uintptr_t indices[4] = { PML4_IDX(addr), PDPT_IDX(addr), PD_IDX(addr), PT_IDX(addr) };

    for (int i = 0; i < 4; i++) 
    {
        if (!(table[indices[i]] & PTE_PRESENT)) return false;
        if (!(table[indices[i]] & PTE_USER)) return false;
        if (i < 3) table = (pt_entry*)phys_to_virt(table[indices[i]] & ~0xFFFULL);
    }
    return true;
}


void paging_init() 
{
    void* new_pml4_phys = pfa_alloc_frame();
    kernel_pml4 = (pt_entry*)phys_to_virt((uintptr_t)new_pml4_phys);
    memset(kernel_pml4, 0, PAGE_SIZE);
    
    for (uintptr_t p = 0; p < 512 * 1024 * 1024; p += PAGE_SIZE)
    {
        pt_entry* pte1 = get_pte(kernel_pml4, (void*)p, true, PTE_PRESENT | PTE_RW);
        *pte1 = p | PTE_PRESENT | PTE_RW;

        pt_entry* pte2 = get_pte(kernel_pml4, phys_to_virt(p), true, PTE_PRESENT | PTE_RW);
        *pte2 = p | PTE_PRESENT | PTE_RW;
    }

    for (uintptr_t p = 0; p < 1024 * 1024; p += PAGE_SIZE) 
        paging_map_page((void*)(0xffffffffc0000000 + p), (void*)p, PTE_PRESENT | PTE_RW);

    uintptr_t vga_phys = 0xB8000;
    uintptr_t vga_virt = 0xffffffff800b8000;

    pt_entry* vga_pte = get_pte(kernel_pml4, (void*)vga_virt, true, PTE_PRESENT | PTE_RW);
    *vga_pte = vga_phys | PTE_PRESENT | PTE_RW;
    asm volatile("mov %0, %%cr3" : : "r"(new_pml4_phys) : "memory");

    printf("[PAGING] Paging active\n");
}