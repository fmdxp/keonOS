/*
 * keonOS - drivers/serial.cpp
 * Copyright (C) 2025-2026 fmdxp
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ADDITIONAL TERMS (Per Section 7 of the GNU GPLv3):
 * - Original author attributions must be preserved in all copies.
 * - Modified versions must be marked as different from the original.
 * - The name "keonOS" or "fmdxp" cannot be used for publicity without permission.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 */


#include <kernel/arch/x86_64/idt.h>
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

int serial_received() 
{
   return inb(COM1 + 5) & 1;
}

char read_serial() 
{
   while (serial_received() == 0);
   return inb(COM1);
}

char serial_getc()
{
    if (serial_received()) return inb(COM1);
    return 0;
}

void write_serial(char a) 
{
    while (is_transmit_empty() == 0);
    outb(COM1, a);
}

void serial_putc(char c) 
{
    if (c == '\n') 
    {
        write_serial('\r');
        write_serial('\n');
    }
    else if (c == '\b') 
    {
        write_serial('\b');
        write_serial(' ');
        write_serial('\b');
    } 
    else write_serial(c);
    
}

void serial_move_cursor(int dx)
{
    if (dx == 0) return;
    
    write_serial('\x1B');
    write_serial('[');
    
    int abs_dx = dx > 0 ? dx : -dx;
    char dir = dx > 0 ? 'C' : 'D';
    
    if (abs_dx >= 100) {
        write_serial('0' + (abs_dx / 100));
        write_serial('0' + ((abs_dx / 10) % 10));
        write_serial('0' + (abs_dx % 10));
    } else if (abs_dx >= 10) {
        write_serial('0' + (abs_dx / 10));
        write_serial('0' + (abs_dx % 10));
    } else {
        write_serial('0' + abs_dx);
    }
    
    write_serial(dir);
}