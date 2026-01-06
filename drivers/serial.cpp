#include <kernel/arch/i386/idt.h>
#include <kernel/constants.h>
#include <drivers/serial.h>


void serial_install() 
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

int is_transmit_empty() 
{
    return inb(COM1 + 5) & 0x20;
}

void write_serial(char a) 
{
    while (is_transmit_empty() == 0);
    outb(COM1, a);
}

void serial_putc(char c) 
{
    if (c == '\b') 
    {
        while ((inb(COM1 + 5) & 0x20) == 0);
        outb(COM1, '\b');
        
        while ((inb(COM1 + 5) & 0x20) == 0);
        outb(COM1, ' ');
        
        while ((inb(COM1 + 5) & 0x20) == 0);
        outb(COM1, '\b');
    } 
    else 
    {
        while ((inb(COM1 + 5) & 0x20) == 0);
        outb(COM1, c);
    }
}