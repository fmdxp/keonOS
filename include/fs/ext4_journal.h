/*
 * keonOS - include/fs/ext4_journal.h
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

#ifndef EXT4_JOURNAL_H
#define EXT4_JOURNAL_H

#include <stdint.h>

// JBD2 Magic number
#define JBD2_MAGIC_NUMBER 0xC03B3998U

// Journal block types
#define JBD2_DESCRIPTOR_BLOCK 1
#define JBD2_COMMIT_BLOCK     2
#define JBD2_SUPERBLOCK_V1    3
#define JBD2_SUPERBLOCK_V2    4
#define JBD2_REVOKE_BLOCK     5

// Journal header
struct journal_header_t {
    uint32_t h_magic;
    uint32_t h_blocktype;
    uint32_t h_sequence;
} __attribute__((packed));

// Journal superblock
struct journal_superblock_t {
    journal_header_t s_header;
    uint32_t s_blocksize;
    uint32_t s_maxlen;
    uint32_t s_first;
    uint32_t s_sequence;
    uint32_t s_start;
    uint32_t s_errno;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    uint32_t s_nr_users;
    uint32_t s_dynsuper;
    uint32_t s_max_transaction;
    uint32_t s_max_trans_data;
    uint8_t  s_checksum_type;
    uint8_t  s_padding2[3];
    uint32_t s_padding[42];
    uint32_t s_checksum;
    uint8_t  s_users[16*48];
} __attribute__((packed));

class Ext4Manager;

class JBD2 {
public:
    Ext4Manager* fs;
    uint32_t journal_inode_num;
    uint32_t block_size;
    uint32_t first_block; // Logical block start of ring buffer
    uint32_t max_blocks;
    uint32_t current_seq;
    
    // Init
    void init(Ext4Manager* fs_ptr, uint32_t inode);
    
    // Transaction mngt
    void start_transaction();
    void log_block(uint64_t fs_block, uint8_t* data);
    void commit_transaction();
    
private:
    uint32_t head_block; // Current write position in journal
    void write_journal_block(uint32_t offset_block, uint8_t* data);
    uint32_t get_next_block_wrapper(uint32_t current);
};

#endif
