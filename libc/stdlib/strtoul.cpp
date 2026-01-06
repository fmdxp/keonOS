#include <stdlib.h>
#include <ctype.h>

unsigned long strtoul(const char* nptr, char** endptr, int base) 
{
    const char* s = nptr;
    unsigned long acc = 0;
    int c;
    bool any = false;

    while (isspace((unsigned char)*s)) s++;

    if (base == 0) 
	{
        if (*s == '0') 
		{
            s++;

            if (*s == 'x' || *s == 'X') 
			{
                s++;
                base = 16;
            } 
			else base = 8;
            
        } 
		else base = 10;
        
    } 

	else if (base == 16) 
        if (*s == '0' && (s[1] == 'x' || s[1] == 'X'))
			 s += 2;
    

    for (;; s++) 
	{
        c = (unsigned char)*s;

        if (isdigit(c))
			c -= '0';

        else if (isalpha(c))
			c = toupper(c) - 'A' + 10;

        else break;

        if (c >= base) break;
        acc = acc * base + c;
        any = true;
    }

    if (endptr != nullptr)
        *endptr = (char*)(any ? s : nptr);

    return acc;
}