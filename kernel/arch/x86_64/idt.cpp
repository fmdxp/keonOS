/*
 * keonOS - kernel/arch/x86_64/idt.cpp
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
#include <kernel/arch/x86_64/paging.h>
#include <kernel/panic.h>
#include <drivers/vga.h>
#include <stdio.h>

struct idt_entry idt_entries[256];
struct idt_ptr idt_pointer;


extern "C" void outb(uint16_t port, uint8_t value) 
{
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

extern "C" uint8_t inb(uint16_t port) 
{
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

extern "C" void outw(uint16_t port, uint16_t value) 
{
    asm volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

extern "C" uint16_t inw(uint16_t port) 
{
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}


void idt_set_gate(uint8_t num, uint64_t base, uint16_t selector, uint8_t flags) 
{
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].base_mid = (base >> 16) & 0xFFFF;
    idt_entries[num].base_high = (base >> 32) & 0xFFFFFFFF;
    
    idt_entries[num].selector = selector;
    idt_entries[num].ist = 0;     
    idt_entries[num].flags = flags;
    idt_entries[num].reserved = 0;
}

bool idt_init() 
{
    idt_pointer.limit = (sizeof(struct idt_entry) * 256) - 1;
    idt_pointer.base = (uint64_t)&idt_entries;

    for (int i = 0; i < 256; i++)
        idt_set_gate((uint8_t)i, (uint64_t)0, (uint16_t)0, (uint8_t)0);
    
    idt_set_gate(0,  (uint64_t)isr0,  0x08, 0x8E);  
    idt_set_gate(1,  (uint64_t)isr1,  0x08, 0x8E);  
    idt_set_gate(14, (uint64_t)isr14, 0x08, 0x8E); 

    idt_set_gate(32, (uint64_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint64_t)irq1,  0x08, 0x8E);    
    
    outb(0x20, 0x11); outb(0xA0, 0x11); 
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    
    outb(0x21, 0xF8); 
    outb(0xA1, 0xFF);

    idt_flush((uintptr_t)(&idt_pointer));
    return true;
}


extern "C" void page_fault_handler(uint64_t error_code) 
{
    uint64_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address));
    
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_RED));
    printf("\n=== PAGE FAULT (x86_64) ===\n");
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    printf("Faulting Address: 0x%lx\n", faulting_address);
    printf("Error Code: 0x%lx\n", error_code);
    
    panic(KernelError::K_ERR_PAGE_FAULT, "MMU Violation");
}

extern "C" void isr_exception_handler(registers_t* regs) 
{
    if (regs->int_no >= 32 && regs->int_no <= 47) 
	{
        if (regs->int_no == 32) timer_handler();
        else if (regs->int_no == 33) keyboard_handler();
        
        if (regs->int_no >= 40) outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }

    if (regs->int_no == 14) 
	{
        page_fault_handler(regs->err_code);
        return;
    }

    printf("\nEXCEPTION: Int %d (Error Code: 0x%lx)\n", (int)regs->int_no, regs->err_code);
    panic(KernelError::K_ERR_UNKNOWN_ERROR, "CPU Exception");
}