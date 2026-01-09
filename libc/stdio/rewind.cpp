#include <stdio.h>

extern "C" void rewind(FILE* stream) 
{
    fseek(stream, 0L, SEEK_SET);
    stream->error = 0;
}