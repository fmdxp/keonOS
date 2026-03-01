/*
 * keonOS - include/drivers/vga.h
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


#ifndef VGA_H
#define VGA_H

#include <kernel/constants.h>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

enum vga_color : uint8_t
{
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_ORANGE = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_YELLOW = 14,
	VGA_COLOR_WHITE = 15,
};

struct vga_color_t
{
    uint8_t fg; 		// foreground
    uint8_t bg; 		// background

    constexpr vga_color_t(uint8_t foreground, uint8_t background) : fg(foreground), bg(background) {}
    constexpr uint8_t to_byte() const { return (bg << 4) | (fg & 0x0F); }
};

static inline uint16_t vga_entry (unsigned char uc, vga_color_t color)
{
	return static_cast<uint16_t>(uc) | static_cast<uint16_t>(color.to_byte()) << 8;
}


void terminal_initialize(void);
void terminal_setcolor(vga_color_t color);
void terminal_putentryat(char c, vga_color_t color, size_t x, size_t y);
void terminal_putchar(char c);
void terminal_scroll(void);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void terminal_clear(vga_color_t custom_color = vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
void update_cursor(size_t x, size_t y);
void terminal_move_cursor(int dx);

#endif		// VGA_H