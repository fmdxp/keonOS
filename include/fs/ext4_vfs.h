/*
 * keonOS - include/fs/ext4_vfs.h
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

#ifndef EXT4_VFS_H
#define EXT4_VFS_H

#include <fs/ext4_structs.h>
#include <fs/vfs_node.h>
#include <fs/ext4_journal.h>
#include <stdint.h>

// Forward declarations
class Ext4Manager;
class Ext4File;
class Ext4Directory;

// Global ext4 instance
extern Ext4Manager ext4_inst;

// Main ext4 filesystem manager
class Ext4Manager {
public:
    Ext4Superblock sb;
    JBD2 journal;
    uint32_t partition_lba;
    uint32_t block_size;
    uint32_t inode_size;
    uint32_t inodes_per_group;
    uint32_t blocks_per_group;
    uint32_t group_desc_size;
    uint32_t groups_count;
    
    // Initialization
    void init(uint32_t lba);
    uint32_t find_ext4_partition();
    
    // Block operations
    void read_block(uint64_t block_num, uint8_t* buffer);
    void write_block(uint64_t block_num, uint8_t* buffer);
    void journal_write_block(uint64_t block_num, uint8_t* buffer); // Journal-aware write
    void read_blocks(uint64_t block_num, uint32_t count, uint8_t* buffer);
    void write_blocks(uint64_t block_num, uint32_t count, uint8_t* buffer);
    
    // Group descriptor operations
    void read_group_desc(uint32_t group, Ext4GroupDesc* desc);
    void write_group_desc(uint32_t group, Ext4GroupDesc* desc);
    uint64_t get_block_bitmap(Ext4GroupDesc* desc);
    uint64_t get_inode_bitmap(Ext4GroupDesc* desc);
    uint64_t get_inode_table(Ext4GroupDesc* desc);
    
    // Inode operations
    void read_inode(uint32_t inode_num, Ext4Inode* inode);
    void write_inode(uint32_t inode_num, Ext4Inode* inode);
    uint32_t get_inode_group(uint32_t inode_num);
    uint32_t get_inode_index(uint32_t inode_num);
    
    // Extent operations
    uint64_t extent_get_block(Ext4Inode* inode, uint32_t logical_block);
    bool extent_allocate_blocks(Ext4Inode* inode, uint32_t logical_start, uint32_t count);
    
    // Bitmap operations
    bool test_block_bitmap(uint32_t group, uint32_t block_in_group);
    void set_block_bitmap(uint32_t group, uint32_t block_in_group, bool value);
    bool test_inode_bitmap(uint32_t group, uint32_t inode_in_group);
    void set_inode_bitmap(uint32_t group, uint32_t inode_in_group, bool value);
    
    // Allocation
    uint64_t allocate_block();
    uint32_t allocate_inode(bool is_directory);
    void free_block(uint64_t block);
    void free_inode(uint32_t inode_num);
    
    // Utility
    bool check_feature_incompat(uint32_t feature);
    bool check_feature_compat(uint32_t feature);
    bool check_feature_ro_compat(uint32_t feature);
};

// ext4 file node
class Ext4File : public VFSNode {
private:
    uint32_t inode_num;
    Ext4Inode inode;
    Ext4Superblock* sb;
    uint32_t dir_entry_block;   // For updating metadata
    uint32_t dir_entry_offset;
    
    bool check_permission(uint16_t required_mode);
    void update_metadata();
    
public:
    Ext4File(const char* n, uint32_t ino, Ext4Superblock* s);
    Ext4File(const char* n, uint32_t ino, Ext4Superblock* s, uint32_t entry_block, uint32_t entry_offset);
    
    uint32_t read(uint32_t offset, uint32_t size, uint8_t* buffer) override;
    uint32_t write(uint32_t offset, uint32_t size, uint8_t* buffer) override;
    void open() override;
    void close() override;
    
    uint32_t get_inode_num() { return inode_num; }
    Ext4Inode* get_inode() { return &inode; }
};

// ext4 directory node
class Ext4Directory : public VFSNode {
private:
    uint32_t inode_num;
    Ext4Inode inode;
    Ext4Superblock* sb;
    
    bool check_permission(uint16_t required_mode);
    uint32_t find_free_entry_space(uint32_t required_size, uint64_t* out_block, uint32_t* out_offset);
    
public:
    Ext4Directory(const char* n, uint32_t ino, Ext4Superblock* s);
    
    VFSNode* finddir(const char* name) override;
    vfs_dirent* readdir(uint32_t index) override;
    int mkdir(const char* name, uint32_t mode) override;
    VFSNode* create(const char* name, uint32_t flags) override;
    bool unlink(const char* name) override;
    
    void open() override;
    void close() override;
    uint32_t read(uint32_t offset, uint32_t size, uint8_t* buffer) override;
    
    uint32_t get_inode_num() { return inode_num; }
    Ext4Inode* get_inode() { return &inode; }
};

// Helper functions
bool compare_ext4_name(const char* entry_name, uint8_t entry_len, const char* search_name);
uint8_t get_ext4_file_type(uint16_t mode);

#endif // EXT4_VFS_H
