#include <kernel/arch/i386/idt.h>
#include <kernel/panic.h>
#include <mm/paging.h>
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

extern "C" uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}


void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) 
{
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].selector = selector;
    idt_entries[num].zero = 0;
    idt_entries[num].flags = flags;
}


bool idt_init() 
{
    idt_pointer.limit = sizeof(struct idt_entry) * 256 - 1;
    idt_pointer.base = (uint32_t)&idt_entries;

    for (int i = 0; i < 256; i++) idt_set_gate(i, 0, 0, 0);    
    
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);  
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);  
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E); 

    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);    
    
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    outb(0x21, 0x20);  
    outb(0xA1, 0x28);  
    
    outb(0x21, 0x04);  
    outb(0xA1, 0x02);  
    
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    
    outb(0x21, 0x00);  
    outb(0xA1, 0xFF);
    
    auto unmask_irq = [](uint8_t irq)
    {
        uint16_t port = (irq < 8) ? 0x21 : 0xA1;
        uint8_t value =  inb(port) & ~(1 << (irq % 8));
        outb(port, value);
    };

    unmask_irq(0);
    unmask_irq(1);

    idt_flush((uint32_t)(&idt_pointer));
    return true;
}


static const char* exception_messages[] = 
{
    "Divide by zero",
    "Debug",
    "Non-maskable interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack fault",
    "General protection fault",
    "Page fault",
    "Reserved",
    "x87 FPU error",
    "Alignment check",
    "Machine check",
    "SIMD floating point exception"
};


extern "C" void page_fault_handler(uint32_t error_code) 
{
    
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address));
    
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_RED));
    printf("=== PAGE FAULT ===\n");
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    printf("Error Code: 0x%x (", error_code);
    if (!(error_code & 0x1)) printf("Not present ");
    if (error_code & 0x2)    printf("Write-violation ");
    if (error_code & 0x4)    printf("User-mode");
    printf(")\n");
    
    panic(KernelError::K_ERR_PAGE_FAULT, nullptr, error_code);
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

    const char* message = (regs->int_no < 20) ? exception_messages[regs->int_no] : "Unknown exception";
    printf("\nEXCEPTION: %s (Int: %d)\n", message, regs->int_no);
    panic(KernelError::K_ERR_UNKNOWN_ERROR, message);
}