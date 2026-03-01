/*
 * keonOS - kernel/syscalls/sys_stat.cpp
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

#include <kernel/syscalls/syscalls.h>
#include <kernel/arch/x86_64/thread.h>
#include <fs/vfs.h>
#include <string.h>

// Must match userspace struct stat
struct stat {
	uint64_t st_dev;
	uint64_t st_ino;
	uint32_t st_mode;
	uint32_t st_nlink;
	uint32_t st_uid;
	uint32_t st_gid;
	uint64_t st_rdev;
	uint64_t st_size;
	uint64_t st_blksize;
	uint64_t st_blocks;
	uint64_t st_atime;
	uint64_t st_mtime;
	uint64_t st_ctime;
};

// File types matching userspace
#define S_IFMT  0xF000
#define S_IFDIR 0x4000
#define S_IFCHR 0x2000
#define S_IFBLK 0x6000
#define S_IFREG 0x8000

uint64_t sys_stat(uint64_t path_ptr, uint64_t statbuf_ptr, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) 
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    char path[256];
    if (!copy_from_user(path, (const void*)path_ptr, 255)) return -1;
    path[255] = '\0';

    VFSNode* node = vfs_root->finddir(path); // This is simplified, vfs_open might be better if it didn't open
    // Since vfs_open increases refcount, we should use it and close it.
    // However, finddir might be enough if we just want info. 
    // But finddir on root might not traverse? vfs_open does traversal.
    // Let's use vfs_open and vfs_close.
    node = vfs_open(path);

    if (!node) return -1;

    struct stat st;
    memset(&st, 0, sizeof(st));

    st.st_ino = node->inode;
    st.st_size = node->size;
    
    // Fake some values
    st.st_dev = 1;
    st.st_nlink = 1;
    st.st_uid = 0;
    st.st_gid = 0;
    st.st_blksize = 512;
    st.st_blocks = (node->size + 511) / 512;

    if (node->type == VFS_DIRECTORY) st.st_mode = S_IFDIR | 0755;
    else if (node->type == VFS_DEVICE) st.st_mode = S_IFCHR | 0600;
    else st.st_mode = S_IFREG | 0644; // VFS_FILE

    vfs_close(node);

    if (!copy_to_user((void*)statbuf_ptr, &st, sizeof(st))) return -1;

    return 0;
}

uint64_t sys_fstat(uint64_t fd, uint64_t statbuf_ptr, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) 
{
    (void)a3; (void)a4; (void)a5; (void)a6;
    thread_t* current = thread_get_current();
    if (fd >= 16 || !current->fd_table[fd]) return -1;

    VFSNode* node = current->fd_table[fd];

    struct stat st;
    memset(&st, 0, sizeof(st));

    st.st_ino = node->inode;
    st.st_size = node->size;
    
    st.st_dev = 1;
    st.st_nlink = 1;
    st.st_uid = 0;
    st.st_gid = 0;
    st.st_blksize = 512;
    st.st_blocks = (node->size + 511) / 512;

    if (node->type == VFS_DIRECTORY) st.st_mode = S_IFDIR | 0755;
    else if (node->type == VFS_DEVICE) st.st_mode = S_IFCHR | 0600;
    else st.st_mode = S_IFREG | 0644;

    if (!copy_to_user((void*)statbuf_ptr, &st, sizeof(st))) return -1;

    return 0;
}
