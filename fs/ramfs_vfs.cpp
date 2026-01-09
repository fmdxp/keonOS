#include <kernel/panic.h>
#include <fs/ramfs_vfs.h>
#include <fs/vfs_node.h>
#include <fs/ramfs.h>
#include <stdio.h>

KeonFS_File::KeonFS_File(const char* n, uint32_t s, void* ptr) 
{
    strncpy(this->name, n, 127);
    this->name[127] = '\0';
    this->size = s;
    this->data_ptr = ptr;
    this->type = VFS_FILE;
}

void KeonFS_MountNode::add_child(VFSNode* node) 
{
    if (children_count >= 128) 
        panic(KernelError::K_ERR_GENERAL_PROTECTION, "Too many files in ramfs");
    children[children_count] = node;

    printf("[DEBUG] Added child: %s, type=%d\n", node->name, node->type);
    if (node->type == VFS_FILE)
        printf("[DEBUG]   size=%d, data_ptr=%x\n", ((KeonFS_File*)node)->size, ((KeonFS_File*)node)->data_ptr);

    children_count++;
}


uint32_t KeonFS_File::read(uint32_t offset, uint32_t size, uint8_t* buffer) 
{
    if (offset >= this->size) 
        return 0;
    
    uint32_t to_read = (offset + size > this->size) ? (this->size - offset) : size;
    memcpy(buffer, (uint8_t*)data_ptr + offset, to_read);
    return to_read;
}


VFSNode* KeonFS_MountNode::finddir(const char* name) 
{
    for (uint32_t i = 0; i < children_count; i++) 
    {
        if (strcmp(children[i]->name, name) == 0)
            return children[i];
    }
    return nullptr;
}

void KeonFS_File::open() {}
void KeonFS_File::close() {}

vfs_dirent* KeonFS_MountNode::readdir(uint32_t index) 
{
    if (index >= children_count) return nullptr;
    static vfs_dirent de;
    strcpy(de.name, children[index]->name);
    de.inode = index;
    return &de;
}

uint32_t KeonFS_MountNode::read(uint32_t, uint32_t, uint8_t*) 
{ 
    return 0;
}