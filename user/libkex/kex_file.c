/*
 * keonOS - user/libkex/kex_file.c
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

#include "kex.h"
#include <string.h>

// Simply verify it is a valid ELF64 with correct machine type
int kex_verify(void* buffer, uint32_t size) {
    if (size < sizeof(Elf64_Ehdr)) return 0;
    
    Elf64_Ehdr* hdr = (Elf64_Ehdr*)buffer;
    if (hdr->e_ident[0] != ELFMAG0 || 
        hdr->e_ident[1] != ELFMAG1 || 
        hdr->e_ident[2] != ELFMAG2 || 
        hdr->e_ident[3] != ELFMAG3) return 0;
        
    if (hdr->e_machine != EM_X86_64) return 0;
    
    return 1;
}

// Locate KexHeader inside .note.kex
// This is a naive implementation that scans program headers like the kernel does.
// Since this is userspace, we assume we have the whole file in buffer.
struct KexHeader* kex_get_header(void* buffer, uint32_t size) {
    Elf64_Ehdr* hdr = (Elf64_Ehdr*)buffer;
    
    if (hdr->e_phoff + hdr->e_phnum * hdr->e_phentsize > size) return 0;
    
    typedef struct {
        uint32_t p_type;
        uint32_t p_flags;
        uint64_t p_offset;
        uint64_t p_vaddr;
        uint64_t p_paddr;
        uint64_t p_filesz;
        uint64_t p_memsz;
        uint64_t p_align;
    } Elf64_Phdr;
    
    Elf64_Phdr* ph = (Elf64_Phdr*)((uint8_t*)buffer + hdr->e_phoff);
    
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (ph[i].p_type == 4) { // PT_NOTE
             uint8_t* note_area = (uint8_t*)buffer + ph[i].p_offset;
             // Check bounds
             if (ph[i].p_offset + ph[i].p_filesz > size) continue;
             
             struct Elf_Note {
                uint32_t namesz;
                uint32_t descsz;
                uint32_t type;
            } *note = (struct Elf_Note*)note_area;
            
            char* name = (char*)(note + 1);
            if (note->type == KEX_NOTE_TYPE_VERSION && 
                note->namesz == 7 && 
                strcmp(name, KEX_NOTE_NAME) == 0) 
            {
                 // Align 4 bytes. 
                 // name is 7 bytes + 1 null = 8 bytes. 
                 // sizeof(Elf_Note) = 12. 
                 // 12 + 8 = 20. 
                 // The desc follows immediately.
                 return (struct KexHeader*)(name + 8);
            }
        }
    }
    return 0;
}
