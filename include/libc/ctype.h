#ifndef _CTYPE_H
#define _CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif

static inline int isspace(int c) 
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r');
}

static inline int isdigit(int c) 
{
    return (c >= '0' && c <= '9');
}

static inline int isalpha(int c) 
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

static inline int toupper(int c) 
{
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    return c;
}

#ifdef __cplusplus
}
#endif

#endif