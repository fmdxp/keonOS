#include <kernel/constants.h>
#include <kernel/kernel.h>
#include <kernel/shell.h>
#include <kernel/error.h>
#include <kernel/panic.h>

#include <kernel/arch/i386/constructor.h>
#include <kernel/arch/i386/gdt.h>
#include <kernel/arch/i386/idt.h>

#include <mm/vmm.h>
#include <mm/heap.h>
#include <mm/paging.h>

#include <drivers/vga.h>
#include <drivers/timer.h>
#include <drivers/serial.h>
#include <drivers/speaker.h>
#include <drivers/keyboard.h>
#include <drivers/multiboot.h>

#include <proc/thread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" uint32_t multiboot_magic;
extern "C" multiboot_info_t *multiboot_info_ptr;
extern "C" uint32_t _kernel_end;

extern "C" void kernel_main()
{
	// 1. Core Architecture Setup
    // Initialize C++ global constructors, terminal output, and CPU tables (GDT/IDT)
	initialize_constructors();
	terminal_initialize();
	gdt_init();
	idt_init();

	// 2. Memory Management Setup
    // Disable interrupts during critical memory initialization
	asm volatile("cli");

	// Calculate total system memory using information passed by the Multiboot bootloader
	uint32_t total_mem_bytes = (multiboot_info_ptr->mem_lower + multiboot_info_ptr->mem_upper) * 1024;
	
	// Enable Paging: This maps physical memory to virtual memory addresses
	paging_init();

	// 3. Dynamic Memory (Heap) Initialization
    // Define the starting point of the heap and allocate an initial 16MB block
	void* heap_start = (void*)VMM::kernel_dynamic_break;
	uint32_t initial_heap_size = 16 * 1024 * 1024;	// 16MB

	// Expand the virtual address space and initialize the kernel heap allocator
	VMM::sbrk(initial_heap_size);
	kheap_init(heap_start, initial_heap_size);
	
	// 4. Subsystem Initialization
    // Initialize the Programmable Interval Timer (PIT) at 100Hz
	timer_init(100);

	// Setup the threading system (Scheduler) and the keyboard driver
	thread_init();
	keyboard_init();
	
	// Re-enable interrupts after safe hardware/memory setup
	asm volatile("sti");
	
	
	// 5. User Interface & Branding
    // Show the boot splash screen and provide visual/auditory feedback (beep)
	terminal_clear();
	terminal_setcolor(vga_color_t(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
	
	beep(900, 100);
	
	printf("  _                     ___  ____ \n");
	printf(" | | _____  ___  _ __  / _ \\/ ___|\n");
	printf(" | |/ / _ \\/ _ \\| '_ \\| | | \\___ \\\n");
	printf(" |   <  __/ (_) | | | | |_| |___) |\n");
	printf(" |_|\\_\\___|\\___/|_| |_|\\___/|____/\n\n\n");
	
	timer_sleep(1000);	 // Brief pause to let the user see the logo
	
	terminal_clear();
	terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
	beep(900, 25);
	
	terminal_setcolor(vga_color_t(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
	printf("\n				-- keonOS --\n\n\n");
	terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));


	// 6. System Status Reporting
    // Log initialization results and verify Multiboot integrity
	printf("[*] Initialized Global Constructors\n");
	printf("[*] Initialized Terminal\n");
	printf("[*] Initialized GDT\n");
	printf("[*] Initialized Heap\n");
	printf("[*] Initialized IDT\n");
	printf("[*] Initialized Timer\n");
	printf("[*] Initialized Threads\n");
	printf("[*] Initialized Keyboard\n");
	printf("[*] Initialized Shell\n");
	
	// Panic if the bootloader didn't provide a valid Multiboot magic number
	if (multiboot_magic == MULTIBOOT_BOOTLOADER_MAGIC) printf("[*] Multiboot OK\n");
	else panic(KernelError::K_ERR_MULTIBOOT_FAILED, "Unknown (Magic: %x)\n", multiboot_magic);
	
	// Display detected RAM size
	if (multiboot_info_ptr->flags & (1 << 0))
		printf("Memory found: %d MB\n\n\n\n", (total_mem_bytes / 1024) / 1024);
	

	// 7. Launch First User Process
    // Initialize the shell and add it as the primary thread to the scheduler
	shell_init();
	thread_add(shell_run, "shell");

	// 8. Idle Loop
    // The main kernel thread enters a low-power halt loop
    // Context will be switched to the shell and other tasks by the timer interrupt
	while (1) asm volatile("hlt");
	
}