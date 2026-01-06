#include <string.h>

char* strncpy(char* dest, const char* src, size_t n) 
{
    char* saved = dest;
    while (n > 0 && *src != '\0') 
	{
        *dest++ = *src++;
        n--;
    }
    while (n > 0)
	{
        *dest++ = '\0';
        n--;
    }
    return saved;
}