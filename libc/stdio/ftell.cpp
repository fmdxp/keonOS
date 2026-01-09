#include <stdio.h>

extern "C" long ftell(FILE* stream) 
{
    if (!stream) return -1L;
    return (long)stream->offset;
}