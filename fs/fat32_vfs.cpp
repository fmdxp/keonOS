/*
 * keonOS - fs/fat32_vfs.cpp
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


#include <fs/fat32_vfs.h>
#include <drivers/ata.h>
#include <mm/heap.h>
#include <string.h>
#include <stdio.h>

FAT32Manager fat32_inst;

void FAT32Manager::init(uint32_t lba) 
{
    partition_lba = lba;
    uint8_t sector_buffer[512]; 
    ATADriver::read_sectors(lba, 1, sector_buffer);
    memcpy(&bpb, sector_buffer, sizeof(FAT32_BPB));

    char label[12];
    memcpy(label, bpb.volume_label, 11);
    label[11] = '\0';

    for (int i = 10; i >= 0; i--) 
    {
        if (label[i] == ' ') label[i] = '\0';
        else if (label[i] != '\0') break;
    }

    if (label[0] == '\0') strcpy(label, "NO NAME");

    printf("[FAT32] Volume Label: %s\n", label);
    printf("[FAT32] Root Cluster: %u\n", bpb.root_cluster);
}

uint32_t FAT32Manager::cluster_to_lba(uint32_t cluster) 
{
    uint32_t fat_size = bpb.table_size_32;
    uint32_t data_lba = partition_lba + bpb.reserved_sector_count + (bpb.table_count * fat_size);
    return data_lba + ((cluster - 2) * bpb.sectors_per_cluster);
}

uint32_t FAT32Manager::get_next_cluster(uint32_t current_cluster) 
{
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = partition_lba + bpb.reserved_sector_count + (fat_offset / 512);
    uint8_t buffer[512];
    ATADriver::read_sectors(fat_sector, 1, buffer);
    return *(uint32_t*)&buffer[fat_offset % 512] & 0x0FFFFFFF;
}

void FAT32Manager::set_next_cluster(uint32_t current_cluster, uint32_t next_cluster) 
{
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = partition_lba + bpb.reserved_sector_count + (fat_offset / 512);
    uint8_t buffer[512];
    ATADriver::read_sectors(fat_sector, 1, buffer);
    *(uint32_t*)&buffer[fat_offset % 512] = (next_cluster & 0x0FFFFFFF);
    ATADriver::write_sectors(fat_sector, 1, buffer);
}

uint32_t FAT32Manager::allocate_cluster()
{
    uint8_t buffer[512];
    for (uint32_t i = 0; i < bpb.table_size_32; i++) 
	{
        uint32_t sector_lba = partition_lba + bpb.reserved_sector_count + i;
        ATADriver::read_sectors(sector_lba, 1, buffer);
        uint32_t* entries = (uint32_t*)buffer;
        for (int j = 0; j < 128; j++) 
		{
            if ((entries[j] & 0x0FFFFFFF) == 0) 
			{
                entries[j] = 0x0FFFFFFF;
                ATADriver::write_sectors(sector_lba, 1, buffer);
                return (i * 128) + j;
            }
        }
    }
    return 0;
}

uint32_t FAT32Manager::find_fat32_partition() 
{
    uint8_t sector[512];
    ATADriver::read_sectors(0, 1, sector);

    MBR_Partition* partitions = (MBR_Partition*)(sector + 446);
    for (int i = 0; i < 4; i++) 
	{
        if (partitions[i].type == 0x0B || partitions[i].type == 0x0C) 
		{
            printf("[FAT32] Found partition at LBA %d\n", partitions[i].lba_start);
            return partitions[i].lba_start;
        }
    }
    return 0;
}


FAT32_File::FAT32_File(const char* n, uint32_t cluster, uint32_t sz, FAT32_BPB* b, uint32_t entry_lba, uint32_t entry_off) 
{
    strncpy(this->name, n, 127);
    this->first_cluster = cluster;
    this->size = sz;
    this->bpb = b;
    this->dir_entry_lba = entry_lba;
    this->dir_entry_offset = entry_off;
    this->type = VFS_FILE;
}

uint32_t FAT32_File::read(uint32_t offset, uint32_t size, uint8_t* buffer) 
{
    if (offset >= this->size) return 0;
    if (offset + size > this->size) size = this->size - offset;

    uint32_t cluster_size = bpb->sectors_per_cluster * 512;
    uint32_t current_cluster = first_cluster;
    
    for (uint32_t i = 0; i < offset / cluster_size; i++)
        current_cluster = fat32_inst.get_next_cluster(current_cluster);

    uint32_t bytes_read = 0;
    uint32_t internal_offset = offset % cluster_size;
    uint8_t* temp_buffer = (uint8_t*)kmalloc(cluster_size);

    while (bytes_read < size) 
	{
        ATADriver::read_sectors(fat32_inst.cluster_to_lba(current_cluster), bpb->sectors_per_cluster, temp_buffer);
        uint32_t to_copy = cluster_size - internal_offset;
        if (to_copy > (size - bytes_read)) to_copy = size - bytes_read;

        memcpy(buffer + bytes_read, temp_buffer + internal_offset, to_copy);
        bytes_read += to_copy;
        internal_offset = 0;

        if (bytes_read < size) 
		{
            current_cluster = fat32_inst.get_next_cluster(current_cluster);
            if (current_cluster >= 0x0FFFFFF8) break;
        }
    }
    kfree(temp_buffer);
    return bytes_read;
}

uint32_t FAT32_File::write(uint32_t offset, uint32_t size, uint8_t* buffer) 
{
    uint32_t cluster_size = bpb->sectors_per_cluster * 512;
    uint32_t bytes_written = 0;

    while (bytes_written < size) 
	{
        uint32_t current_offset = offset + bytes_written;
        uint32_t current_cluster = first_cluster;
        
        for (uint32_t i = 0; i < current_offset / cluster_size; i++) 
		{
            uint32_t next = fat32_inst.get_next_cluster(current_cluster);
            if (next >= 0x0FFFFFF8) 
			{
                next = fat32_inst.allocate_cluster();
                fat32_inst.set_next_cluster(current_cluster, next);
            }
            current_cluster = next;
        }

        uint32_t lba = fat32_inst.cluster_to_lba(current_cluster);
        uint32_t sector_in_cluster = (current_offset % cluster_size) / 512;
        uint32_t byte_in_sector = current_offset % 512;
        
        uint8_t sector_data[512];
        ATADriver::read_sectors(lba + sector_in_cluster, 1, sector_data);
        
        uint32_t to_write = 512 - byte_in_sector;
        if (to_write > (size - bytes_written)) to_write = size - bytes_written;

        memcpy(sector_data + byte_in_sector, buffer + bytes_written, to_write);
        ATADriver::write_sectors(lba + sector_in_cluster, 1, sector_data);

        bytes_written += to_write;
    }

    if (offset + size > this->size) 
	{
        this->size = offset + size;
        update_metadata();
    }
    return bytes_written;
}


FAT32_Directory::FAT32_Directory(const char* n, uint32_t c, FAT32_BPB* b) 
{
    strncpy(this->name, n, 127);
    this->cluster = c;
    this->bpb = b;
    this->type = VFS_DIRECTORY;
}


VFSNode* FAT32_Directory::finddir(const char* name) 
{
    uint32_t current_cluster = this->cluster;
    uint32_t cluster_size = bpb->sectors_per_cluster * 512;
    uint8_t* buffer = (uint8_t*)kmalloc(cluster_size);
    char lfn_name[256]; memset(lfn_name, 0, 256);

    while (current_cluster < 0x0FFFFFF8 && current_cluster != 0) 
	{
        uint32_t lba = fat32_inst.cluster_to_lba(current_cluster);
        ATADriver::read_sectors(lba, bpb->sectors_per_cluster, buffer);
        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)buffer;

        for (uint32_t i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++) 
		{
            if (entries[i].name[0] == 0x00)
            {
                kfree(buffer);
                return nullptr; 
            }

            if ((uint8_t)entries[i].name[0] == 0xE5) 
            {
                memset(lfn_name, 0, 256);
                continue;
            }

            if ((entries[i].attr & 0x08) && entries[i].attr != 0x0F) continue;

            if (entries[i].attr == 0x0F) 
			{
                FAT32_LFNEntry* lfn = (FAT32_LFNEntry*)&entries[i];
                int index = ((lfn->sequence & 0x3F) - 1) * 13;
                if (index >= 0 && index < 240) 
                {
                    for(int k=0; k<5; k++) if(lfn->name1[k] && lfn->name1[k] != 0xFFFF) lfn_name[index+k] = (char)lfn->name1[k];
                    for(int k=0; k<6; k++) if(lfn->name2[k] && lfn->name2[k] != 0xFFFF) lfn_name[index+5+k] = (char)lfn->name2[k];
                    for(int k=0; k<2; k++) if(lfn->name3[k] && lfn->name3[k] != 0xFFFF) lfn_name[index+11+k] = (char)lfn->name3[k];
                }
                continue;
            }

            if (entries[i].attr & 0x08) continue; 
            
            bool match = false;
            if (lfn_name[0] != 0) 
            {
                if (strcmp(lfn_name, name) == 0) match = true;
            }

            else
            {
                if (compare_fat_name(entries[i].name, name)) match = true;
            }

            memset(lfn_name, 0, 256);

            if (match) 
            {
                uint32_t f_cluster = ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
                uint32_t entry_lba = lba + ((i * sizeof(FAT32_DirectoryEntry)) / 512);
                uint32_t entry_off = (i * sizeof(FAT32_DirectoryEntry)) % 512;

                VFSNode* res = nullptr;
                if (entries[i].attr & 0x10) res = new FAT32_Directory(name, f_cluster, bpb);
                else res = new FAT32_File(name, f_cluster, entries[i].file_size, bpb, entry_lba, entry_off);
                if (res) res->parent = this;
                
                kfree(buffer);
                return res;
            }
        }
        current_cluster = fat32_inst.get_next_cluster(current_cluster);
    }

    kfree(buffer);
    return nullptr;
}

vfs_dirent* FAT32_Directory::readdir(uint32_t index) 
{
    uint32_t current_cluster = this->cluster;
    uint32_t cluster_size = bpb->sectors_per_cluster * 512;
    uint8_t* buffer = (uint8_t*)kmalloc(cluster_size);
    static vfs_dirent dirent;
    memset(&dirent, 0, sizeof(vfs_dirent));
    
    uint32_t logical_index = 0;
    char lfn_name[256]; 
    memset(lfn_name, 0, 256);

    while (current_cluster < 0x0FFFFFF8 && current_cluster != 0) 
    {
        ATADriver::read_sectors(fat32_inst.cluster_to_lba(current_cluster), bpb->sectors_per_cluster, buffer);
        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)buffer;

        for (uint32_t i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++) 
        {
            if (entries[i].name[0] == 0x00) { kfree(buffer); return nullptr; }
            if ((uint8_t)entries[i].name[0] == 0xE5) 
            {
                memset(lfn_name, 0, 256);
                continue;
            }

            if (entries[i].attr & 0x08) 
            {
                if (entries[i].attr != 0x0F) 
                {
                    memset(lfn_name, 0, 256);
                    continue;
                }
            }

            if (entries[i].attr == 0x0F) 
            {
                FAT32_LFNEntry* lfn = (FAT32_LFNEntry*)&entries[i];
                int lfn_idx = ((lfn->sequence & 0x3F) - 1) * 13;
                if (lfn_idx >= 0 && lfn_idx < 240) 
                {
                    for(int k=0; k<5; k++) if(lfn->name1[k]) lfn_name[lfn_idx+k] = (char)lfn->name1[k];
                    for(int k=0; k<6; k++) if(lfn->name2[k]) lfn_name[lfn_idx+5+k] = (char)lfn->name2[k];
                    for(int k=0; k<2; k++) if(lfn->name3[k]) lfn_name[lfn_idx+11+k] = (char)lfn->name3[k];
                }
                continue;
            }


            if (logical_index == index) 
            {
                if (lfn_name[0] != '\0') strncpy(dirent.name, lfn_name, 127);
                else 
                {
                    int p = 0;
                    for (int j = 0; j < 8; j++) 
                    {
                        if (entries[i].name[j] != ' ') dirent.name[p++] = entries[i].name[j];
                    }

                    if (!(entries[i].attr & 0x08) && entries[i].name[8] != ' ')
                    {
                        dirent.name[p++] = '.';
                        for (int j = 8; j < 11; j++) 
                        {
                            if (entries[i].name[j] != ' ') dirent.name[p++] = entries[i].name[j];
                        }
                    }

                    else if (entries[i].attr & 0x08)
                    {
                        for (int j = 8; j < 11; j++) 
                        {
                            if (entries[i].name[j] != ' ') dirent.name[p++] = entries[i].name[j];
                        }
                    }
                    dirent.name[p] = '\0';
                }

                dirent.inode = ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
                kfree(buffer);
                return &dirent;
            }
            
            logical_index++;
            memset(lfn_name, 0, 256);
        }
        current_cluster = fat32_inst.get_next_cluster(current_cluster);
    }
    kfree(buffer);
    return nullptr;
}


int FAT32_Directory::mkdir(const char* name, uint32_t mode) 
{
    uint32_t new_cluster = fat32_inst.allocate_cluster();
    if (!new_cluster) return -1;

    uint32_t cluster_size = bpb->sectors_per_cluster * 512;
    uint8_t* sector_data = (uint8_t*)kmalloc(cluster_size);
    memset(sector_data, 0, cluster_size);

    FAT32_DirectoryEntry* dots = (FAT32_DirectoryEntry*)sector_data;
    
    memset(dots[0].name, ' ', 11); memcpy(dots[0].name, ".", 1);
    dots[0].attr = 0x10;
    dots[0].cluster_low = new_cluster & 0xFFFF;
    dots[0].cluster_high = (new_cluster >> 16) & 0xFFFF;

    memset(dots[1].name, ' ', 11); memcpy(dots[1].name, "..", 2);
    dots[1].attr = 0x10;

    uint32_t parent_cluster_val = this->cluster;
    if (parent_cluster_val == bpb->root_cluster) parent_cluster_val = 0;

    dots[1].cluster_low = parent_cluster_val & 0xFFFF;
    dots[1].cluster_high = (parent_cluster_val >> 16) & 0xFFFF;

    ATADriver::write_sectors(fat32_inst.cluster_to_lba(new_cluster), bpb->sectors_per_cluster, sector_data);

    uint32_t target_lba, target_offset;
    if (find_free_entry_index(&target_lba, &target_offset) == 0xFFFFFFFF) 
    {
        kfree(sector_data);
        return -1;
    }

    uint8_t entry_buf[512];
    ATADriver::read_sectors(target_lba, 1, entry_buf);
    
    FAT32_DirectoryEntry* new_entry = (FAT32_DirectoryEntry*)(entry_buf + target_offset);
    memset(new_entry, 0, sizeof(FAT32_DirectoryEntry));
    memset(new_entry->name, ' ', 11);

    for (int i = 0; i < 8 && name[i] != '\0' && name[i] != '.'; i++) 
    {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        new_entry->name[i] = c;
    }

    new_entry->attr = 0x10;
    new_entry->cluster_low = new_cluster & 0xFFFF;
    new_entry->cluster_high = (new_cluster >> 16) & 0xFFFF;

    ATADriver::write_sectors(target_lba, 1, entry_buf);
    
    kfree(sector_data);
    return 0;
}

uint32_t FAT32_Directory::find_free_entry_index(uint32_t* out_lba, uint32_t* out_offset) 
{
    uint32_t current_cluster = this->cluster;
    uint32_t cluster_size = bpb->sectors_per_cluster * 512;
    uint8_t* buffer = (uint8_t*)kmalloc(cluster_size);

    while (current_cluster < 0x0FFFFFF8) 
	{
        uint32_t lba = fat32_inst.cluster_to_lba(current_cluster);
        ATADriver::read_sectors(lba, bpb->sectors_per_cluster, buffer);
        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)buffer;

        for (uint32_t i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++) 
		{
            if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) 
			{
                *out_lba = lba + ((i * sizeof(FAT32_DirectoryEntry)) / 512);
                *out_offset = (i * sizeof(FAT32_DirectoryEntry)) % 512;
                kfree(buffer);
                return i;
            }
        }
        
        uint32_t next = fat32_inst.get_next_cluster(current_cluster);
        if (next >= 0x0FFFFFF8) 
		{
            next = fat32_inst.allocate_cluster();
            fat32_inst.set_next_cluster(current_cluster, next);
        }
        current_cluster = next;
    }
    kfree(buffer);
    return 0xFFFFFFFF;
}

VFSNode* FAT32_Directory::create(const char* name, [[maybe_unused]] uint32_t flags) 
{
    uint32_t target_lba, target_offset;
    if (find_free_entry_index(&target_lba, &target_offset) == 0xFFFFFFFF) return nullptr;

    uint32_t first_cluster = fat32_inst.allocate_cluster();
    if (!first_cluster) return nullptr;

    uint8_t sector[512];
    ATADriver::read_sectors(target_lba, 1, sector);

    FAT32_DirectoryEntry* new_file = (FAT32_DirectoryEntry*)(sector + target_offset);
    memset(new_file, 0, sizeof(FAT32_DirectoryEntry));
    memset(new_file->name, ' ', 11);

    int i = 0;
    while (i < 8 && name[i] != '\0' && name[i] != '.') 
    {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        new_file->name[i] = c;
        i++;
    }

    const char* dot = strrchr(name, '.');
    if (dot) 
    {
        dot++;
        int ext_ptr = 0;
        while (ext_ptr < 3 && dot[ext_ptr] != '\0') 
        {
            char c = dot[ext_ptr];
            if (c >= 'a' && c <= 'z') c -= 32;
            new_file->name[8 + ext_ptr] = c;
            ext_ptr++;
        }
    }

    new_file->attr = 0x20;
    new_file->cluster_low = first_cluster & 0xFFFF;
    new_file->cluster_high = (first_cluster >> 16) & 0xFFFF;
    new_file->file_size = 0;

    ATADriver::write_sectors(target_lba, 1, sector);
    return new FAT32_File(name, first_cluster, 0, bpb, target_lba, target_offset);
}

void FAT32_File::update_metadata() 
{
    uint8_t sector[512];
    ATADriver::read_sectors(this->dir_entry_lba, 1, sector);
    
    FAT32_DirectoryEntry* entry = (FAT32_DirectoryEntry*)(sector + this->dir_entry_offset);
    entry->file_size = this->size;
    
    ATADriver::write_sectors(this->dir_entry_lba, 1, sector);
}

void FAT32Manager::free_cluster_chain(uint32_t cluster)
{
    if (cluster < 2) return;

    while (cluster < 0x0FFFFFF8 && cluster != 0) 
	{
        uint32_t next = get_next_cluster(cluster);
        
        uint32_t fat_offset = cluster * 4;
        uint32_t fat_sector = partition_lba + bpb.reserved_sector_count + (fat_offset / 512);
        uint8_t buffer[512];
        
        ATADriver::read_sectors(fat_sector, 1, buffer);
        *(uint32_t*)&buffer[fat_offset % 512] = 0x00000000;
        ATADriver::write_sectors(fat_sector, 1, buffer);
        
        cluster = next;
    }
}

bool FAT32_Directory::unlink(const char* name) 
{
    uint32_t current_cluster = this->cluster;
    uint32_t cluster_size = bpb->sectors_per_cluster * 512;
    uint8_t* buffer = (uint8_t*)kmalloc(cluster_size);

	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return false;

    while (current_cluster < 0x0FFFFFF8 && current_cluster != 0) 
    {
        uint32_t lba = fat32_inst.cluster_to_lba(current_cluster);
        ATADriver::read_sectors(lba, bpb->sectors_per_cluster, buffer);
        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)buffer;

        for (uint32_t i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++) 
        {
            if (entries[i].name[0] == 0x00) break;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;

            if (compare_fat_name(entries[i].name, name)) 
            {
                uint32_t f_cluster = ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
                fat32_inst.free_cluster_chain(f_cluster);

				int j = i - 1;
				while (j >= 0 && entries[j].attr == 0x0F) 
				{
					entries[j].name[0] = (char)0xE5;
					j--;
				}

                entries[i].name[0] = (char)0xE5;

                ATADriver::write_sectors(lba, bpb->sectors_per_cluster, buffer);

                kfree(buffer);
                return true;
            }
        }
        current_cluster = fat32_inst.get_next_cluster(current_cluster);
    }

    kfree(buffer);
    return false;
}

bool compare_fat_name(const char* fat_name, const char* search_name) 
{
    if (strcmp(search_name, ".") == 0) return memcmp(fat_name, ".          ", 11) == 0;
    if (strcmp(search_name, "..") == 0) return memcmp(fat_name, "..         ", 11) == 0;

    char name83[11];
    for(int i = 0; i < 11; i++) name83[i] = ' ';

    int i = 0, j = 0;

    
    while (search_name[i] != '.' && search_name[i] != '\0' && j < 8) 
    {
        char c = search_name[i++];
        if (c >= 'a' && c <= 'z') c -= 32; 
        name83[j++] = c;
    }

    if (search_name[i] == '.') 
    {
        i++;
        j = 8;
        while (search_name[i] != '\0' && j < 11) 
        {
            char c = search_name[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            name83[j++] = c;
        }
    }

    return memcmp(fat_name, name83, 11) == 0;
}