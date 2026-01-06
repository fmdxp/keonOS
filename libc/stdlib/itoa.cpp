#include <stdlib.h>

char* itoa(int value, char* str, int base) 
{
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    int tmp_value;
    bool negative = false;

    if (value < 0 && base == 10) 
    {
        negative = true;
        value = -value;
    }

    if (value == 0) 
    {
        *ptr++ = '0';
        *ptr = '\0';
        return str;
    }
    
    while (value != 0) 
    {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + (tmp_value - value * base)];
    }

    
    if (negative) *ptr++ = '-';
    *ptr-- = '\0';
    
    while (ptr1 < ptr) 
    {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return str;
}