#include <stdio.h>

extern "C" int fseek(FILE* stream, long offset, int whence) 
{
    if (!stream) return -1;

    switch (whence) 
    {
        case SEEK_SET:
            if (offset < 0) return -1;
            stream->offset = (uint32_t)offset;
            break;

        case SEEK_CUR:
            if (offset < 0 && (uint32_t)(-offset) > stream->offset) return -1;
            stream->offset += offset;
            break;

        case SEEK_END:
            if (offset > 0) return -1;
            if ((uint32_t)(-offset) > stream->size) return -1;
            stream->offset = stream->size + offset;
            break;

        default:
            return -1;
    }

    stream->error = 0;
    return 0;
}