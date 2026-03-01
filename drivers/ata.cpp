/*
 * keonOS - drivers/ata.cpp
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
#include <drivers/ata.h>


void ATADriver::wait_bsy() { while (inb(ATA_PRIMARY_COMM_STAT) & 0x80); }
void ATADriver::wait_drq() { while (!(inb(ATA_PRIMARY_COMM_STAT) & 0x08)); }

void ATADriver::read_sectors(uint32_t lba, uint8_t count, uint8_t* buffer) 
{
    ATADriver::wait_bsy();

    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, count);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x20);

    uint16_t* ptr = (uint16_t*)buffer;
    for (int j = 0; j < count; j++) 
	{
        ATADriver::wait_bsy();
        ATADriver::wait_drq();
        for (int i = 0; i < 256; i++) *ptr++ = inw(0x1F0);
    }
}


void ATADriver::write_sectors(uint32_t lba, uint8_t count, uint8_t* buffer) 
{
    ATADriver::wait_bsy();
    
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, count);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30);

    uint16_t* ptr = (uint16_t*)buffer;
    for (int j = 0; j < count; j++) 
	{
        ATADriver::wait_bsy();
        ATADriver::wait_drq();
        for (int i = 0; i < 256; i++) outw(0x1F0, *ptr++);
    }
}