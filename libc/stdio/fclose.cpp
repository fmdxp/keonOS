#include <stdio.h>
#include <stdlib.h>
#include <fs/vfs.h>
#include <mm/heap.h>

extern "C" int fclose(FILE* stream) 
{
    if (!stream) return EOF;

    VFSNode* node = (VFSNode*)stream->fd;
    
    vfs_close(node);
    
    kfree(stream);

    return 0;
}