#include <kernel/panic.h>
#include <drivers/vga.h>
#include <stdio.h>
#include <string.h>

/*
 * panic: Handles unrecoverable kernel errors.
 * Displays system state, CPU registers, and a memory dump before halting.
*/

void panic(KernelError error, const char* message, uint32_t error_code)
{
    // 1. Safety first: Disable interrupts to prevent context switching during panic
    asm volatile("cli");   
    
    // Clear screen and set a high-visibility alert color (White on Red)
    terminal_clear();
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_RED));
    
    printf("\n");
    printf("========================================\n");
    printf("   KERNEL PANIC - SYSTEM HALTED\n");
    printf("========================================\n");
    printf("\n");
    
    // Display the specific error type categorized by the kernel
    terminal_setcolor(vga_color_t(VGA_COLOR_YELLOW, VGA_COLOR_RED));
    printf("Error type: ");
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_RED));
    printf("%s", kerror_to_str(error));
    printf("\n");
    
    // If a custom developer message was provided, show it
    if (message)
    {
        terminal_setcolor(vga_color_t(VGA_COLOR_YELLOW, VGA_COLOR_RED));
        printf("Message: ");
        terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_RED));
        printf("%s", message);
    }
    
    // Display raw error code (useful for Page Faults or Exception codes)
    if (error_code || error_code == 0UL)
    {
        terminal_setcolor(vga_color_t(VGA_COLOR_YELLOW, VGA_COLOR_RED));
        printf("\nError Code: 0x");
        terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_RED));
        
        char buffer[32];
        itoa(static_cast<int>(error_code), buffer, 16);
        printf("%s", buffer);
    }

    // 2. CPU Snapshot: Capture General Purpose and Control Registers
    // These reflect the CPU state at the time the panic was called
    uint32_t eax, ebx, ecx, edx, esp, ebp, esi, edi, cr0, cr2, cr3;
    asm volatile("mov %%eax, %0" : "=r"(eax));
    asm volatile("mov %%ebx, %0" : "=r"(ebx));
    asm volatile("mov %%ecx, %0" : "=r"(ecx));
    asm volatile("mov %%edx, %0" : "=r"(edx));
    asm volatile("mov %%esi, %0" : "=r"(esi));
    asm volatile("mov %%edi, %0" : "=r"(edi));
    asm volatile("mov %%ebp, %0" : "=r"(ebp));
    asm volatile("mov %%esp, %0" : "=r"(esp));

    // Paging Control Registers (CR2 contains the faulting address if a Page Fault occurred)
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    asm volatile("mov %%cr2, %0" : "=r"(cr2));
    asm volatile("mov %%cr3, %0" : "=r"(cr3));


    printf("\n");
    terminal_setcolor(vga_color_t(VGA_COLOR_YELLOW, VGA_COLOR_RED));
    printf("Registers:\n");
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_RED));
    printf("EAX: %08x EBX: %08x ECX: %08x EDX: %08x\n", eax, ebx, ecx, edx);
    printf("ESI: %08x EDI: %08x EBP: %08x ESP: %08x\n", esi, edi, ebp, esp); 
    printf("CR0: %08x  CR2: %08x  CR3: %08x\n", cr0, cr2, cr3);


    // 3. Memory Dump: Show a hex/ASCII preview of the stack
    // This allows the developer to see local variables and return addresses
    printf("\n\n\n");
    terminal_setcolor(vga_color_t(VGA_COLOR_YELLOW, VGA_COLOR_RED));
    printf("Memory Dump (Stack Trace):\n");
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_RED));

    uint8_t* ptr = (uint8_t*)esp;
    for (int i = 0; i < 64; i += 16) 
    {
        printf("0x%x: ", (uint32_t)(ptr + i));

        for (int j = 0; j < 16; j++) 
        {
            if (ptr[i+j] < 0x10) printf("0");
            char buf[10];
            itoa(ptr[i+j], buf, 16);
            printf("%s ", buf);
        }

        printf(" | ");

        // ASCII sidebar: Shows printable characters, hides control characters with '.'
        for (int j = 0; j < 16; j++) 
        {
            char c = ptr[i+j];
            if (c >= 32 && c <= 126) printf("%c", c);
            else printf(".");
        }
        printf("\n");
    }

    printf("\n");
    terminal_setcolor(vga_color_t(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_RED));
    printf("System: %s\n", OS_VERSION_STRING);

    // Final Halt: Put the CPU in a low-power state and wait for a hard reset
    while (1) { asm volatile ("hlt"); }
}