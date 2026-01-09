#ifndef VFS_H
#define VFS_H

#include "vfs_node.h"

void vfs_init();
void vfs_mount(VFSNode* node);

VFSNode* vfs_open(const char* path);
uint32_t vfs_read(VFSNode* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(VFSNode* node, uint32_t offset, uint32_t size, uint8_t* buffer);
vfs_dirent* vfs_readdir(VFSNode* node, uint32_t index);
void vfs_close(VFSNode* node);

#endif 	// VFS_H