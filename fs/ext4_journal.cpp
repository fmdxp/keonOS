/*
 * keonOS -fs/ext4_journal.cpp
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

#include <fs/ext4_journal.h>
#include <fs/ext4_vfs.h>
#include <mm/heap.h>
#include <kernel/panic.h>
#include <string.h>
#include <stdio.h>

void JBD2::init(Ext4Manager* fs_ptr, uint32_t inode)
{
    this->fs = fs_ptr;
    this->journal_inode_num = inode;
    this->block_size = fs->block_size;
    
    // Read journal superblock (usually inside the journal inode data)
    // For simplicity we assume it's initialized and just start appending
    // In a real recovery scenario we would read s_start/s_sequence
    
    // TODO: We should read the journal superblock to find head
    this->head_block = 1; // Skip superblock
    this->current_seq = 1;
    this->max_blocks = 1024; // Arbitrary limit for now if not reading SB
    
    printf("[JBD2] Journal initialized on Inode %d\n", inode);
}

void JBD2::write_journal_block(uint32_t offset_block, uint8_t* data)
{
    // Map logical journal block to physical disk block using Ext4's extent tree
    // The journal itself is just a file!
    
    Ext4Inode journal_inode;
    fs->read_inode(journal_inode_num, &journal_inode);
    
    uint64_t phys_block = fs->extent_get_block(&journal_inode, offset_block);
    if (phys_block != 0) fs->write_block(phys_block, data);
    else printf("[JBD2] Error: Journal file hole at block %d\n", offset_block);
}

uint32_t JBD2::get_next_block_wrapper(uint32_t current)
{
    current++;
    if (current >= max_blocks) current = 1; // Wrap around (ring buffer), skip SB
    return current;
}

struct log_entry 
{
    uint64_t fs_block; // Physical block on FS
    uint8_t* data;
};
static log_entry current_trans[MAX_TRANS_BLOCKS];
static int trans_count = 0;

void JBD2::start_transaction()
{
    trans_count = 0;
    current_seq++;
}

void JBD2::log_block(uint64_t fs_block, uint8_t* data)
{
    if (trans_count >= MAX_TRANS_BLOCKS) 
    {
        commit_transaction();
        start_transaction();
    }
    
    // Copy data to buffer
    uint8_t* copy = (uint8_t*)kmalloc(block_size);
    memcpy(copy, data, block_size);
    
    current_trans[trans_count].fs_block = fs_block;
    current_trans[trans_count].data = copy;
    trans_count++;
}

void JBD2::commit_transaction()
{
    if (trans_count == 0) return;
    
    // 1. Write Descriptor Block
    uint8_t* desc_buf = (uint8_t*)kmalloc(block_size);
    memset(desc_buf, 0, block_size);
    
    journal_header_t* header = (journal_header_t*)desc_buf;
    header->h_magic = 0xC03B3998; // Big endian magic usually? Ext4 is little endian on x86
    header->h_blocktype = __builtin_bswap32(JBD2_DESCRIPTOR_BLOCK); 
    header->h_sequence = __builtin_bswap32(current_seq);
    
    // Descriptor tags follow header... simplified for now
    // We assume 1-to-1 mapping for this basic implementation
    
    write_journal_block(head_block, desc_buf);
    head_block = get_next_block_wrapper(head_block);
    kfree(desc_buf);
    
    // 2. Write Data Blocks
    for (int i = 0; i < trans_count; i++) 
    {
        write_journal_block(head_block, current_trans[i].data);
        head_block = get_next_block_wrapper(head_block);
        // Also write check-pointing (write to real FS)
        // In ordered mode, we write to FS *before* commit block
        fs->write_block(current_trans[i].fs_block, current_trans[i].data);
        kfree(current_trans[i].data);
    }
    
    // 3. Write Commit Block
    uint8_t* commit_buf = (uint8_t*)kmalloc(block_size);
    memset(commit_buf, 0, block_size);
    
    header = (journal_header_t*)commit_buf;
    header->h_magic = 0xC03B3998;
    header->h_blocktype = __builtin_bswap32(JBD2_COMMIT_BLOCK);
    header->h_sequence = __builtin_bswap32(current_seq);
    
    write_journal_block(head_block, commit_buf);
    head_block = get_next_block_wrapper(head_block);
    kfree(commit_buf);
    
    trans_count = 0;
    // printf("[JBD2] Transaction %d committed\n", current_seq);
}
