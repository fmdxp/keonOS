#ifndef VFS_NODE_H
#define VFS_NODE_H

#include <stdint.h>
#include <string.h>

enum VFS_TYPE { VFS_FILE = 1, VFS_DIRECTORY = 2, VFS_DEVICE = 3 };

struct vfs_dirent 
{
    char name[128];
    uint32_t inode;
};

class VFSNode 
{
public:
    char name[128];
    uint32_t type;
    uint32_t size;
    uint32_t inode;

    VFSNode() : type(0), size(0), inode(0) 
	{ 
		name[0] = '\0'; 
	}
    virtual ~VFSNode() {}

    virtual uint32_t read(uint32_t offset, uint32_t size, uint8_t* buffer) = 0;
    virtual uint32_t write(uint32_t offset, uint32_t size, uint8_t* buffer) { return 0; }
    virtual void open() = 0;
    virtual void close() = 0;
    virtual vfs_dirent* readdir(uint32_t index) { return nullptr; }
    virtual VFSNode* finddir(const char* name) { return nullptr; }
};

class RootFS : public VFSNode 
{
private:
    VFSNode* mounts[32];
    uint32_t mount_count;

    
public:

    RootFS() : mount_count(0) 
	{
        memset(mounts, 0, sizeof(mounts));
        strcpy(name, "/");
        type = VFS_DIRECTORY;
    }

	
    void register_node(VFSNode* node) 
	{
        if (mount_count < 32) mounts[mount_count++] = node;
    }


    uint32_t read(uint32_t, uint32_t, uint8_t*) override { return 0; }
    void open() override {}
    void close() override {}


    VFSNode* finddir(const char* name) override 
    {
        for (uint32_t i = 0; i < mount_count; i++) 
        {
            if (strcmp(mounts[i]->name, name) == 0)
                return mounts[i];
        }
        return nullptr;
    }


    vfs_dirent* readdir(uint32_t index) override 
    {
        static vfs_dirent de;
        if (index < mount_count) 
        {
            strcpy(de.name, mounts[index]->name);
            de.inode = index;
            return &de;
        }
        return nullptr;
    }
};

#endif