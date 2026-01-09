#include <stdio.h>
#include <fs/vfs.h>

extern "C" size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) 
{
    if (!stream || stream->fd == 0) return 0;

    VFSNode* node = (VFSNode*)stream->fd;
    uint32_t total_bytes = size * nmemb;

    uint32_t read = vfs_read(node, stream->offset, total_bytes, (uint8_t*)ptr);

    stream->offset += read;

    return (size > 0) ? (read / size) : 0;
}