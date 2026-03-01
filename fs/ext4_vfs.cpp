/*
 * keonOS - fs/ext4_vfs.cpp
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

#include <fs/ext4_vfs.h>
#include <drivers/ata.h>
#include <mm/heap.h>
#include <kernel/panic.h>
#include <stdio.h>
#include <string.h>

Ext4Manager ext4_inst;

void Ext4Manager::init(uint32_t lba) 
{
    partition_lba = lba;
    
    // Read Superblock (located at 1024 bytes offset from partition start)
    // If block size is 1024, it's block 1. If > 1024, it's inside block 0.
    uint8_t buffer[4096];
    ATADriver::read_sectors(partition_lba + 2, 2, buffer); // Read 1024 bytes (2 sectors)
    
    memcpy(&sb, buffer, sizeof(Ext4Superblock));
    
    if (sb.s_magic != EXT4_SUPER_MAGIC) 
        panic(KernelError::K_ERR_GENERAL_PROTECTION, "Invalid EXT4 magic signature");
    
    block_size = 1024 << sb.s_log_block_size;
    inodes_per_group = sb.s_inodes_per_group;
    blocks_per_group = sb.s_blocks_per_group;
    
    if (sb.s_rev_level > 0) inode_size = sb.s_inode_size;
    else inode_size = 128; // Standard ext2 inode size
    
    // Calculate number of block groups
    uint64_t total_blocks = ext4_get_blocks_count(&sb);
    groups_count = (total_blocks + blocks_per_group - 1) / blocks_per_group;
    
    // Determine group descriptor size (64-bit feature check)
    if (check_feature_incompat(EXT4_FEATURE_INCOMPAT_64BIT)) group_desc_size = sb.s_desc_size;
    else group_desc_size = 32;
    
    printf("[EXT4] Init: Block Size=%d, Inode Size=%d, Groups=%d\n", 
           block_size, inode_size, groups_count);
           
    if (check_feature_incompat(EXT4_FEATURE_INCOMPAT_EXTENTS)) 
        printf("[EXT4] Feature: Extents enabled\n");
    
    // Init Journal (if present)
    if (sb.s_journal_inum != 0) 
    {
        journal.init(this, sb.s_journal_inum);
        journal.start_transaction(); // Start first transaction
    }
}

uint32_t Ext4Manager::find_ext4_partition() 
{
    uint8_t sector[512];
    ATADriver::read_sectors(0, 1, sector);

    // Check MBR partitions
    struct PartitionEntry 
    {
        uint8_t status;
        uint8_t chs_start[3];
        uint8_t type;
        uint8_t chs_end[3];
        uint32_t lba_start;
        uint32_t sectors_count;
    } __attribute__((packed));
    
    PartitionEntry* partitions = (PartitionEntry*)(sector + 446);
    
    for (int i = 0; i < 4; i++) 
    {
        // Linux partition type is usually 0x83
        if (partitions[i].type == 0x83) 
        {
            // Verify ext4 signature
            uint8_t buffer[1024];
            // Superblock is at byte 1024 of partition
            ATADriver::read_sectors(partitions[i].lba_start + 2, 2, buffer);
            Ext4Superblock* potential_sb = (Ext4Superblock*)buffer;
            
            if (potential_sb->s_magic == EXT4_SUPER_MAGIC) {
                printf("[EXT4] Found partition at LBA %d\n", partitions[i].lba_start);
                return partitions[i].lba_start;
            }
        }
    }
    return 0;
}


void Ext4Manager::read_block(uint64_t block_num, uint8_t* buffer) 
{
    uint32_t sectors_per_block = block_size / 512;
    uint64_t lba = partition_lba + (block_num * sectors_per_block);
    ATADriver::read_sectors(lba, sectors_per_block, buffer);
}

void Ext4Manager::write_block(uint64_t block_num, uint8_t* buffer) 
{
    uint32_t sectors_per_block = block_size / 512;
    uint64_t lba = partition_lba + (block_num * sectors_per_block);
    ATADriver::write_sectors(lba, sectors_per_block, buffer);
}

void Ext4Manager::read_blocks(uint64_t block_num, uint32_t count, uint8_t* buffer)
{
    uint32_t sectors_per_block = block_size / 512;
    uint64_t lba = partition_lba + (block_num * sectors_per_block);
    ATADriver::read_sectors(lba, count * sectors_per_block, buffer);
}


void Ext4Manager::read_group_desc(uint32_t group, Ext4GroupDesc* desc)
{
    // Group descriptors are located after the superblock
    // Superblock is usually 1 block (if block size > 1024) or 2 blocks (if 1024)
    // Actually, it's always block "s_first_data_block + 1"
    
    uint32_t first_desc_block = sb.s_first_data_block + 1;
    uint32_t desc_per_block = block_size / group_desc_size;
    
    uint32_t block_offset = group / desc_per_block;
    uint32_t offset_in_block = (group % desc_per_block) * group_desc_size;
    
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    read_block(first_desc_block + block_offset, buffer);
    
    memcpy(desc, buffer + offset_in_block, sizeof(Ext4GroupDesc)); // Copy base size
    
    // Handle 64-bit part if needed
    if (group_desc_size > 32) {}
    kfree(buffer);
}

uint64_t Ext4Manager::get_block_bitmap(Ext4GroupDesc* desc) 
{
    uint64_t block = desc->bg_block_bitmap_lo;
    if (check_feature_incompat(EXT4_FEATURE_INCOMPAT_64BIT)) 
        block |= ((uint64_t)desc->bg_block_bitmap_hi << 32);
    return block;
}

uint64_t Ext4Manager::get_inode_bitmap(Ext4GroupDesc* desc) 
{
    uint64_t block = desc->bg_inode_bitmap_lo;
    if (check_feature_incompat(EXT4_FEATURE_INCOMPAT_64BIT)) 
        block |= ((uint64_t)desc->bg_inode_bitmap_hi << 32);
    return block;
}

uint64_t Ext4Manager::get_inode_table(Ext4GroupDesc* desc) 
{
    uint64_t block = desc->bg_inode_table_lo;
    if (check_feature_incompat(EXT4_FEATURE_INCOMPAT_64BIT)) 
        block |= ((uint64_t)desc->bg_inode_table_hi << 32);
    return block;
}


uint32_t Ext4Manager::get_inode_group(uint32_t inode_num) 
{
    return (inode_num - 1) / inodes_per_group;
}

uint32_t Ext4Manager::get_inode_index(uint32_t inode_num) 
{
    return (inode_num - 1) % inodes_per_group;
}

void Ext4Manager::read_inode(uint32_t inode_num, Ext4Inode* inode)
{
    uint32_t group = get_inode_group(inode_num);
    uint32_t index = get_inode_index(inode_num);
    
    Ext4GroupDesc gd;
    read_group_desc(group, &gd);
    
    uint64_t table_block = get_inode_table(&gd);
    uint32_t inodes_per_block = block_size / inode_size;
    
    uint32_t block_offset = index / inodes_per_block;
    uint32_t offset_in_block = (index % inodes_per_block) * inode_size;
    
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    read_block(table_block + block_offset, buffer);
    
    memcpy(inode, buffer + offset_in_block, sizeof(Ext4Inode));
    
    kfree(buffer);
}

void Ext4Manager::write_inode(uint32_t inode_num, Ext4Inode* inode)
{
    uint32_t group = get_inode_group(inode_num);
    uint32_t index = get_inode_index(inode_num);
    
    Ext4GroupDesc gd;
    read_group_desc(group, &gd);
    
    uint64_t table_block = get_inode_table(&gd);
    uint32_t inodes_per_block = block_size / inode_size;
    
    uint32_t block_offset = index / inodes_per_block;
    uint32_t offset_in_block = (index % inodes_per_block) * inode_size;
    
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    read_block(table_block + block_offset, buffer);
    
    memcpy(buffer + offset_in_block, inode, sizeof(Ext4Inode));
    // Direct write to ensure visibility (bypass journal for debug/latency fix)
    write_block(table_block + block_offset, buffer);
    
    kfree(buffer);
}

bool Ext4Manager::extent_allocate_blocks(Ext4Inode* inode, uint32_t logical_start, uint32_t count)
{
    // This is a simplified "allocator" that only supports:
    // 1. Appending to the last extent if contiguous
    // 2. Creating a new extent in the root of the tree (if space exists)
    // Does NOT support: Splitting index nodes, growing the tree depth.

    if (!(inode->i_flags & EXT4_EXTENTS_FL)) return false;

    Ext4ExtentHeader* header = (Ext4ExtentHeader*)inode->i_block;
    
    // We only support depth 0 for now (direct extents in inode)
    if (header->eh_depth != 0) 
    {
        printf("[EXT4] Error: extent_allocate_blocks only supports depth 0 (TODO)\n");
        return false;
    }

    Ext4Extent* extents = (Ext4Extent*)(header + 1);
    
    // Check if we can append to the last extent
    if (header->eh_entries > 0) 
    {
        Ext4Extent* last_extent = &extents[header->eh_entries - 1];
        
        // Logical contiguity?
        if (logical_start == last_extent->ee_block + last_extent->ee_len) 
        {
            // Physical contiguity? We need to try to allocate the next physical block.
            uint64_t last_phys = ext4_get_extent_start(last_extent);
            uint64_t next_phys = last_phys + last_extent->ee_len;
            
            // Check if next_phys is free (simple check, assume we want 1 block)
            // Ideally we should ask the allocator for *specific* block
            // For now, let's just allocate *any* block and see if it happens to be contiguous
            // OR simpler: Just create a new extent if not contiguous.
            // Let's trying to allocate ANY block first.
            
            uint64_t new_block = allocate_block(); 
            if (new_block == 0) return false;
            
            if (new_block == next_phys && last_extent->ee_len < 32768) 
            {
                // Merge!
                last_extent->ee_len++;
                return true;
            }
            else 
            {
                // Cannot merge, need new extent.
                // But wait, we allocated 'new_block', we must use it for the new extent.
                // Fallthrough to add new extent logic using 'new_block'.
                
                if (header->eh_entries >= header->eh_max) 
                {
                    printf("[EXT4] Error: Inode extent list full (depth 0)\n");
                    free_block(new_block);
                    return false;
                }
                
                Ext4Extent* new_ext = &extents[header->eh_entries];
                new_ext->ee_block = logical_start;
                new_ext->ee_len = count; // Assume count=1 for this logic
                new_ext->ee_start_lo = new_block & 0xFFFFFFFF;
                new_ext->ee_start_hi = (new_block >> 32) & 0xFFFF;
                
                header->eh_entries++;
                return true;
            }
        }
    }

    // New extent required
    if (header->eh_entries >= header->eh_max) 
    {
        printf("[EXT4] Error: Inode extent list full (depth 0)\n");
        return false;
    }

    uint64_t new_block = allocate_block();
    if (new_block == 0) return false;

    Ext4Extent* new_ext = &extents[header->eh_entries];
    new_ext->ee_block = logical_start;
    new_ext->ee_len = count;
    new_ext->ee_start_lo = new_block & 0xFFFFFFFF;
    new_ext->ee_start_hi = (new_block >> 32) & 0xFFFF;

    header->eh_entries++;
    return true;
}


uint64_t Ext4Manager::extent_get_block(Ext4Inode* inode, uint32_t logical_block)
{
    // Check if inode uses extents
    if (!(inode->i_flags & EXT4_EXTENTS_FL)) 
    {
        // Fallback or TODO: Implement indirect blocks support
        printf("[EXT4] Error: Inode %d does not use extents (not supported yet)\n", 0);
        return 0;
    }

    // Root of extent tree is in i_block array
    Ext4ExtentHeader* header = (Ext4ExtentHeader*)inode->i_block;
    
    if (header->eh_magic != EXT4_EXTENT_MAGIC) 
    {
        printf("[EXT4] Error: Invalid extent header magic\n");
        return 0;
    }
    
    // We need to traverse the tree. While depth > 0, we are at index nodes.
    // When depth == 0, we are at leaf nodes.
    
    uint8_t* current_block_data = nullptr; // To track if we alloc'd buffer
    Ext4ExtentHeader* current_header = header;
    
    uint16_t depth = current_header->eh_depth;
    
    while (true)
    {
        if (depth == 0) 
        {
            // Leaf node: search for extent covering logical_block
            Ext4Extent* extents = (Ext4Extent*)(current_header + 1);
            
            for (uint16_t i = 0; i < current_header->eh_entries; i++) 
            {
                if (logical_block >= extents[i].ee_block && 
                    logical_block < extents[i].ee_block + extents[i].ee_len) 
                {
                    uint64_t physical_start = ext4_get_extent_start(&extents[i]);
                    uint64_t result = physical_start + (logical_block - extents[i].ee_block);
                    
                    if (current_block_data) kfree(current_block_data);
                    return result;
                }
            }
            break; // Not found
        } 
        else 
        {
            // Index node: search for index covering logical_block
            Ext4ExtentIdx* indices = (Ext4ExtentIdx*)(current_header + 1);
            bool found_index = false;
            
            for (uint16_t i = 0; i < current_header->eh_entries; i++) 
            {
                // Verify if this index covers the block
                // An index covers everything from its ei_block up to the next index's ei_block
                // or infinity if it's the last one.
                
                uint32_t next_start = 0xFFFFFFFF;
                if (i + 1 < current_header->eh_entries) 
                    next_start = indices[i+1].ei_block;
                
                if (logical_block >= indices[i].ei_block && logical_block < next_start) 
                {
                    // Found the correct branch
                    uint64_t next_block_phys = ((uint64_t)indices[i].ei_leaf_hi << 32) | indices[i].ei_leaf_lo;
                    
                    // Read the next block in the tree
                    if (current_block_data) kfree(current_block_data);
                    current_block_data = (uint8_t*)kmalloc(block_size);
                    read_block(next_block_phys, current_block_data);
                    
                    current_header = (Ext4ExtentHeader*)current_block_data;
                    
                    if (current_header->eh_magic != EXT4_EXTENT_MAGIC) 
                    {
                        printf("[EXT4] Corrupt extent tree block %llu\n", next_block_phys);
                        kfree(current_block_data);
                        return 0;
                    }
                    
                    depth = current_header->eh_depth;
                    found_index = true;
                    break;
                }
            }
            if (!found_index) break;
        }
    }

    if (current_block_data) kfree(current_block_data);
    return 0;
}


bool Ext4Manager::check_feature_incompat(uint32_t feature) 
{
    return (sb.s_feature_incompat & feature) != 0;
}


Ext4File::Ext4File(const char* n, uint32_t ino, Ext4Superblock* s) 
{
    strncpy(this->name, n, 127);
    this->inode_num = ino;
    this->sb = s;
    this->type = VFS_FILE;
    ext4_inst.read_inode(ino, &this->inode);
    this->size = ext4_get_inode_size(&this->inode);
}

void Ext4File::open() { ref_count++; }
void Ext4File::close() 
{
    if (ref_count > 0) ref_count--; 
    if (ref_count == 0) delete this;
}

uint32_t Ext4File::read(uint32_t offset, uint32_t size, uint8_t* buffer) 
{
    // Permission check
    if (!check_permission(EXT4_S_IRUSR)) return 0;

    if (offset >= this->size) return 0;
    if (offset + size > this->size) size = this->size - offset;

    uint32_t bytes_read = 0;
    uint32_t block_size = ext4_inst.block_size;
    
    while (bytes_read < size) 
    {
        uint32_t current_offset = offset + bytes_read;
        uint32_t logical_block = current_offset / block_size;
        uint32_t offset_in_block = current_offset % block_size;
        
        uint64_t physical_block = ext4_inst.extent_get_block(&this->inode, logical_block);
        
        uint32_t to_read = block_size - offset_in_block;
        if (to_read > (size - bytes_read)) to_read = size - bytes_read;
        
        if (physical_block == 0)
            memset(buffer + bytes_read, 0, to_read); // Sparse file hole - return zeros
        else 
        {
            // Read actual data
            uint8_t* temp_buf = (uint8_t*)kmalloc(block_size);
            ext4_inst.read_block(physical_block, temp_buf);
            memcpy(buffer + bytes_read, temp_buf + offset_in_block, to_read);
            kfree(temp_buf);
        }
        
        bytes_read += to_read;
    }
    
    return bytes_read;
}

uint32_t Ext4File::write(uint32_t offset, uint32_t size, uint8_t* buffer) 
{
    if (!check_permission(EXT4_S_IWUSR)) return 0;

    // TODO: Support offset > size (sparse files)
    // For now we assume sequential write or overwrite
    
    uint32_t bytes_written = 0;
    uint32_t block_size = ext4_inst.block_size;

    while (bytes_written < size)
    {
        uint32_t current_offset = offset + bytes_written;
        uint32_t logical_block = current_offset / block_size;
        uint32_t offset_in_block = current_offset % block_size;
        
        uint64_t physical_block = ext4_inst.extent_get_block(&this->inode, logical_block);
        
        if (physical_block == 0) 
        {
            // Allocate new block
            if (!ext4_inst.extent_allocate_blocks(&this->inode, logical_block, 1)) 
            {
                printf("[EXT4] Error: Failed to allocate block for inode %d\n", inode_num);
                break;
            }
            physical_block = ext4_inst.extent_get_block(&this->inode, logical_block);
            if (physical_block == 0) break; // Should not happen
        }

        uint32_t to_write = block_size - offset_in_block;
        if (to_write > (size - bytes_written)) to_write = size - bytes_written;

        // Read-Modify-Write if partial block
        uint8_t* temp_buf = (uint8_t*)kmalloc(block_size);
        
        if (to_write < block_size) 
            ext4_inst.read_block(physical_block, temp_buf);
        
        memcpy(temp_buf + offset_in_block, buffer + bytes_written, to_write);
        ext4_inst.journal_write_block(physical_block, temp_buf);
        kfree(temp_buf);
        
        bytes_written += to_write;
    }
    
    if (offset + bytes_written > this->size) 
    {
        this->size = offset + bytes_written;
        this->inode.i_size_lo = this->size;
        ext4_inst.write_inode(inode_num, &this->inode);
    }
    else 
    {
        // Just update inode if extents changed (block allocation optimization)
        // But we always write inode if we allocated blocks...
        ext4_inst.write_inode(inode_num, &this->inode); 
    }

    return bytes_written;
}


Ext4Directory::Ext4Directory(const char* n, uint32_t ino, Ext4Superblock* s) 
{
    strncpy(this->name, n, 127);
    this->inode_num = ino;
    this->sb = s;
    this->type = VFS_DIRECTORY;
    ext4_inst.read_inode(ino, &this->inode);
    this->size = ext4_get_inode_size(&this->inode);
}

void Ext4Directory::open() { ref_count++; }
void Ext4Directory::close() 
{ 
    if (ref_count > 0) ref_count--;
    if (ref_count == 0) delete this;
}

uint32_t Ext4Directory::read(uint32_t, uint32_t, uint8_t*) { return 0; }

vfs_dirent* Ext4Directory::readdir(uint32_t index) 
{
    // Implementation of linear directory reading
    // TODO: Support HTree (Hash Tree) directories for large folders
    
    static vfs_dirent vdirent;
    uint32_t current_idx = 0;
    uint32_t offset = 0;
    uint32_t block_size = ext4_inst.block_size;
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    
    while (offset < this->size) 
    {
        uint32_t logical_block = offset / block_size;
        uint32_t offset_in_block = offset % block_size;
        
        uint64_t physical_block = ext4_inst.extent_get_block(&this->inode, logical_block);
        
        if (physical_block == 0) 
        {
            offset += block_size - offset_in_block; // Skip hole
            continue;
        }
        
        ext4_inst.read_block(physical_block, buffer);
        
        // Iterate entries in this block
        uint8_t* ptr = buffer + offset_in_block;
        uint8_t* end = buffer + block_size;
        
        while (ptr < end && offset < this->size) 
        {
            Ext4DirEntry2* entry = (Ext4DirEntry2*)ptr;
            
            if (entry->inode != 0) 
            {
                if (current_idx == index) 
                {
                    memset(&vdirent, 0, sizeof(vfs_dirent));
                    memcpy(vdirent.name, entry->name, entry->name_len);
                    vdirent.name[entry->name_len] = '\0';
                    vdirent.inode = entry->inode;
                    
                    // Map ext4 file type to VFS type
                    if (entry->file_type == EXT4_FT_DIR) vdirent.type = VFS_DIRECTORY;
                    else vdirent.type = VFS_FILE;
                    
                    kfree(buffer);
                    return &vdirent;
                }
                current_idx++;
            }
            
            offset += entry->rec_len;
            ptr += entry->rec_len;
            
            // Safety check for infinite loop
            if (entry->rec_len == 0) 
            {
                offset += block_size; // Skip corrupted entry
                break;
            }
        }
    }
    
    kfree(buffer);
    return nullptr;
}

VFSNode* Ext4Directory::finddir(const char* name) 
{
    uint32_t offset = 0;
    uint32_t block_size = ext4_inst.block_size;
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    
    while (offset < this->size) 
    {
        uint32_t logical_block = offset / block_size;
        uint64_t physical_block = ext4_inst.extent_get_block(&this->inode, logical_block);
        
        if (physical_block == 0) 
        {
             offset += block_size;
             continue;
        }
        
        ext4_inst.read_block(physical_block, buffer);
        
        uint8_t* ptr = buffer;
        uint8_t* end = buffer + block_size;
        
        while (ptr < end) 
        {
            Ext4DirEntry2* entry = (Ext4DirEntry2*)ptr;
            
            if (entry->inode != 0) 
            {
                // Verify name length and content
                if (strlen(name) == entry->name_len && 
                    strncmp(name, entry->name, entry->name_len) == 0) 
                {
                    uint32_t found_inode = entry->inode;
                    Ext4Inode inode_data;
                    ext4_inst.read_inode(found_inode, &inode_data);
                    
                    VFSNode* result = nullptr;
                    if ((inode_data.i_mode & 0xF000) == EXT4_S_IFDIR) result = new Ext4Directory(name, found_inode, &ext4_inst.sb);
                    else result = new Ext4File(name, found_inode, &ext4_inst.sb);
                    
                    kfree(buffer);
                    return result;
                }
            }
            
            if (entry->rec_len == 0) break;
            ptr += entry->rec_len;
            offset += entry->rec_len;
        }
    }
    
    kfree(buffer);
    return nullptr;
}

int Ext4Directory::mkdir(const char* name, [[maybe_unused]] uint32_t mode)
{
    if (!check_permission(EXT4_S_IWUSR | EXT4_S_IXUSR)) return -1;

    // 1. Allocate Inode
    uint32_t new_inode_num = ext4_inst.allocate_inode(true);
    if (!new_inode_num) return -1;

    // 2. Allocate Data Block for '.' and '..'
    uint64_t block = ext4_inst.allocate_block();
    if (!block) return -1;

    // 3. Init Inode
    Ext4Inode new_inode;
    memset(&new_inode, 0, sizeof(Ext4Inode));
    new_inode.i_mode = EXT4_S_IFDIR | 0755;
    new_inode.i_links_count = 2; // . and ..
    new_inode.i_size_lo = ext4_inst.block_size;
    new_inode.i_flags = EXT4_EXTENTS_FL;
    new_inode.i_uid = 0;
    new_inode.i_gid = 0;
    
    // Init extent header pointing to 'block'
    Ext4ExtentHeader* eh = (Ext4ExtentHeader*)new_inode.i_block;
    eh->eh_magic = EXT4_EXTENT_MAGIC;
    eh->eh_entries = 1;
    eh->eh_max = 4;
    eh->eh_depth = 0;
    
    Ext4Extent* ee = (Ext4Extent*)(eh + 1);
    ee->ee_block = 0;
    ee->ee_len = 1;
    ee->ee_start_lo = block & 0xFFFFFFFF;
    ee->ee_start_hi = (block >> 32) & 0xFFFF;
    
    ext4_inst.write_inode(new_inode_num, &new_inode);

    // 4. Init Directory Block with . and ..
    uint32_t block_size = ext4_inst.block_size;
    uint8_t* buf = (uint8_t*)kmalloc(block_size);
    memset(buf, 0, block_size);
    
    // Entry 1: "."
    Ext4DirEntry2* dot = (Ext4DirEntry2*)buf;
    dot->inode = new_inode_num;
    dot->name_len = 1;
    dot->rec_len = 12; // 8+1 aligned to 4
    dot->file_type = EXT4_FT_DIR;
    dot->name[0] = '.';
    
    // Entry 2: ".."
    Ext4DirEntry2* dotdot = (Ext4DirEntry2*)((uint8_t*)dot + dot->rec_len);
    dotdot->inode = this->inode_num; // Parent inode
    dotdot->name_len = 2;
    dotdot->rec_len = block_size - dot->rec_len; // Rest of block
    dotdot->file_type = EXT4_FT_DIR;
    dotdot->name[0] = '.'; dotdot->name[1] = '.';
    
    // Write directory block directly (Data=Ordered: Data updates before Metadata commit)
    ext4_inst.write_block(block, buf);
    kfree(buf);

    // 5. Add to Parent Directory (Duplicate logic from create)
    uint8_t name_len = strlen(name);

    uint32_t entry_len = 8 + name_len;
    if (entry_len % 4 != 0) entry_len += (4 - (entry_len % 4));
    
    uint64_t phys_block = 0;
    uint32_t entry_block_offset = 0;
    uint32_t space_found = find_free_entry_space(entry_len, &phys_block, &entry_block_offset);
    
    if (space_found != 0) 
    {
        // Read block again
        uint8_t* pbuf = (uint8_t*)kmalloc(block_size);
        ext4_inst.read_block(phys_block, pbuf);
        
        Ext4DirEntry2* existing_entry = (Ext4DirEntry2*)(pbuf + entry_block_offset);
        uint16_t old_actual_len = 8 + existing_entry->name_len;
        if (old_actual_len % 4 != 0) old_actual_len += (4 - (old_actual_len % 4));
        uint16_t original_rec_len = existing_entry->rec_len;
        
        existing_entry->rec_len = old_actual_len;
        
        Ext4DirEntry2* new_entry = (Ext4DirEntry2*)((uint8_t*)existing_entry + old_actual_len);
        new_entry->inode = new_inode_num;
        new_entry->rec_len = original_rec_len - old_actual_len;
        new_entry->name_len = name_len;
        new_entry->file_type = EXT4_FT_DIR;
        memcpy(new_entry->name, name, name_len);
        
        ext4_inst.journal_write_block(phys_block, pbuf);
        kfree(pbuf);
    }
    
    // 6. Update Parent Inode (links count++)
    this->inode.i_links_count++;
    ext4_inst.write_inode(this->inode_num, &this->inode);
    
    // Commit
    if (ext4_inst.sb.s_journal_inum != 0) 
    {
        ext4_inst.journal.commit_transaction();
        ext4_inst.journal.start_transaction();
    }

    printf("[EXT4] Created directory '%s' (inode %d)\n", name, new_inode_num);
    return 0;
}

bool Ext4Directory::unlink(const char* name)
{
    if (!check_permission(EXT4_S_IWUSR | EXT4_S_IXUSR)) return false;
    
    // Find entry logic (needs block ptr to write back)
    uint32_t offset = 0;
    uint32_t block_size = ext4_inst.block_size;
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    
    bool found = false;
    uint32_t found_inode_num = 0;
    
    while (offset < this->size) 
    {
        uint32_t logical_block = offset / block_size;
        uint64_t physical_block = ext4_inst.extent_get_block(&this->inode, logical_block);
        
        if (physical_block == 0) 
        {
            offset += block_size;
            continue;
        }
        
        ext4_inst.read_block(physical_block, buffer);
        
        uint32_t block_offset = 0;
        Ext4DirEntry2* prev = nullptr;
        
        while (block_offset < block_size) 
        {
            Ext4DirEntry2* entry = (Ext4DirEntry2*)(buffer + block_offset);
            
            // Check for valid entry and match name
            if (entry->inode != 0 && strlen(name) == entry->name_len && strncmp(name, entry->name, entry->name_len) == 0) 
            {
                // Found it!
                found = true;
                found_inode_num = entry->inode;
                
                // Remove entry
                if (prev) 
                {
                    // Merge with previous
                    prev->rec_len += entry->rec_len;
                } 
                else 
                {
                    // First entry: mark as unused logic (inode=0)
                    // Note: If we just set inode=0, the space is lost until compact (or reused if exact match)
                    // This is simple "soft" delete
                    entry->inode = 0;
                }
                
                ext4_inst.journal_write_block(physical_block, buffer);
                goto cleanup; // Exit loops
            }
            
            prev = entry;
            if (entry->rec_len == 0) break; // corrupt
            block_offset += entry->rec_len;
        }
        offset += block_size;
    }

cleanup:
    kfree(buffer);
    
    if (!found) return false;
    
    // Decrement links count of target
    Ext4Inode target_inode;
    ext4_inst.read_inode(found_inode_num, &target_inode);
    
    if (target_inode.i_links_count > 0) 
    {
        target_inode.i_links_count--;
        ext4_inst.write_inode(found_inode_num, &target_inode);
    }
    
    // Check type for parent link update
    if ((target_inode.i_mode & 0xF000) == EXT4_S_IFDIR) 
    {
        this->inode.i_links_count--;
        ext4_inst.write_inode(this->inode_num, &this->inode);
    }
    
    // TODO: If links==0, call free_inode/free_blocks
    if (target_inode.i_links_count == 0) printf("[EXT4] Unlinked inode %d (fully deleted)\n", found_inode_num);
    else printf("[EXT4] Unlinked inode %d (links remaining: %d)\n", found_inode_num, target_inode.i_links_count);
    

    // Commit
    if (ext4_inst.sb.s_journal_inum != 0) 
    {
        ext4_inst.journal.commit_transaction();
        ext4_inst.journal.start_transaction();
    }
    
    return true;
}


bool Ext4Manager::test_block_bitmap(uint32_t group, uint32_t block_in_group)
{
    Ext4GroupDesc gd;
    read_group_desc(group, &gd);
    uint64_t bitmap_block = get_block_bitmap(&gd);
    
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    read_block(bitmap_block, buffer);
    
    uint32_t byte_idx = block_in_group / 8;
    uint32_t bit_idx = block_in_group % 8;
    
    bool used = (buffer[byte_idx] & (1 << bit_idx)) != 0;
    
    kfree(buffer);
    return used;
}

void Ext4Manager::set_block_bitmap(uint32_t group, uint32_t block_in_group, bool value)
{
    Ext4GroupDesc gd;
    read_group_desc(group, &gd);
    uint64_t bitmap_block = get_block_bitmap(&gd);
    
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    read_block(bitmap_block, buffer);
    
    uint32_t byte_idx = block_in_group / 8;
    uint32_t bit_idx = block_in_group % 8;
    
    if (value)
        buffer[byte_idx] |= (1 << bit_idx);
    else
        buffer[byte_idx] &= ~(1 << bit_idx);
        
    journal_write_block(bitmap_block, buffer);
    kfree(buffer);
}

// Same for inodes... omitting to save space, logic is identical but using get_inode_bitmap
// Implementing generic bitmap helper would be better but explicit is fine.

bool Ext4Manager::test_inode_bitmap(uint32_t group, uint32_t inode_in_group)
{
    Ext4GroupDesc gd;
    read_group_desc(group, &gd);
    uint64_t bitmap_block = get_inode_bitmap(&gd);
    
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    read_block(bitmap_block, buffer);
    
    uint32_t byte_idx = inode_in_group / 8;
    uint32_t bit_idx = inode_in_group % 8;
    
    bool used = (buffer[byte_idx] & (1 << bit_idx)) != 0;
    
    kfree(buffer);
    return used;
}

void Ext4Manager::set_inode_bitmap(uint32_t group, uint32_t inode_in_group, bool value)
{
    Ext4GroupDesc gd;
    read_group_desc(group, &gd);
    uint64_t bitmap_block = get_inode_bitmap(&gd);
    
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    read_block(bitmap_block, buffer);
    
    uint32_t byte_idx = inode_in_group / 8;
    uint32_t bit_idx = inode_in_group % 8;
    
    if (value)
        buffer[byte_idx] |= (1 << bit_idx);
    else
        buffer[byte_idx] &= ~(1 << bit_idx);
        
    write_block(bitmap_block, buffer);
    kfree(buffer);
}

uint64_t Ext4Manager::allocate_block()
{
    // Iterate groups looking for free space
    for (uint32_t g = 0; g < groups_count; g++) 
    {
        Ext4GroupDesc gd;
        read_group_desc(g, &gd);
        
        if (gd.bg_free_blocks_count_lo > 0) 
        {
            // Found a group with free blocks. Find the exact bit.
            uint64_t bitmap_block = get_block_bitmap(&gd);
            uint8_t* buffer = (uint8_t*)kmalloc(block_size);
            read_block(bitmap_block, buffer);
            
            for (uint32_t i = 0; i < blocks_per_group; i++) 
            {
                uint32_t byte_idx = i / 8;
                uint32_t bit_idx = i % 8;
                
                if (!(buffer[byte_idx] & (1 << bit_idx))) 
                {
                    // Found free bit
                    buffer[byte_idx] |= (1 << bit_idx);
                    journal_write_block(bitmap_block, buffer);
                    kfree(buffer);
                    
                    // Update descriptors
                    gd.bg_free_blocks_count_lo--;
                    // Re-calculate checksum if needed (omitted for now)
                    write_group_desc(g, &gd);
                    
                    // Update superblock
                    sb.s_free_blocks_count_lo--;
                    // write_superblock(); // TODO: Implement write_superblock logic
                    
                    uint64_t absolute_block = (uint64_t)g * blocks_per_group + i + sb.s_first_data_block;
                    return absolute_block;
                }
            }
            kfree(buffer);
        }
    }
    return 0; // No space left
}

uint32_t Ext4Manager::allocate_inode(bool is_directory)
{
    // Iterate groups looking for free inodes
    for (uint32_t g = 0; g < groups_count; g++) 
    {
        Ext4GroupDesc gd;
        read_group_desc(g, &gd);
        
        if (gd.bg_free_inodes_count_lo > 0) 
        {
            uint64_t bitmap_block = get_inode_bitmap(&gd);
            uint8_t* buffer = (uint8_t*)kmalloc(block_size);
            read_block(bitmap_block, buffer);
            
            for (uint32_t i = 0; i < inodes_per_group; i++) 
            {
                uint32_t byte_idx = i / 8;
                uint32_t bit_idx = i % 8;
                
                if (!(buffer[byte_idx] & (1 << bit_idx))) 
                {
                    // Found free inode
                    buffer[byte_idx] |= (1 << bit_idx);
                    journal_write_block(bitmap_block, buffer);
                    kfree(buffer);
                    
                    // Update descriptors
                    gd.bg_free_inodes_count_lo--;
                    if (is_directory) gd.bg_used_dirs_count_lo++;
                    write_group_desc(g, &gd);
                    
                    // Update superblock
                    sb.s_free_inodes_count--;
                    
                    uint32_t inode_num = g * inodes_per_group + i + 1;
                    return inode_num;
                }
            }
            kfree(buffer);
        }
    }
    return 0;
}

void Ext4Manager::free_block(uint64_t block)
{
    // Simple implementation: calculate group and bit, clear it.
    uint64_t block_in_fs = block - sb.s_first_data_block;
    uint32_t group = block_in_fs / blocks_per_group;
    uint32_t block_in_group = block_in_fs % blocks_per_group;
    
    set_block_bitmap(group, block_in_group, false);
    
    Ext4GroupDesc gd;
    read_group_desc(group, &gd);
    gd.bg_free_blocks_count_lo++;
    write_group_desc(group, &gd);
    
    sb.s_free_blocks_count_lo++;
    // write_superblock(); 
}

// Helper to write back group descriptor
void Ext4Manager::write_group_desc(uint32_t group, Ext4GroupDesc* desc)
{
    uint32_t first_desc_block = sb.s_first_data_block + 1;
    uint32_t desc_per_block = block_size / group_desc_size;
    
    uint32_t block_offset = group / desc_per_block;
    uint32_t offset_in_block = (group % desc_per_block) * group_desc_size;
    
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    read_block(first_desc_block + block_offset, buffer);
    
    memcpy(buffer + offset_in_block, desc, sizeof(Ext4GroupDesc));
    journal_write_block(first_desc_block + block_offset, buffer);
    
    kfree(buffer);
}



bool Ext4Directory::check_permission(uint16_t required_mode)
{
    // Simulate user identity (uid=0 is root)
    uint32_t current_uid = 0; 
    uint32_t current_gid = 0;
    
    // Root always has access
    if (current_uid == 0) return true;
    
    uint32_t inode_uid = ext4_get_inode_uid(&inode);
    uint32_t inode_gid = ext4_get_inode_gid(&inode);
    uint16_t mode = inode.i_mode;
    
    // Shift required_mode depending on who we are
    uint16_t check_mode = required_mode;
    
    if (current_uid == inode_uid) {}
        // We are owner, use mode as is (assuming required_mode is passed as USR bits like EXT4_S_IRUSR)
    else if (current_gid == inode_gid) check_mode >>= 3;
    else check_mode >>= 6;
    
    return (mode & check_mode) == check_mode;
}

uint32_t Ext4Directory::find_free_entry_space(uint32_t required_size, uint64_t* out_phys_block, uint32_t* out_offset)
{
    // Need space for: entry struct (8 bytes) + name + 1 byte (null/type) -> aligned to 4
    // Actually ext4 dir entry is: 8 bytes fixed + name_len.
    // required_size passed here should be the NEW entry size.
    
    uint32_t offset = 0;
    uint32_t block_size = ext4_inst.block_size;
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    
    while (offset < this->size) 
    {
        uint32_t logical_block = offset / block_size;
        uint64_t physical_block = ext4_inst.extent_get_block(&this->inode, logical_block);
        
        if (physical_block == 0) 
        {
            // Should allocate new block here if we hit a hole
            offset += block_size;
            continue;
        }
        
        ext4_inst.read_block(physical_block, buffer);
        
        uint32_t block_offset = 0;
        while (block_offset < block_size) 
        {
            Ext4DirEntry2* entry = (Ext4DirEntry2*)(buffer + block_offset);
            
            // Real size used by this entry
            // 8 bytes header + name_len
            uint16_t actual_used = 8 + entry->name_len;
            // Pad to 4 bytes
            if (actual_used % 4 != 0) actual_used += (4 - (actual_used % 4));
            
            // Valid rec_len?
            if (entry->rec_len == 0) break; // Corrupt
            
            // Check if we have enough spare space in this entry (it might cover free space)
            if (entry->rec_len >= actual_used + required_size) 
            {
                // Found space!
                *out_phys_block = physical_block;
                *out_offset = block_offset; // Offset within the block
                kfree(buffer);
                return entry->rec_len - actual_used; // Return available size
            }
            
            block_offset += entry->rec_len;
        }
        
        offset += block_size;
    }
    
    kfree(buffer);
    return 0; // No space found (should expand directory)
}

VFSNode* Ext4Directory::create(const char* name, [[maybe_unused]] uint32_t flags)
{
    if (!check_permission(EXT4_S_IWUSR)) return nullptr; // Check write perm
    
    // 1. Prepare new entry
    uint8_t name_len = strlen(name);
    uint32_t entry_len = 8 + name_len;
    if (entry_len % 4 != 0) entry_len += (4 - (entry_len % 4));
    
    // 2. Find space in directory
    uint64_t phys_block = 0;
    uint32_t entry_block_offset = 0;
    uint32_t space_found = find_free_entry_space(entry_len, &phys_block, &entry_block_offset);
    
    if (space_found == 0) 
    {
        printf("[EXT4] Error: Directory full (expanding not impld)\n");
        return nullptr;
    }
    
    // 3. Allocate Inode
    uint32_t new_inode_num = ext4_inst.allocate_inode(false); // is_dir = false
    if (new_inode_num == 0) return nullptr;
    
    // 4. Initialize New Inode
    Ext4Inode new_inode;
    memset(&new_inode, 0, sizeof(Ext4Inode));
    new_inode.i_mode = EXT4_S_IFREG | 0644;
    new_inode.i_links_count = 1;
    new_inode.i_uid = 0;
    new_inode.i_gid = 0;
    new_inode.i_size_lo = 0;
    new_inode.i_flags = EXT4_EXTENTS_FL;
    
    // Init extent header
    Ext4ExtentHeader* eh = (Ext4ExtentHeader*)new_inode.i_block;
    eh->eh_magic = EXT4_EXTENT_MAGIC;
    eh->eh_entries = 0;
    eh->eh_max = 4;
    eh->eh_depth = 0;
    eh->eh_generation = 0;
    
    ext4_inst.write_inode(new_inode_num, &new_inode);
    
    // 5. Write Directory Entry
    // We need to read the block again, modify the existing entry to shrink it, 
    // and append the new one.
    
    uint32_t block_size = ext4_inst.block_size;
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    ext4_inst.read_block(phys_block, buffer);
    
    Ext4DirEntry2* existing_entry = (Ext4DirEntry2*)(buffer + entry_block_offset);
    
    // Calculate actual used size of existing entry
    uint16_t old_actual_len = 8 + existing_entry->name_len;
    if (old_actual_len % 4 != 0) old_actual_len += (4 - (old_actual_len % 4));
    
    uint16_t original_rec_len = existing_entry->rec_len;
    
    // Shrink existing entry
    existing_entry->rec_len = old_actual_len;
    
    // Create new entry
    Ext4DirEntry2* new_entry = (Ext4DirEntry2*)((uint8_t*)existing_entry + old_actual_len);
    new_entry->inode = new_inode_num;
    new_entry->rec_len = original_rec_len - old_actual_len;
    new_entry->name_len = name_len;
    new_entry->file_type = EXT4_FT_REG_FILE;
    memcpy(new_entry->name, name, name_len);
    
    // Write back directory block
    ext4_inst.journal_write_block(phys_block, buffer);
    kfree(buffer);
    
    printf("[EXT4] Created file '%s' (inode %d)\n", name, new_inode_num);
    
    // Commit transaction (atomic create)
    if (ext4_inst.sb.s_journal_inum != 0) 
    {
        ext4_inst.journal.commit_transaction();
        ext4_inst.journal.start_transaction(); // Start new one for next ops
    }
    
    return new Ext4File(name, new_inode_num, sb);
}


// ============================================================================
// Ext4File Helper
// ============================================================================

bool Ext4File::check_permission(uint16_t required_mode)
{
    // Simulate user identity (uid=0 is root)
    uint32_t current_uid = 0; 
    uint32_t current_gid = 0;
    
    // Root always has access
    if (current_uid == 0) return true;
    
    uint32_t inode_uid = ext4_get_inode_uid(&inode);
    uint32_t inode_gid = ext4_get_inode_gid(&inode);
    uint16_t mode = inode.i_mode;
    
    uint16_t check_mode = required_mode;
    
    if (current_uid == inode_uid) {} // Owner
    else if (current_gid == inode_gid) check_mode >>= 3;
    else check_mode >>= 6;
    return (mode & check_mode) == check_mode;
}


// Future journaling implementation will go here.
// For now, we rely on forced-check on next reboot if dirty bit is set in SB.


void Ext4Manager::journal_write_block(uint64_t block_num, uint8_t* buffer)
{
    // If journal is active, log it. The commit will write to FS.
    if (sb.s_journal_inum != 0) journal.log_block(block_num, buffer);
    else write_block(block_num, buffer);
}
