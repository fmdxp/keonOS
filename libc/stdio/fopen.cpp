#include <stdio.h>
#include <stdlib.h>
#include <fs/vfs.h>
#include <mm/heap.h>


extern "C" FILE* fopen(const char* filename, const char* mode) 
{
    VFSNode* node = vfs_open(filename);
    if (!node) return NULL;
	
    FILE* stream = (FILE*)kmalloc(sizeof(FILE));
    if (!stream) {
        vfs_close(node);
        return NULL;
    }

    stream->fd = (int)node;
    stream->offset = 0;
    stream->size = node->size;
    stream->error = 0;

    return stream;
}