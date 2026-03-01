/*
 * keonOS - include/fs/vfs.h
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

#ifndef VFS_H
#define VFS_H

#include <fs/vfs_node.h>

extern VFSNode* vfs_root;
extern VFSNode* cwd_node;

void vfs_init(VFSNode* root_node = nullptr);
void vfs_mount(VFSNode* node);

VFSNode* vfs_open(const char* path);
uint32_t vfs_read(VFSNode* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(VFSNode* node, uint32_t offset, uint32_t size, uint8_t* buffer);
vfs_dirent* vfs_readdir(VFSNode* node, uint32_t index);
void vfs_close(VFSNode* node);
VFSNode* vfs_create(const char* path, uint32_t flags);
int vfs_mkdir(const char* path, uint32_t mode);
bool vfs_unlink(const char* path);

#endif 	// VFS_H