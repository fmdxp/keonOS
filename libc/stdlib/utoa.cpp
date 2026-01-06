#include <stdlib.h>

char* utoa(unsigned int value, char* str, int base) 
{
    char *rc;
    char *ptr;
    char *lowstr;
    
    rc = ptr = str;
    
    if (base < 2 || base > 36)
	{
        *str = '\0';
        return str;
    }

    do 
	{
        unsigned int res = value % base;
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[res];
        value /= base;
    } while (value);

    *ptr-- = '\0';

    lowstr = rc;
    while (lowstr < ptr) 
	{
        char tmp = *lowstr;
        *lowstr++ = *ptr;
        *ptr-- = tmp;
    }
    return rc;
}