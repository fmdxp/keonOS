/*
 * keonOS - drivers/vga.cpp
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
#include <drivers/vga.h>
#include <string.h>

size_t terminal_row;
size_t terminal_column;
vga_color_t terminal_color = vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
uint16_t* volatile terminal_buffer = (uint16_t*)VGA_MEMORY;

static void terminal_enable_cursor(uint8_t cursor_start, uint8_t cursor_end) 
{
	outb(0x3D4, 0x0A);
	outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
 
	outb(0x3D4, 0x0B);
	outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void terminal_initialize(void)
{
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
	
	for (size_t y = 0; y < VGA_HEIGHT; y++)
		for (size_t x = 0; x < VGA_WIDTH; x++)
			terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);

	terminal_enable_cursor(14, 15);
}

void terminal_setcolor(vga_color_t color)
{
	terminal_color = color;
}

void terminal_putentryat(char c, vga_color_t color, size_t x, size_t y)
{
	terminal_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
}

void terminal_scroll(void)
{
	memmove(terminal_buffer, terminal_buffer + VGA_WIDTH, sizeof(uint16_t) * (VGA_HEIGHT - 1) * VGA_WIDTH);

	for (size_t x = 0; x < VGA_WIDTH; x++)
		terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
	
	terminal_row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c)
{	
	if (c == '\n')
	{
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT) terminal_scroll();
    	update_cursor(terminal_column, terminal_row);
		return;
	}

	else if (c == '\t') 
	{
        terminal_column = (terminal_column + 8) & ~7;
        if (terminal_column >= VGA_WIDTH) 
		{
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT) terminal_scroll();
        }
		update_cursor(terminal_column, terminal_row);
		return;
    }

	if (c == '\b') 
	{
		if (terminal_column > 0) terminal_column--;
        else if (terminal_row > 0) 
		{
            terminal_row--;
            terminal_column = VGA_WIDTH - 1;
        }

        const size_t index = terminal_row * VGA_WIDTH + terminal_column;
        terminal_buffer[index] = vga_entry(' ', terminal_color);        
	    update_cursor(terminal_column, terminal_row);
        return;
    }

	terminal_putentryat(c, terminal_color, terminal_column, terminal_row);

	if (++terminal_column == VGA_WIDTH) 
	{
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT) terminal_scroll();
	}

    update_cursor(terminal_column, terminal_row);
}



void terminal_write(const char* data, size_t size)
{
	for (size_t i = 0; i < size; i++) terminal_putchar(data[i]);
}

void terminal_writestring(const char* data)
{
	terminal_write(data, strlen(data));
}

void terminal_clear(vga_color_t custom_color)
{
	terminal_row = 0;
	terminal_column = 0;

	for (size_t y = 0; y < VGA_HEIGHT; y++)
		for (size_t x = 0; x < VGA_WIDTH; x++)
			terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', custom_color);
	
	update_cursor(0, 0);
}


void update_cursor(size_t x, size_t y) 
{
    uint16_t pos = (uint16_t)(y * VGA_WIDTH + x);

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF)); 
	outb(0x80, 0);
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void terminal_move_cursor(int dx)
{
    int current_pos = (int)terminal_row * VGA_WIDTH + (int)terminal_column;
    current_pos += dx;
    if (current_pos < 0) current_pos = 0;
    else if (current_pos >= (int)(VGA_HEIGHT * VGA_WIDTH)) current_pos = VGA_HEIGHT * VGA_WIDTH - 1;
    
    terminal_row = (size_t)(current_pos / VGA_WIDTH);
    terminal_column = (size_t)(current_pos % VGA_WIDTH);
    update_cursor(terminal_column, terminal_row);
}