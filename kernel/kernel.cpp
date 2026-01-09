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

#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <fs/vfs_node.h>
#include <fs/ramfs_vfs.h>

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


extern "C" uint32_t _kernel_end;

extern "C" void kernel_main(uint32_t magic, multiboot_info_t* info)
{
	// 1. Core Architecture Setup
    // Initialize C++ global constructors, terminal output, and CPU tables (GDT/IDT)
	initialize_constructors();
	terminal_initialize();
	serial_install();
	gdt_init();
	idt_init();

	// 2. Memory Management Setup
    // Disable interrupts during critical memory initialization
	asm volatile("cli");
	
	// Calculate total system memory using information passed by the Multiboot bootloader
	uint32_t total_mem_bytes = (info->mem_lower + info->mem_upper) * 1024;
	

	uint32_t rd_phys = 0;
	uint32_t rd_size = 0;
	if (info->mods_count > 0) 
	{
		multiboot_module_t* m = (multiboot_module_t*)info->mods_addr;
		rd_phys = m[0].mod_start;
		rd_size = m[0].mod_end - m[0].mod_start;
	}
		

	// Enable Paging: This maps physical memory to virtual memory addresses
	paging_init();	
	
	// 3. Dynamic Memory (Heap) Initialization
    // Define the starting point of the heap and allocate an initial 16MB block
	void* heap_start = (void*)VMM::kernel_dynamic_break;
	uint32_t initial_heap_size = 4 * 1024 * 1024;	// 4MB
	
	// Expand the virtual address space and initialize the kernel heap allocator
	VMM::sbrk(initial_heap_size);
	kheap_init(heap_start, initial_heap_size);
	
	
	// 4. Subsystem Initialization
    // Initialize the Programmable Interval Timer (PIT) at 100Hz
	timer_init(100);
	
	// Setup the threading system (Scheduler) and the keyboard driver
	thread_init();
	keyboard_init();


	printf("mods_count = %d\n", info->mods_count);
	printf("mods_addr  = %x\n", info->mods_addr);

	void* ramdisk_vaddr = nullptr;
	KeonFS_MountNode* ramfs_ptr = nullptr;

	if (rd_phys != 0) 
	{
		size_t rd_pages = (rd_size + 4095) / 4096;
		ramdisk_vaddr = (void*)0xE0000000;

		for(size_t i = 0; i < rd_pages; i++)
		{
			paging_map_page(
				(void*)((uintptr_t)ramdisk_vaddr + (i * PAGE_SIZE)), 
				(void*)(rd_phys + (i * PAGE_SIZE)), 
				PTE_PRESENT, 
				true
        	);
    	}

		KeonFS_Info* fs_info = (KeonFS_Info*)ramdisk_vaddr;
		if (fs_info->magic != KEONFS_MAGIC)
			panic(KernelError::K_ERR_RAMFS_MAGIC_FAILED, "Ramfs not found at 0xE0000000");
	}

	vfs_init();
	if (ramdisk_vaddr != nullptr) 
	{
		ramfs_ptr = new KeonFS_MountNode("initrd", ramdisk_vaddr);
		KeonFS_FileHeader* hdr = (KeonFS_FileHeader*)((uintptr_t)ramfs_ptr->base + sizeof(KeonFS_Info));

		if (ramfs_ptr->info->magic != KEONFS_MAGIC)
        	panic(KernelError::K_ERR_RAMFS_MAGIC_FAILED, "Magic corrupt after constructor!");
    	
		uint32_t* magic_ptr = (uint32_t*)ramdisk_vaddr;
		vfs_mount(ramfs_ptr);
	}
		

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
	printf("[*] Initialized KeonFS (ramfs)\n");
	
	// Display detected RAM size
	if (info->flags & (1 << 0))
		printf("Memory found: %d MB\n\n\n\n", (total_mem_bytes / 1024) / 1024);



	FILE* f = fopen("/initrd/test.txt", "r");
	if (!f) printf("[ERROR] fopen failed! Check file path or mount.\n");
	else 
	{
		char c;
		printf("Contents of /initd/test.txt: ");
		while (fread(&c, 1, 1, f) == 1) 
			putchar(c);
		
		printf("\n");

		fclose(f);
	}
	
	// 7. Launch First User Process
    // Initialize the shell and add it as the primary thread to the scheduler
	shell_init();
	thread_add(shell_run, "shell");

	// 8. Idle Loop
    // The main kernel thread enters a low-power halt loop
    // Context will be switched to the shell and other tasks by the timer interrupt
	while (1) asm volatile("hlt");
	
}