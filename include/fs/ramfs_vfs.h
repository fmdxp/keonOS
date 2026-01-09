#ifndef RAMFS_VFS_H
#define RAMFS_VFS_H

#include <kernel/error.h>
#include <fs/vfs_node.h>
#include <fs/ramfs.h>
#include <mm/heap.h>


class KeonFS_File : public VFSNode 
{
public:
    void* data_ptr;
    KeonFS_File(const char* n, uint32_t s, void* ptr);
    
    uint32_t read(uint32_t offset, uint32_t size, uint8_t* buffer) override;
    void open() override;
    void close() override;
};


class KeonFS_MountNode : public VFSNode 
{
public:
    void* base;
    KeonFS_Info* info;
    VFSNode* children[128];
    uint32_t children_count;

    KeonFS_MountNode(const char* mount_name, void* addr) : children_count(0)
    {
        memset(children, 0, sizeof(children));
        strncpy(this->name, mount_name, 127);

        this->base = addr;
        this->info = (KeonFS_Info*)addr;


        if (this->info->magic != KEONFS_MAGIC) 
            panic(KernelError::K_ERR_RAMFS_MAGIC_FAILED, nullptr, 0UL);
        

        this->size = info->fs_size;
        this->type = VFS_DIRECTORY;

        KeonFS_FileHeader* hdr = (KeonFS_FileHeader*)((uintptr_t)base + sizeof(KeonFS_Info));


        for (uint32_t i = 0; i < info->total_files; i++)
        {

            if (hdr->offset + hdr->size > info->fs_size)
                panic(KernelError::K_ERR_GENERAL_PROTECTION, "File not in ramdisk", 0UL);
            

            KeonFS_File* child = new KeonFS_File(hdr->name, hdr->size, (void*)((uintptr_t)this->base + hdr->offset));
            add_child(child);

            if (hdr->next_header == 0) break;
            hdr = (KeonFS_FileHeader*)((uintptr_t)base + hdr->next_header);
        }
    }

    VFSNode* finddir(const char* name) override;
    vfs_dirent* readdir(uint32_t index) override;
    
    void add_child(VFSNode* node);
    uint32_t read(uint32_t offset, uint32_t size, uint8_t* buffer) override;
    void open() override {}
    void close() override {}
};

#endif // RAMFS_VFS_H