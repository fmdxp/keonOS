#ifndef _KERNEL_IDT_H
#define _KERNEL_IDT_H

#include <stdint.h>
#include <kernel/constants.h>

struct idt_entry
{
	uint16_t 	base_low;
	uint16_t 	selector;
	uint8_t 	zero;
	uint8_t 	flags;
	uint16_t 	base_high;
} __attribute__((packed));

struct idt_ptr
{
	uint16_t limit;
	uint32_t base;
} __attribute__((packed));

struct registers_t
{
	uint32_t ds;
	uint32_t edi, esi, ebp, esp_ptr, ebx, edx, ecx, eax;
	uint32_t int_no, err_code;
	uint32_t eip, cs, eflags, useresp, ss;
};


extern "C" void outb(uint16_t port, uint8_t value);
extern "C" uint8_t inb(uint16_t port);
extern "C" void outw(uint16_t port, uint16_t value);
extern "C" uint16_t inw(uint16_t port);

bool idt_init();
void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags);


extern "C" void idt_flush(uint32_t);

extern "C" void timer_handler();
extern "C" void keyboard_handler();

extern "C" void isr0();
extern "C" void isr1();
extern "C" void isr14(); 
extern "C" void irq0(); 
extern "C" void irq1(); 

#endif		// _KERNEL_IDT_H