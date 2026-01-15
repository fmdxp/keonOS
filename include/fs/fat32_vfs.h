/*
 * keonOS - include/fs/fat32_vfs.cpp
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


#pragma once
#include <fs/vfs_node.h>
#include <fs/fat32_structs.h>


uint8_t fat32_checksum(const char* short_name);

class FAT32Manager 
{
    public:
    uint32_t partition_lba;
    FAT32_BPB bpb;
    
    void init(uint32_t lba);
    uint32_t cluster_to_lba(uint32_t cluster);
    uint32_t get_next_cluster(uint32_t current_cluster);
    void set_next_cluster(uint32_t current_cluster, uint32_t next_cluster);
    uint32_t allocate_cluster();
    uint32_t find_fat32_partition();
    void free_cluster_chain(uint32_t cluster);
};

extern FAT32Manager fat32_inst;

class FAT32_File : public VFSNode 
{
    uint32_t first_cluster;
    FAT32_BPB* bpb;
    uint32_t dir_entry_lba;
    uint32_t dir_entry_offset;
    
    public:
    FAT32_File(const char* n, uint32_t cluster, uint32_t sz, FAT32_BPB* b, uint32_t entry_lba, uint32_t entry_off);
    void open() override {} 
    void close() override {}
    uint32_t read(uint32_t offset, uint32_t size, uint8_t* buffer) override;
    uint32_t write(uint32_t offset, uint32_t size, uint8_t* buffer) override;
    void update_metadata();
};

class FAT32_Directory : public VFSNode 
{
    uint32_t cluster;
    FAT32_BPB* bpb;

public:
    uint32_t read(uint32_t, uint32_t, uint8_t*) { return 0; }
    uint32_t write(uint32_t, uint32_t, uint8_t*) { return 0; }
    void open() { VFSNode::open(); }
    void close() { VFSNode::close(); }
    FAT32_Directory(const char* n, uint32_t c, FAT32_BPB* b);
    VFSNode* finddir(const char* name) override;
    vfs_dirent* readdir(uint32_t index) override;
    bool unlink(const char* name) override;
    int mkdir(const char* name, uint32_t mode) override;
    VFSNode* create(const char* name, uint32_t flags) override;

    
private:
    uint32_t find_free_entry_index(uint32_t* out_lba, uint32_t* out_offset);
};

bool compare_fat_name(const char* fat_name, const char* search_name);