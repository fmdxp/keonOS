/*
 * keonOS - include/fs/ramfs_vfs.h
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


#ifndef RAMFS_VFS_H
#define RAMFS_VFS_H

#include <kernel/error.h>
#include <fs/vfs_node.h>
#include <fs/ramfs.h>
#include <mm/heap.h>


class KeonFS_File : public VFSNode 
{
public:
    void* data_ptr;
    KeonFS_File(const char* n, uint32_t s, void* ptr);
    
    uint32_t read(uint32_t offset, uint32_t size, uint8_t* buffer) override;
    void open() override;
    void close() override;
};


class KeonFS_MountNode : public VFSNode 
{
public:
    void* base;
    KeonFS_Info* info;
    VFSNode* children[128];
    uint32_t children_count;

    KeonFS_MountNode(const char* mount_name, void* addr) : children_count(0)
    {
        memset(children, 0, sizeof(children));
        strncpy(this->name, mount_name, 127);

        this->base = addr;
        this->info = (KeonFS_Info*)addr;


        if (this->info->magic != KEONFS_MAGIC) 
            panic(KernelError::K_ERR_RAMFS_MAGIC_FAILED, nullptr, 0UL);
        

        this->size = info->fs_size;
        this->type = VFS_DIRECTORY;

        KeonFS_FileHeader* hdr = (KeonFS_FileHeader*)((uintptr_t)base + sizeof(KeonFS_Info));


        for (uint32_t i = 0; i < info->total_files; i++)
        {

            if (hdr->offset + hdr->size > info->fs_size)
                panic(KernelError::K_ERR_GENERAL_PROTECTION, "File not in ramdisk", 0UL);
            

            KeonFS_File* child = new KeonFS_File(hdr->name, hdr->size, (void*)((uintptr_t)this->base + hdr->offset));
            add_child(child);

            if (hdr->next_header == 0) break;
            hdr = (KeonFS_FileHeader*)((uintptr_t)base + hdr->next_header);
        }
    }

    VFSNode* finddir(const char* name) override;
    vfs_dirent* readdir(uint32_t index) override;
    
    void add_child(VFSNode* node);
    uint32_t read(uint32_t offset, uint32_t size, uint8_t* buffer) override;
    
    // Read-only filesystem - disable write operations
    uint32_t write(uint32_t, uint32_t, uint8_t*) override { return 0; }
    VFSNode* create(const char*, uint32_t) override { return nullptr; }
    int mkdir(const char*, uint32_t) override { return -1; }
    bool unlink(const char*) override { return false; }
    
    void open() override {}
    void close() override {}
};

#endif // RAMFS_VFS_H