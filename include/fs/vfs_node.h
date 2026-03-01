/*
 * keonOS - include/fs/vfs_node.h
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

#ifndef VFS_NODE_H
#define VFS_NODE_H

#include <stdint.h>
#include <string.h>

enum VFS_TYPE { VFS_FILE = 1, VFS_DIRECTORY = 2, VFS_DEVICE = 3 };

struct vfs_dirent 
{
    char name[128];
    uint32_t inode;
    uint32_t type;
};

class VFSNode 
{
public:
    char name[128];
    uint32_t type;
    uint32_t size;
    uint32_t inode;
    uint32_t ref_count;
    VFSNode* parent;

    VFSNode() : type(0), size(0), inode(0), ref_count(0), parent(nullptr)
    { 
        name[0] = '\0'; 
    }
    virtual ~VFSNode() {}

    virtual uint32_t read(uint32_t offset, uint32_t size, uint8_t* buffer) = 0;
    virtual uint32_t write([[maybe_unused]] uint32_t offset, [[maybe_unused]] uint32_t size, [[maybe_unused]] uint8_t* buffer) { return 0; }
    virtual void open() = 0;
    virtual void close() = 0;
    virtual bool unlink([[maybe_unused]] const char* name) { return false; }
    virtual vfs_dirent* readdir([[maybe_unused]] uint32_t index) { return nullptr; }
    virtual VFSNode* finddir([[maybe_unused]] const char* name) { return nullptr; }
    virtual VFSNode* create([[maybe_unused]] const char* name, [[maybe_unused]] uint32_t flags) { return nullptr; }
    virtual int mkdir([[maybe_unused]] const char* name, [[maybe_unused]] uint32_t mode) { return -1; }
};

class RootFS : public VFSNode 
{
private:
    VFSNode* mounts[32];
    uint32_t mount_count;

    
public:

    RootFS() : mount_count(0) 
	{
        memset(mounts, 0, sizeof(mounts));
        strcpy(name, "/");
        type = VFS_DIRECTORY;
        ref_count = 1;
        inode = 0;
        size = 0;
        parent = this;
    }

    void open() override { ref_count++; }
    void close() override {  if (ref_count > 0) ref_count--; }

	
    void register_node(VFSNode* node) 
    {
        if (mount_count < 32) 
        {
            mounts[mount_count++] = node;
            node->parent = this;
        }
    }


    uint32_t read(uint32_t, uint32_t, uint8_t*) override { return 0; }

    VFSNode* finddir(const char* name) override 
    {
        if (!name || name[0] == '\0' || strcmp(name, "/") == 0)
            return this;

        for (uint32_t i = 0; i < mount_count; i++) 
        {
            if (strcmp(mounts[i]->name, name) == 0) 
            {
                mounts[i]->parent = this;
                return mounts[i];
            }
        }
        return nullptr;
    }


    vfs_dirent* readdir(uint32_t index) override 
    {
        static vfs_dirent de;
        if (index < mount_count) 
        {
            strcpy(de.name, mounts[index]->name);
            de.inode = index;
            de.type = mounts[index]->type;
            return &de;
        }
        return nullptr;
    }
};

class DeviceNode : public VFSNode 
{
public:
    DeviceNode(const char* name) 
    {
        strncpy(this->name, name, 127);
        this->type = VFS_DEVICE;
        this->size = 64 * 1024 * 1024;
    }

    void open() override { ref_count++; }
    void close() override { if (ref_count > 0) ref_count--; }
    
    vfs_dirent* readdir(uint32_t) override { return nullptr; }
    VFSNode* finddir(const char*) override { return nullptr; }

    uint32_t read(uint32_t offset, uint32_t size, uint8_t* buffer) override;
};


class SimpleDirectory : public VFSNode 
{
private:
    VFSNode* children[32];
    uint32_t child_count = 0;
public:
    SimpleDirectory(const char* n) 
    {
        strncpy(this->name, n, 127);
        this->type = VFS_DIRECTORY;
        memset(children, 0, sizeof(children));
    }
    
    void add_child(VFSNode* node) 
    {
        if (child_count < 16) children[child_count++] = node;
    }

    VFSNode* finddir(const char* name) override 
    {
        for (uint32_t i = 0; i < child_count; i++) 
        {
            if (strcmp(children[i]->name, name) == 0) return children[i];
        }
        return nullptr;
    }

    vfs_dirent* readdir(uint32_t index) override 
    {
        static vfs_dirent de;
        if (index < child_count) 
        {
            strcpy(de.name, children[index]->name);
            de.type = children[index]->type;
            de.inode = index;
            return &de;
        }
        return nullptr;
    }

    void open() override { ref_count++; }
    void close() override { ref_count--; }
    uint32_t read(uint32_t, uint32_t, uint8_t*) override { return 0; }
};


class MountOverlayNode : public VFSNode 
{
private:
    VFSNode* underlying;  // The underlying FAT32 directory
    VFSNode* mounts[8];   // Mounted filesystems
    char mount_names[8][128];
    uint32_t mount_count;

public:
    MountOverlayNode(VFSNode* base) : underlying(base), mount_count(0)
    {
        memset(mounts, 0, sizeof(mounts));
        memset(mount_names, 0, sizeof(mount_names));
        
        // Copy properties from underlying node
        strncpy(this->name, base->name, 127);
        this->type = base->type;
        this->size = base->size;
        this->inode = base->inode;
        this->parent = base->parent;
    }

    void add_mount(const char* name, VFSNode* node) 
    {
        if (mount_count < 8) 
        {
            strncpy(mount_names[mount_count], name, 127);
            mounts[mount_count] = node;
            node->parent = this;
            mount_count++;
        }
    }

    VFSNode* finddir(const char* name) override 
    {
        // First check mounted filesystems
        for (uint32_t i = 0; i < mount_count; i++) 
        {
            if (strcmp(mount_names[i], name) == 0) 
            {
                return mounts[i];
            }
        }
        
        // Then check underlying filesystem
        return underlying->finddir(name);
    }

    vfs_dirent* readdir(uint32_t index) override 
    {
        // First list mounted filesystems
        if (index < mount_count) 
        {
            static vfs_dirent de;
            strcpy(de.name, mount_names[index]);
            de.inode = index;
            de.type = mounts[index]->type;
            return &de;
        }
        
        // Then list underlying filesystem entries
        return underlying->readdir(index - mount_count);
    }

    uint32_t read(uint32_t offset, uint32_t size, uint8_t* buffer) override 
    {
        return underlying->read(offset, size, buffer);
    }

    uint32_t write(uint32_t offset, uint32_t size, uint8_t* buffer) override 
    {
        return underlying->write(offset, size, buffer);
    }

    VFSNode* create(const char* name, uint32_t flags) override 
    {
        return underlying->create(name, flags);
    }

    int mkdir(const char* name, uint32_t mode) override 
    {
        return underlying->mkdir(name, mode);
    }

    bool unlink(const char* name) override 
    {
        return underlying->unlink(name);
    }

    void open() override { ref_count++; }
    void close() override { if (ref_count > 0) ref_count--; }
};

#endif      // VFS_NODE