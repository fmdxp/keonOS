/*
 * keonOS - kernel/kernel.cpp
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

#define KERNEL_VIRTUAL_OFFSET 0xFFFFFFFF80000000

#include <kernel/constants.h>
#include <kernel/kernel.h>
#include <kernel/shell.h>
#include <kernel/error.h>
#include <kernel/panic.h>

#include <kernel/arch/x86_64/constructor.h>
#include <kernel/arch/x86_64/paging.h>
#include <kernel/arch/x86_64/thread.h>
#include <kernel/arch/x86_64/gdt.h>
#include <kernel/arch/x86_64/idt.h>

#include <mm/vmm.h>
#include <mm/heap.h>

#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <fs/vfs_node.h>
#include <fs/fat32_vfs.h>
#include <fs/ramfs_vfs.h>
#include <fs/fat32_structs.h>

#include <drivers/vga.h>
#include <drivers/ata.h>
#include <drivers/timer.h>
#include <drivers/serial.h>
#include <drivers/speaker.h>
#include <drivers/keyboard.h>
#include <drivers/multiboot2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" char _kernel_end;

void init_file_system(void* ramdisk_vaddr) 
{
    vfs_init();
    RootFS* root = (RootFS*)vfs_root;

    SimpleDirectory* dev_dir = new SimpleDirectory("dev");
    root->register_node(dev_dir);
    
    DeviceNode* disk = new DeviceNode("keon_disk");
    dev_dir->size = 64 * 1024 * 1024;
    dev_dir->add_child(disk);

    SimpleDirectory* mnt_dir = new SimpleDirectory("mnt");
    root->register_node(mnt_dir);

    uint32_t fat_lba = fat32_inst.find_fat32_partition();
    if (fat_lba != 0) 
    {
        fat32_inst.init(fat_lba);
        
        FAT32_Directory* fat_root = new FAT32_Directory("fat32", 
                                       fat32_inst.bpb.root_cluster, 
                                       &fat32_inst.bpb);
        
        mnt_dir->add_child(fat_root);
        printf("[FAT32] Hardware partition mounted in /mnt/fat32\n");
    }

    if (ramdisk_vaddr != nullptr) 
    {
        KeonFS_MountNode* ramfs_ptr = new KeonFS_MountNode("initrd", ramdisk_vaddr);
        root->register_node(ramfs_ptr);
        printf("[RAMFS] Ramdisk loaded in /initrd\n");
    }
}

extern "C" void kernel_main([[maybe_unused]] uint64_t magic, uint64_t multiboot_phys_addr)
{
	uintptr_t multiboot_virt_addr = multiboot_phys_addr + KERNEL_VIRTUAL_OFFSET;

	// 1. Core Architecture Setup
	initialize_constructors();
	terminal_initialize();
	serial_install();
	gdt_init();
	idt_init();

	uint64_t total_mem_bytes = 0;
    uintptr_t rd_phys = 0;
    uint32_t rd_size = 0;
	uint32_t mods_count = 0;

	multiboot_tag *tag;
    for (tag = (multiboot_tag*)(multiboot_virt_addr + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))) 
    {
        switch (tag->type) 
		{
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: 
			{
                auto mem_tag = (multiboot_tag_basic_meminfo *)tag;
                total_mem_bytes = (mem_tag->mem_lower + mem_tag->mem_upper) * 1024;
                break;
            }
            case MULTIBOOT_TAG_TYPE_MODULE: 
            {
                auto mod_tag = (multiboot_tag_module *)tag;
                rd_phys = mod_tag->mod_start;
                rd_size = mod_tag->mod_end - mod_tag->mod_start;
				mods_count++;
                break;
            }
        }
    }


	// 2. Memory Management Setup
    // Disable interrupts during critical memory initialization
	asm volatile("cli");
	
	pfa_init_from_multiboot2((void*)multiboot_virt_addr);
	paging_init();	

	VMM::kernel_dynamic_break = ((uintptr_t)&_kernel_end + 0xFFF) & ~0xFFF;
	VMM::kernel_dynamic_break += 0x10000;
	
	printf("[VMM] Kernel end: %p, Heap start: %p\n", &_kernel_end, VMM::kernel_dynamic_break);
	void* heap_start = (void*)VMM::kernel_dynamic_break;
    uintptr_t initial_heap_size = 4 * 1024 * 1024;
		
	VMM::sbrk(initial_heap_size);
    kheap_init(heap_start, initial_heap_size);
    
	
	// 4. Subsystem Initialization
	timer_init(100);
	thread_init();
	keyboard_init();


	void* ramdisk_vaddr = nullptr;
	if (rd_phys != 0) 
    {
        size_t rd_pages = (rd_size + 4095) / 4096;
        ramdisk_vaddr = (void*)0xFFFFFFFFE0000000;

        for(size_t i = 0; i < rd_pages; i++)
        {
            paging_map_page(
                (void*)((uintptr_t)ramdisk_vaddr + (i * PAGE_SIZE)), 
                (void*)(rd_phys + (i * PAGE_SIZE)), 
                PTE_PRESENT | PTE_RW
            );
        }

        KeonFS_Info* fs_info = (KeonFS_Info*)ramdisk_vaddr;
        if (fs_info->magic != KEONFS_MAGIC)
            panic(KernelError::K_ERR_RAMFS_MAGIC_FAILED, "RAMFS not found or wrong magic");
    }

	init_file_system(ramdisk_vaddr);
		

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
	
	timer_sleep(500);
	terminal_clear();

	printf("\n				-- keonOS --\n\n\n");
	terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

	
	// Display detected RAM size
	if (total_mem_bytes)
		printf("Memory found: %llu MB\n\n", (total_mem_bytes / 1024) / 1024);


	// Show the user the OS's copyrights
	printf("keonOS version %s (alpha)\n", OS_VERSION_STRING_NO_NAME);
	printf("Copyright (C) 2026 fmdxp. Licensed under a custom GNU GPLv3.\n");
	printf("This program comes with ABSOLUTELY NO WARRANTY.\n\n");
	printf("Type 'help' for commands.\n");


	// 7. Launch First User Process
    // Initialize the shell and add it as the primary thread to the scheduler
	shell_init();
	thread_add(shell_run, "shell");

	// 8. Idle Loop
    // The main kernel thread enters a low-power halt loop
    // Context will be switched to the shell and other tasks by the timer interrupt
	while (1) asm volatile("hlt");
	
}