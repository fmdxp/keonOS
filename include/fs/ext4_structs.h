/*
 * keonOS - include/fs/ext4_structs.h
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

#ifndef EXT4_STRUCTS_H
#define EXT4_STRUCTS_H

#include <stdint.h>

// Magic numbers
#define EXT4_SUPER_MAGIC    0xEF53
#define EXT4_EXTENT_MAGIC   0xF30A

// Feature flags
#define EXT4_FEATURE_INCOMPAT_EXTENTS   0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT     0x0080
#define EXT4_FEATURE_INCOMPAT_FLEX_BG   0x0200

// Inode flags
#define EXT4_EXTENTS_FL     0x00080000  // Inode uses extents

// File types
#define EXT4_FT_UNKNOWN     0
#define EXT4_FT_REG_FILE    1
#define EXT4_FT_DIR         2
#define EXT4_FT_CHRDEV      3
#define EXT4_FT_BLKDEV      4
#define EXT4_FT_FIFO        5
#define EXT4_FT_SOCK        6
#define EXT4_FT_SYMLINK     7

// Inode modes
#define EXT4_S_IFSOCK   0xC000  // Socket
#define EXT4_S_IFLNK    0xA000  // Symbolic link
#define EXT4_S_IFREG    0x8000  // Regular file
#define EXT4_S_IFBLK    0x6000  // Block device
#define EXT4_S_IFDIR    0x4000  // Directory
#define EXT4_S_IFCHR    0x2000  // Character device
#define EXT4_S_IFIFO    0x1000  // FIFO

// Permission bits
#define EXT4_S_ISUID    0x0800  // Set UID
#define EXT4_S_ISGID    0x0400  // Set GID
#define EXT4_S_ISVTX    0x0200  // Sticky bit
#define EXT4_S_IRWXU    0x01C0  // User rwx
#define EXT4_S_IRUSR    0x0100  // User read
#define EXT4_S_IWUSR    0x0080  // User write
#define EXT4_S_IXUSR    0x0040  // User execute
#define EXT4_S_IRWXG    0x0038  // Group rwx
#define EXT4_S_IRGRP    0x0020  // Group read
#define EXT4_S_IWGRP    0x0010  // Group write
#define EXT4_S_IXGRP    0x0008  // Group execute
#define EXT4_S_IRWXO    0x0007  // Others rwx
#define EXT4_S_IROTH    0x0004  // Others read
#define EXT4_S_IWOTH    0x0002  // Others write
#define EXT4_S_IXOTH    0x0001  // Others execute

// Superblock structure (1024 bytes)
struct Ext4Superblock {
    uint32_t s_inodes_count;        // Total inodes
    uint32_t s_blocks_count_lo;     // Total blocks (low 32 bits)
    uint32_t s_r_blocks_count_lo;   // Reserved blocks
    uint32_t s_free_blocks_count_lo;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;    // First data block
    uint32_t s_log_block_size;      // Block size = 1024 << s_log_block_size
    uint32_t s_log_cluster_size;
    uint32_t s_blocks_per_group;    // Blocks per group
    uint32_t s_clusters_per_group;
    uint32_t s_inodes_per_group;    // Inodes per group
    uint32_t s_mtime;               // Mount time
    uint32_t s_wtime;               // Write time
    uint16_t s_mnt_count;           // Mount count
    uint16_t s_max_mnt_count;
    uint16_t s_magic;               // 0xEF53
    uint16_t s_state;               // Filesystem state
    uint16_t s_errors;              // Error handling
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;           // Last check time
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;           // Revision level
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    
    // EXT4_DYNAMIC_REV specific
    uint32_t s_first_ino;           // First non-reserved inode
    uint16_t s_inode_size;          // Inode size (typically 256)
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;      // Compatible features
    uint32_t s_feature_incompat;    // Incompatible features
    uint32_t s_feature_ro_compat;   // Read-only compatible features
    uint8_t  s_uuid[16];            // Filesystem UUID
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    
    // Performance hints
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_reserved_gdt_blocks;
    
    // Journaling support
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;        // Journal inode number
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_jnl_backup_type;
    uint16_t s_desc_size;           // Group descriptor size
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_mkfs_time;
    uint32_t s_jnl_blocks[17];
    
    // 64-bit support
    uint32_t s_blocks_count_hi;
    uint32_t s_r_blocks_count_hi;
    uint32_t s_free_blocks_count_hi;
    uint16_t s_min_extra_isize;
    uint16_t s_want_extra_isize;
    uint32_t s_flags;
    uint16_t s_raid_stride;
    uint16_t s_mmp_interval;
    uint64_t s_mmp_block;
    uint32_t s_raid_stripe_width;
    uint8_t  s_log_groups_per_flex;
    uint8_t  s_checksum_type;
    uint8_t  s_reserved_pad;
    uint8_t  s_reserved_pad2;
    uint64_t s_kbytes_written;
    
    uint32_t s_reserved[160];       // Padding to 1024 bytes
} __attribute__((packed));

// Block Group Descriptor (64 bytes for 64-bit ext4)
struct Ext4GroupDesc {
    uint32_t bg_block_bitmap_lo;        // Block bitmap block (low 32)
    uint32_t bg_inode_bitmap_lo;        // Inode bitmap block
    uint32_t bg_inode_table_lo;         // Inode table block
    uint16_t bg_free_blocks_count_lo;   // Free blocks count
    uint16_t bg_free_inodes_count_lo;   // Free inodes count
    uint16_t bg_used_dirs_count_lo;     // Directories count
    uint16_t bg_flags;                  // Flags
    uint32_t bg_exclude_bitmap_lo;      // Snapshot exclude bitmap
    uint16_t bg_block_bitmap_csum_lo;   // Block bitmap checksum
    uint16_t bg_inode_bitmap_csum_lo;   // Inode bitmap checksum
    uint16_t bg_itable_unused_lo;       // Unused inodes count
    uint16_t bg_checksum;               // Group checksum
    
    // 64-bit support (if descriptor size > 32)
    uint32_t bg_block_bitmap_hi;
    uint32_t bg_inode_bitmap_hi;
    uint32_t bg_inode_table_hi;
    uint16_t bg_free_blocks_count_hi;
    uint16_t bg_free_inodes_count_hi;
    uint16_t bg_used_dirs_count_hi;
    uint16_t bg_itable_unused_hi;
    uint32_t bg_exclude_bitmap_hi;
    uint16_t bg_block_bitmap_csum_hi;
    uint16_t bg_inode_bitmap_csum_hi;
    uint32_t bg_reserved;
} __attribute__((packed));

// Inode structure (256 bytes for modern ext4)
struct Ext4Inode {
    uint16_t i_mode;            // File mode (type + permissions)
    uint16_t i_uid;             // Owner UID (low 16 bits)
    uint32_t i_size_lo;         // Size in bytes (low 32 bits)
    uint32_t i_atime;           // Access time
    uint32_t i_ctime;           // Change time
    uint32_t i_mtime;           // Modification time
    uint32_t i_dtime;           // Deletion time
    uint16_t i_gid;             // Group GID (low 16 bits)
    uint16_t i_links_count;     // Hard links count
    uint32_t i_blocks_lo;       // Blocks count (low 32 bits)
    uint32_t i_flags;           // File flags
    uint32_t i_osd1;            // OS dependent
    uint32_t i_block[15];       // Block pointers / Extent tree root
    uint32_t i_generation;      // File version (for NFS)
    uint32_t i_file_acl_lo;     // Extended attributes block
    uint32_t i_size_high;       // Size high 32 bits / dir ACL
    uint32_t i_obso_faddr;      // Obsolete fragment address
    
    // OS dependent 2
    uint16_t i_blocks_high;     // Blocks count high 16 bits
    uint16_t i_file_acl_high;   // Extended attributes high 16 bits
    uint16_t i_uid_high;        // Owner UID high 16 bits
    uint16_t i_gid_high;        // Group GID high 16 bits
    uint16_t i_checksum_lo;     // Inode checksum low
    uint16_t i_reserved;
    
    // Extra inode fields (if inode size > 128)
    uint16_t i_extra_isize;     // Extra inode size
    uint16_t i_checksum_hi;     // Inode checksum high
    uint32_t i_ctime_extra;     // Extra change time
    uint32_t i_mtime_extra;     // Extra modification time
    uint32_t i_atime_extra;     // Extra access time
    uint32_t i_crtime;          // File creation time
    uint32_t i_crtime_extra;    // Extra file creation time
    uint32_t i_version_hi;      // Version high 32 bits
    uint32_t i_projid;          // Project ID
} __attribute__((packed));

// Extent header (12 bytes)
struct Ext4ExtentHeader {
    uint16_t eh_magic;          // 0xF30A
    uint16_t eh_entries;        // Number of valid entries
    uint16_t eh_max;            // Capacity of store
    uint16_t eh_depth;          // Tree depth (0 = leaf)
    uint32_t eh_generation;     // Generation
} __attribute__((packed));

// Extent (12 bytes) - leaf node entry
struct Ext4Extent {
    uint32_t ee_block;          // First logical block
    uint16_t ee_len;            // Number of blocks (max 32768)
    uint16_t ee_start_hi;       // Physical block high 16 bits
    uint32_t ee_start_lo;       // Physical block low 32 bits
} __attribute__((packed));

// Extent index (12 bytes) - internal node entry
struct Ext4ExtentIdx {
    uint32_t ei_block;          // Logical block covered
    uint32_t ei_leaf_lo;        // Physical block of next level
    uint16_t ei_leaf_hi;        // Physical block high 16 bits
    uint16_t ei_unused;
} __attribute__((packed));

// Directory entry (variable length)
struct Ext4DirEntry2 {
    uint32_t inode;             // Inode number
    uint16_t rec_len;           // Directory entry length
    uint8_t  name_len;          // Name length
    uint8_t  file_type;         // File type
    char     name[255];         // File name (variable length)
} __attribute__((packed));

// Helper functions
inline uint64_t ext4_get_blocks_count(Ext4Superblock* sb) {
    return ((uint64_t)sb->s_blocks_count_hi << 32) | sb->s_blocks_count_lo;
}

inline uint64_t ext4_get_inode_size(Ext4Inode* inode) {
    return ((uint64_t)inode->i_size_high << 32) | inode->i_size_lo;
}

inline uint64_t ext4_get_extent_start(Ext4Extent* extent) {
    return ((uint64_t)extent->ee_start_hi << 32) | extent->ee_start_lo;
}

inline uint32_t ext4_get_inode_uid(Ext4Inode* inode) {
    return ((uint32_t)inode->i_uid_high << 16) | inode->i_uid;
}

inline uint32_t ext4_get_inode_gid(Ext4Inode* inode) {
    return ((uint32_t)inode->i_gid_high << 16) | inode->i_gid;
}

#endif // EXT4_STRUCTS_H
