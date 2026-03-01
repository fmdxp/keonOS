 /*
 * keonOS - kernel/exec/kex_loader.cpp
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



#include <exec/kex.h>
#include <exec/kex_loader.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <kernel/arch/x86_64/thread.h>
#include <kernel/arch/x86_64/paging.h>
#include <sys/errno.h>
#include <string.h>
#include <stdio.h>

static bool validate_elf(Elf64_Ehdr* hdr) 
{
    if (hdr->e_ident[EI_MAG0] != ELFMAG0 || 
        hdr->e_ident[EI_MAG1] != ELFMAG1 || 
        hdr->e_ident[EI_MAG2] != ELFMAG2 || 
        hdr->e_ident[EI_MAG3] != ELFMAG3) return false;
        
    if (hdr->e_ident[EI_CLASS] != ELFCLASS64) return false;
    if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) return false;
    if (hdr->e_machine != EM_X86_64) return false;
    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN) return false;
    
    return true;
}

int kex_load(const char* path, [[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    VFSNode* file = vfs_open(path);
    if (!file) {
        printf("Error: File not found '%s'.\n", path);
        return -1;
    }

    Elf64_Ehdr hdr;
    if (vfs_read(file, 0, sizeof(Elf64_Ehdr), (uint8_t*)&hdr) != sizeof(Elf64_Ehdr)) {
        printf("Error: Cannot read ELF header.\n");
        vfs_close(file);
        return -1;
    }

    if (!validate_elf(&hdr)) {
        printf("Error: Invalid ELF/KEX format.\n");
        vfs_close(file);
        return -1;
    }

    // Read Program Headers
    uint32_t ph_size = hdr.e_phnum * hdr.e_phentsize;
    uint8_t* ph_buf = (uint8_t*)kmalloc(ph_size);
    if (vfs_read(file, hdr.e_phoff, ph_size, ph_buf) != ph_size) {
        printf("Error: Cannot read Program Headers.\n");
        kfree(ph_buf);
        vfs_close(file);
        return -1;
    }

    Elf64_Phdr* ph = (Elf64_Phdr*)ph_buf;
    bool kex_note_found = false;

    // First Pass: Check for KEX Note
    for (int i = 0; i < hdr.e_phnum; i++) {
        if (ph[i].p_type == PT_NOTE) {
            uint8_t* note_buf = (uint8_t*)kmalloc(ph[i].p_filesz);
            vfs_read(file, ph[i].p_offset, ph[i].p_filesz, note_buf);
            
            struct Elf_Note {
                uint32_t namesz;
                uint32_t descsz;
                uint32_t type;
            } *note = (Elf_Note*)note_buf;
            
            char* note_name = (char*)(note + 1);
            if (note->type == KEX_NOTE_TYPE_VERSION && 
                note->namesz == 7 && 
                strcmp(note_name, KEX_NOTE_NAME) == 0) 
            {
                 kex_note_found = true;
                 
                 // Check version in desc (KEX1)
                 // void* desc = (void*)(note_name + 8); // aligned(7)=8
                 // We can parse KexHeader here for stack requirements
            }
            kfree(note_buf);
        }
    }

    if (!kex_note_found) {
        printf("Error: No valid KEX signature found.\n");
        kfree(ph_buf);
        vfs_close(file);
        return -1;
    }

    // Create User Thread
    // Disable interrupts to prevent scheduler from picking up the new thread before content is loaded (Race Condition Fix)
    // Use thread_add to ensure thread is linked to scheduler list
    asm volatile("cli");
    thread_t* t = thread_add((void(*)())hdr.e_entry, path, true);
    if (t) t->state = THREAD_BLOCKED;
    asm volatile("sti");

    if (!t) 
    {
        printf("Error: Failed to create user thread.\n");
        kfree(ph_buf);
        vfs_close(file);
        return -1;
    }

    // Load Segments
    uintptr_t max_vaddr = 0x0;
    uintptr_t min_vaddr = ~0; // Max uintptr_t

    for (int i = 0; i < hdr.e_phnum; i++) 
    {
        if (ph[i].p_type == PT_LOAD) 
        {
            uintptr_t vaddr_start = ph[i].p_vaddr;
            uintptr_t vaddr_end = ph[i].p_vaddr + ph[i].p_memsz;
            
            uintptr_t file_offset = ph[i].p_offset;
            uintptr_t file_size = ph[i].p_filesz;
            
            if (vaddr_end == vaddr_start) continue;

            if (vaddr_end > max_vaddr) max_vaddr = vaddr_end;
            if (vaddr_start < min_vaddr) min_vaddr = vaddr_start;

            uintptr_t page_start = vaddr_start & ~0xFFF;
            uintptr_t page_end = (vaddr_end + 0xFFF) & ~0xFFF;
            
            // Force RW to allow loading content. Security TODO: Remap RO after load if needed.
            uint32_t flags = PTE_PRESENT | PTE_USER | PTE_RW; 
            if (!(ph[i].p_flags & PF_X)) {} // NX bit?

            // Map Pages
            for (uintptr_t addr = page_start; addr < page_end; addr += 4096) {
                // Check if already mapped? 
                // We trust pfa will give fresh frames.
                // TODO: Zero pages for security (pfa_alloc doesn't guarantee zero)
                void* phys = pfa_alloc_frame();
                if (!phys) 
                {
                    // Cleanup partial thread
                    t->user_image_start = min_vaddr & ~0xFFF;
                    t->user_image_end = (max_vaddr + 0xFFF) & ~0xFFF;
                    thread_kill(t->id);
                    
                    kfree(ph_buf);
                    vfs_close(file);
                    return -1;
                }
                
                paging_map_page((void*)addr, phys, flags);
                
                // Zero page manually
                memset((void*)addr, 0, 4096); 
            }
            
            // Read file content
            if (file_size > 0) {
                 vfs_read(file, file_offset, file_size, (uint8_t*)vaddr_start);
            }
        }
    }

    if (t) 
    {
        // Align to page boundary
        // Align to page boundary
        
        // Start Heap after Image (aligned to 4KB)
        // Note: Original code hardcoded heap start at 1GB (0x40000000).
        // To be safe/compatible with existing sbrk logic, let's keep heap start at 1GB?
        // But we want to track the image size.
        
        t->user_image_start = min_vaddr & ~0xFFF;
        t->user_image_end = (max_vaddr + 0xFFF) & ~0xFFF;
        
        uintptr_t heap_start_standard = 0x40000000;
        if (max_vaddr < heap_start_standard) max_vaddr = heap_start_standard;
        
        t->user_heap_break = (max_vaddr + 0xFFF) & ~0xFFF;
    }

    // Flush TLB to ensure User permissions are visible in upper paging levels
    // This fixes #PF 0x5 when switching to User Mode if upper paging directories were cached as Supervisor-only.
    uintptr_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");

    kfree(ph_buf);
    vfs_close(file);
    
    // Wake up thread
    asm volatile("cli");
    t->state = THREAD_READY;
    asm volatile("sti");
    
    asm volatile("sti");
    return t->id;
}


uintptr_t kdl_load(const char* path, thread_t* t)
{
    if (!t) return 0;

    VFSNode* file = vfs_open(path);
    if (!file) 
    {
        printf("Error: Library not found '%s'.\n", path);
        return 0;
    }

    Elf64_Ehdr hdr;
    if (vfs_read(file, 0, sizeof(Elf64_Ehdr), (uint8_t*)&hdr) != sizeof(Elf64_Ehdr)) 
    {
        printf("Error: Cannot read ELF header.\n");
        vfs_close(file);
        return 0;
    }

    if (!validate_elf(&hdr) || hdr.e_type != ET_DYN) 
    {
        printf("Error: Invalid dynamic library format.\n");
        printf("Debug: Magic: %c%c%c, Class: %d, Data: %d, Machine: %d, Type: %d\n", 
            hdr.e_ident[1], hdr.e_ident[2], hdr.e_ident[3], 
            hdr.e_ident[EI_CLASS], hdr.e_ident[EI_DATA], 
            hdr.e_machine, hdr.e_type);
        vfs_close(file);
        return 0;
    }

    // Read Program Headers
    uint32_t ph_size = hdr.e_phnum * hdr.e_phentsize;
    uint8_t* ph_buf = (uint8_t*)kmalloc(ph_size);
    if (vfs_read(file, hdr.e_phoff, ph_size, ph_buf) != ph_size) 
    {
        printf("Error: Cannot read Program Headers.\n");
        kfree(ph_buf);
        vfs_close(file);
        return 0;
    }

    Elf64_Phdr* ph = (Elf64_Phdr*)ph_buf;
    
    // 1. Calculate the required size of the library in memory
    uintptr_t lib_size = 0;
    for (int i = 0; i < hdr.e_phnum; i++) 
    {
        if (ph[i].p_type == PT_LOAD) 
        {
            uintptr_t seg_end = ph[i].p_vaddr + ph[i].p_memsz;
            if (seg_end > lib_size) lib_size = seg_end;
        }
    }
    
    // Page-align library size
    lib_size = (lib_size + 0xFFF) & ~0xFFF;

    // 2. Assign a base virtual address for this library
    // We can use a high memory region for shared libraries, e.g. 0x700000000000
    // For simplicity, we assign a fixed base or we can scan for free space.
    // Let's implement a simple bump allocator for dynamic libraries per thread:
    // We need to store this in thread_t. For now, we will use a fixed hardcoded
    // region starting at 0x700000000000 and increment it by the library size.
    // Assuming t->dyn_lib_break initialized to 0x700000000000
    if (t->dyn_lib_break == 0) t->dyn_lib_break = 0x700000000000;
    
    uintptr_t load_base = t->dyn_lib_break;
    t->dyn_lib_break += lib_size;
    

    // 3. Map Segments
    Elf64_Dyn* dyn_table = nullptr;
    
    for (int i = 0; i < hdr.e_phnum; i++) 
    {
        if (ph[i].p_type == PT_LOAD) 
        {
            uintptr_t vaddr_start = load_base + ph[i].p_vaddr;
            uintptr_t vaddr_end = load_base + ph[i].p_vaddr + ph[i].p_memsz;
            
            uintptr_t file_offset = ph[i].p_offset;
            uintptr_t file_size = ph[i].p_filesz;
            
            if (vaddr_end == vaddr_start) continue;

            uintptr_t page_start = vaddr_start & ~0xFFF;
            uintptr_t page_end = (vaddr_end + 0xFFF) & ~0xFFF;
            
            uint32_t flags = PTE_PRESENT | PTE_USER | PTE_RW; 

            // Map Pages
            for (uintptr_t addr = page_start; addr < page_end; addr += 4096) 
            {
                void* phys = pfa_alloc_frame();
                if (!phys) 
                {
                    kfree(ph_buf);
                    vfs_close(file);
                    return 0;
                }
                
                paging_map_page((void*)addr, phys, flags);
                memset((void*)addr, 0, 4096); 
            }
            
            // Read file content
            if (file_size > 0) 
                 vfs_read(file, file_offset, file_size, (uint8_t*)vaddr_start);
        }
        else if (ph[i].p_type == PT_DYNAMIC)
            dyn_table = (Elf64_Dyn*)(load_base + ph[i].p_vaddr);
    }

    // 4. Process Relocations
    if (dyn_table)
    {
        Elf64_Rela* rela = nullptr;
        size_t rela_sz = 0;
        size_t rela_ent = 0;

        for (Elf64_Dyn* d = dyn_table; d->d_tag != DT_NULL; d++)
        {
            if (d->d_tag == DT_RELA) rela = (Elf64_Rela*)(load_base + d->d_un.d_ptr);
            else if (d->d_tag == DT_RELASZ) rela_sz = d->d_un.d_val;
            else if (d->d_tag == DT_RELAENT) rela_ent = d->d_un.d_val;
        }

        if (rela && rela_ent > 0)
        {
            size_t num_relas = rela_sz / rela_ent;
            for (size_t i = 0; i < num_relas; i++)
            {
                if (ELF64_R_TYPE(rela[i].r_info) == R_X86_64_RELATIVE)
                {
                    uintptr_t target = load_base + rela[i].r_offset;
                    uint64_t* ptr = (uint64_t*)target;
                    *ptr = load_base + rela[i].r_addend;
                }
            }
        }
    }

    // Flush TLB
    uintptr_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");

    kfree(ph_buf);
    vfs_close(file);
    
    return load_base;
}
