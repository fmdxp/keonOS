/*
 * keonOS - fs/vfs.cpp
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


#include <fs/vfs_node.h>
#include <drivers/ata.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <stdio.h>

static uint8_t rootfs_buffer[sizeof(RootFS)]; 
VFSNode* vfs_root = nullptr;
VFSNode* cwd_node = nullptr;

static const char* skip_slashes(const char* path) 
{
    while (*path == '/') path++;
    return path;
}

void vfs_init() 
{
    vfs_root = new (rootfs_buffer) RootFS();
}

void vfs_mount(VFSNode* node) 
{
    if (!vfs_root || !node) return;

    printf("[DEBUG] vfs_root addr: %x\n", vfs_root);
    printf("[DEBUG] vfs_root vptr: %x\n", *(uint32_t*)vfs_root);

    ((RootFS*)vfs_root)->register_node(node);
}

VFSNode* vfs_open(const char* path) 
{
    if (!vfs_root || !path) return nullptr;

    VFSNode* current = vfs_root;
    
    if (path[0] == '/') 
    {
        current = vfs_root;
        path = skip_slashes(path);
    } 
    else current = (cwd_node != nullptr) ? cwd_node : vfs_root;
    
    current->open();

    if (*path == '\0') return current;
    
    char component[128];
    const char* p = path;

    while (*p != '\0') 
	{
        uint32_t i = 0;
        while (*p != '/' && *p != '\0' && i < 127) component[i++] = *p++;
        component[i] = '\0';

        VFSNode* next = nullptr;

        if (strcmp(component, ".") == 0) 
        {
            p = skip_slashes(p);
            continue;
        } 

        else if (strcmp(component, "..") == 0) 
        {
            if (current == vfs_root) next = vfs_root;
            else next = current->parent; 
            if (next) next->open();
        } 
        else next = current->finddir(component);
        
        if (!next) 
        {
            current->close();
            return nullptr;
        }

        VFSNode* old = current;
        current = next;
        old->close();
        p = skip_slashes(p);
    }
    return current;
}

uint32_t vfs_read(VFSNode* node, uint32_t offset, uint32_t size, uint8_t* buffer) 
{
    return (node) ? node->read(offset, size, buffer) : 0;
}

uint32_t vfs_write(VFSNode* node, uint32_t offset, uint32_t size, uint8_t* buffer) 
{
    return (node) ? node->write(offset, size, buffer) : 0;
}

vfs_dirent* vfs_readdir(VFSNode* node, uint32_t index) 
{
    return (node) ? node->readdir(index) : nullptr;
}

void vfs_close(VFSNode* node) 
{
    if (node) node->close();
}

void VFSNode::open() { ref_count++; }
void VFSNode::close() 
{ 
    ref_count--; 
    if (ref_count <= 0 && this != vfs_root) delete this;
}

bool vfs_unlink(const char* path) 
{
    if (!vfs_root || !path || path[0] == '\0') return false;

    char parent_path[256];
    char filename[128];
    
    const char* last_slash = strrchr(path, '/');

    if (!last_slash) 
    {
        strcpy(parent_path, ".");
        strcpy(filename, path);
    }
    else if (last_slash == path) 
    {
        strcpy(parent_path, "/");
        strcpy(filename, path + 1);
    }
    else 
    {
        size_t parent_len = last_slash - path;
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
        strcpy(filename, last_slash + 1);
    }

    VFSNode* parent_node = vfs_open(parent_path);
    if (!parent_node) return false;

    bool result = parent_node->unlink(filename);

    vfs_close(parent_node);
    return result;
}


uint32_t DeviceNode::read(uint32_t offset, uint32_t size, uint8_t* buffer) 
{
    uint32_t start_sector = offset / 512;
    uint32_t end_sector = (offset + size - 1) / 512;
    uint32_t num_sectors = end_sector - start_sector + 1;

    uint8_t* temp_buffer = (uint8_t*)kmalloc(num_sectors * 512);
    if (!temp_buffer) return 0;

    ATADriver::read_sectors(start_sector, num_sectors, temp_buffer);
    uint32_t offset_in_first_sector = offset % 512;
    memcpy(buffer, temp_buffer + offset_in_first_sector, size);
    kfree(temp_buffer);

    return size;
}


VFSNode* vfs_create(const char* path, uint32_t flags) 
{
    if (!vfs_root || !path || *path == '\0') return nullptr;

    char parent_path[256];
    char filename[128];
    const char* last_slash = strrchr(path, '/');

    if (!last_slash) 
    {
        strcpy(parent_path, ".");
        strcpy(filename, path);
    }
    else if (last_slash == path) 
    {
        strcpy(parent_path, "/");
        strcpy(filename, path + 1);
    }
    else 
    {
        size_t parent_len = last_slash - path;
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
        strcpy(filename, last_slash + 1);
    }

    VFSNode* parent_node = vfs_open(parent_path);
    if (!parent_node) return nullptr;

    VFSNode* new_node = parent_node->create(filename, flags);

    vfs_close(parent_node);
    return new_node;
}


int vfs_mkdir(const char* path, [[maybe_unused]] uint32_t mode) 
{
    if (!vfs_root || !path || *path == '\0') return -1;

    char parent_path[256];
    char dirname[128];
    const char* last_slash = strrchr(path, '/');

    if (!last_slash) 
    {
        strcpy(parent_path, ".");
        strcpy(dirname, path);
    }
    else if (last_slash == path) 
    {
        strcpy(parent_path, "/");
        strcpy(dirname, path + 1);
    }
    else 
    {
        size_t parent_len = last_slash - path;
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
        strcpy(dirname, last_slash + 1);
    }

    VFSNode* parent_node = vfs_open(parent_path);
    if (!parent_node) return -1;

    int result = parent_node->mkdir(dirname, mode); 

    vfs_close(parent_node);
    return result;
}