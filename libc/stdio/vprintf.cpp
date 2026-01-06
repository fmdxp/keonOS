#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <drivers/serial.h>

static void print(const char* data, size_t data_length) 
{
    for (size_t i = 0; i < data_length; i++)
    {
        serial_putc(data[i]);
        putchar((int) ((const unsigned char*) data)[i]);
    }
}


int vprintf(const char* format, va_list arg) 
{
    int written = 0;

    while (*format != '\0') 
    {
        if (format[0] != '%' || format[1] == '%') 
        {
            if (format[0] == '%') format++;
            size_t amount = 1;
            while (format[amount] && format[amount] != '%')
                amount++;
        
            print(format, amount);
            format += amount;
            written += amount;
            continue;
        }

        const char* format_begun_at = format++;

        bool align_left = false;
        if (*format == '-') 
        {
            align_left = true;
            format++;
        }

        int width = 0;
        while (*format >= '0' && *format <= '9') 
        {
            width = width * 10 + (*format - '0');
            format++;
        }

        if (*format == 'c') 
        {
            format++;
            char c = (char) va_arg(arg, int);
            print(&c, sizeof(c));
            written++;
        } 

        else if (*format == 's') 
        {
            format++;
            const char* s = va_arg(arg, const char*);
            if (!s) s = "(null)";
            size_t len = strlen(s);

            if (!align_left && (int)len < width) 
                for (int p = 0; p < (width - (int)len); p++) 
                {
                    char space = ' ';
                    print(&space, 1);
                    written++;
                }
            

            print(s, len);
            written += len;

            if (align_left && (int)len < width) 
                for (int p = 0; p < (width - (int)len); p++) 
                {
                    char space = ' ';
                    print(&space, 1);
                    written++;
                }
            
        } 

        else if (*format == 'd' || *format == 'i' || *format == 'u' || *format == 'x') 
        {
            char type = *format;
            format++;
            
            char buffer[32];

            if (type == 'd' || type == 'i') 
            {
                int val = va_arg(arg, int);
                itoa(val, buffer, 10);
            } 

            else if (type == 'u')
            {
                unsigned int val = va_arg(arg, unsigned int);
                utoa(val, buffer, 10);
            } 

            else 
            {
                unsigned int val = va_arg(arg, unsigned int);
                itoa(val, buffer, 16);
            }

            size_t len = strlen(buffer);
            
            if (!align_left && (int)len < width)
                for (int p = 0; p < (width - (int)len); p++) 
                {
                    char space = ' ';
                    print(&space, 1); written++;
                }
            

            print(buffer, len);
            written += len;

            if (align_left && (int)len < width)
                for (int p = 0; p < (width - (int)len); p++)
                {
                    char space = ' '; 
                    print(&space, 1); written++;
                }
        }

        else 
        {
            size_t len = format - format_begun_at;
            print(format_begun_at, len);
            written += len;
        }
    }
    return written;
}