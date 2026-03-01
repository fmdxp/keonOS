/*
 * keonOS - include/kernel/constants.h
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

#ifndef CONSTANTS_H
#define CONSTANTS_H

// OS VERSION CONSTANTS

#define OS_NAME				"keonOS"
#define OS_VERSION_MAJOR	"0"
#define OS_VERSION_MINOR	"6"
#define OS_VERSION_PATCH	" "			// Uses letters

#define OS_VERSION_STRING 			OS_NAME " v" OS_VERSION_MAJOR "." OS_VERSION_MINOR OS_VERSION_PATCH
#define OS_VERSION_STRING_NO_NAME 	OS_VERSION_MAJOR "." OS_VERSION_MINOR OS_VERSION_PATCH



// KERNEL CONSTANTS

#define KERNEL_VIRT_OFFSET 0xFFFFFFFF80000000



// C - CPP CONSTANTS

#ifndef NULL
	#ifdef __cplusplus
		#define NULL nullptr
	#else
		#define NULL ((void*)0)
	#endif
#endif



// VGA CONSTANTS

#define VGA_WIDTH		80
#define VGA_HEIGHT		25
#define VGA_MEMORY		(0xB8000 + KERNEL_VIRT_OFFSET)



// PAGING CONSTANTS

#define PAGE_SIZE 4096

#define PML4_SIZE 512
#define PDPT_SIZE 512
#define PD_SIZE   512
#define PT_SIZE   512

#define PML4_IDX(addr) (((uintptr_t)(addr) >> 39) & 0x1FF)
#define PDPT_IDX(addr) (((uintptr_t)(addr) >> 30) & 0x1FF)
#define PD_IDX(addr)   (((uintptr_t)(addr) >> 21) & 0x1FF)
#define PT_IDX(addr)   (((uintptr_t)(addr) >> 12) & 0x1FF)


// IDT CONSTANTS

#define IDT_GATE_FLAGS 0x8E



// KEYBOARD CONSTANTS

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_BUFFER_SIZE 256

#define KEY_UP   0x11
#define KEY_DOWN 0x12
#define KEY_LEFT 0x13
#define KEY_RIGHT 0x14



// SHELL CONSTANTS

#define SHELL_BUFFER_SIZE 	1024
#define SHELL_MAX_ARGS		32
#define MAX_HISTORY 		10



// SERIAL CONSTANTS

#define COM1 0x3F8



// TIMER CONSTANTS

#define PIT_CHANNEL0_DATA 0x40
#define PIT_CHANNEL1_DATA 0x41
#define PIT_CHANNEL2_DATA 0x42
#define PIT_COMMAND       0x43
#define PIT_FREQUENCY 1193182

#define PIC1_COMMAND 0x20
#define PIC_EOI      0x20



// THREAD CONSTANTS

#define THREAD_NOT_FOUND (uint32_t)-1
#define THREAD_AMBIGUOUS (uint32_t)-2



// ATA CONSTANTS

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERR          0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LOW      0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HIGH     0x1F5
#define ATA_PRIMARY_DRIVE_SEL    0x1F6
#define ATA_PRIMARY_COMM_STAT    0x1F7



// EXT4 CONSTANTS

#define MAX_TRANS_BLOCKS 16

#endif 		// CONSTANTS_H
