#include <kernel/constants.h>
#include <kernel/kernel.h>
#include <kernel/shell.h>

#include <mm/paging.h>
#include <mm/heap.h>
#include <mm/vmm.h>

#include <proc/thread.h>

#include <fs/ramfs.h>
#include <fs/ramfs_vfs.h>
#include <fs/vfs.h>
#include <fs/vfs_node.h>

#include <drivers/keyboard.h>
#include <drivers/timer.h>
#include <drivers/vga.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*
 * keonOS Interactive Shell
 * This module handles user input, command parsing, and system interaction. Kernel Only, tho.
 */


static char input_buffer[SHELL_BUFFER_SIZE];
static int buffer_pos = 0;

// List of supported commands for the Tab-completion system
static const char* command_list[] = 
{
    "help", "clear", "echo", "info", "testheap", "meminfo", 
    "reboot", "halt", "paginginfo", "testpaging", "memstat", "dump",
	"uptime", "ps", "pkill", "ls", "cat"
};
#define COMMAND_COUNT (sizeof(command_list) / sizeof(char*))


// Initializes the shell buffer and state
void shell_init()
{
	buffer_pos = 0;
	memset(input_buffer, 0, SHELL_BUFFER_SIZE);
}


// Displays the shell prompt with branding
static void shell_prompt()
{
    terminal_putchar('\n');
	terminal_setcolor(vga_color_t(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
	printf("keonOS");

	terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
	printf("> ");
}


/*
 * cmd_help: Displays available commands.
 * Supports '--dev' flag for advanced debugging tools.
 */
static void cmd_help(const char* args)
{
	bool is_dev = false;

	if (args && args[0] != '\0')
		if (strcmp(args, "--dev") == 0)
			is_dev = true;
	
	if (is_dev) 
	{
        printf("Developer commands:\n");
        printf("  testheap   - Test heap allocation\n");
        printf("  testpaging - Test paging functionality\n");
        printf("  paginginfo - Show paging statistics\n\n");
        printf("  dump       - Dumps a memory address\n");
    } 
	
	else 
	{
        printf("Available commands:\n");
        printf("  help     - Show this help message\n");
        printf("  clear    - Clear the screen\n");
        printf("  echo     - Echo text to screen\n");
        printf("  info     - Show system information\n");
        printf("  meminfo  - Show memory information\n");
        printf("  reboot   - Reboot the system\n");
        printf("  halt     - Halt the system\n");
        printf("  uptime   - Shows system uptime\n");
        printf("  ps       - Shows system processes\n");
        printf("  pkill    - Kills an active process\n");
        printf("  ls       - Lists the contents of a directory\n");
        printf("  cat      - Displays the content of a file\n");
        printf("\nType 'help --dev' for developer commands\n");
	}
}


/*
 * cmd_clear: Clears the screen.
 * Uses the VGA driver to clear the screen.
 */
static void cmd_clear()
{
	terminal_clear();
}

/*
 * cmd_echo: Writes a custom message to the screen.
 * Uses the printf (libc) function as __is_klib.
 */
static void cmd_echo(const char* args)
{
    if (args && args[0] != '\0') printf("%s\n", args);
    else printf("\n");
}


/**
 * cmd_info: Displays the user the system info
 */
static void cmd_info()
{
    terminal_setcolor(vga_color_t(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
    printf("\n%s\n", OS_VERSION_STRING);
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    printf("A x86 operating system written in C++ and Assembly\n");
    printf("Memory: Paging enabled, Kernel Heap active\n");
    printf("Bootloader: GRUB\n");
}

/**
 * cmd_testheap: Stress tests the Kernel Heap allocator
 * Verifies kmalloc, kfree, and C++ new/delete operators.
 */
static void cmd_testheap()
{
	printf("Testing heap allocation...\n\n");   
    
    void* ptr1 = kmalloc(64);
    if (ptr1) printf("  [+] Allocated 64 bytes\n");
    else 
	{
        printf("  [-] Allocation failed!\n");
        return;
    }
    
    void* ptr2 = kmalloc(128);
    void* ptr3 = kmalloc(256);
    if (ptr2 && ptr3) printf("  [+] Multiple allocations successful\n");
    else printf("  [-] Multiple allocations failed!\n");
    
    kfree(ptr2);
    void* ptr4 = kmalloc(128);
    if (ptr4) printf("  [+] Free and reallocate successful\n");
    else printf("  [-] Free and reallocate failed!\n");
    
    int* arr = new int[10];
    if (arr) 
	{
        printf("  [+] C++ new operator works\n");
        delete[] arr;
        printf("  [+] C++ delete operator works\n");
    } 
	else printf("  [-] C++ new operator failed!\n");
    
    kfree(ptr1);
    kfree(ptr3);
    kfree(ptr4);
    
    terminal_setcolor(vga_color_t(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    printf("\nHeap test completed!");
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}


/**
 * cmd_meminfo: Displays the user the kheap info
 */
static void cmd_meminfo() 
{
    struct heap_stats stats;
    kheap_get_stats(&stats);
    
    printf("\n --- Kernel Heap Information --- \n");

    printf("Heap Start: 0x%x\n", (uint32_t)get_kheap_start());
    printf("Total Size: %d KB\n", (int)(stats.total_size / 1024));

    int used_kb = (int)(stats.used_size / 1024);
    int used_pct = (stats.total_size > 0) ? (int)((stats.used_size * 100) / stats.total_size) : 0;
    printf("Used:       %d KB (%d%%)\n", used_kb, used_pct);
    
    int free_kb = (int)(stats.free_size / 1024);
    int free_pct = (stats.total_size > 0) ? (int)((stats.free_size * 100) / stats.total_size) : 0;
    printf("Free:       %d KB (%d%%)\n", free_kb, free_pct);

    printf("Blocks:     Total: %d, Free: %d\n", (int)stats.block_count, (int)stats.free_block_count);
    
    printf("-------------------------------\n");
}

/**
 * cmd_reboot: Reboots the system
 */
static void cmd_reboot() 
{
    printf("Rebooting system...");
    asm volatile("cli");
    
    asm volatile(
        "movb $0xFE, %%al\n\t"
        "outb %%al, $0x64"
        : : : "al", "memory"
    );
    
    for (volatile int i = 0; i < 1000000; i++);
    
    asm volatile(
        "movb $0x06, %%al\n\t"
        "outb %%al, $0xCF9"
        : : : "al", "memory"
    );
    
    struct {
        uint16_t limit;
        uint32_t base;
    } invalid_idt = {0, 0};
    
    asm volatile("lidt %0" : : "m" (invalid_idt));
    asm volatile("int $0x00");
    
    printf("Reboot failed! System halted.");
    while (1) asm volatile("hlt");
}

/**
 * cmd_halt: Halts the system
 */
static void cmd_halt() 
{
    printf("Halting system...\n");
    printf("System halted. Press Ctrl+Alt+Del to restart.");
    while (1) asm volatile("hlt");
}

/**
 * cmd_pkill: Kills a process
 */

static void cmd_pkill(const char* args) 
{
    if (!args || args[0] == '\0') 
    {
        printf("Usage: pkill <thread_id>\n");
        return;
    }

    uint32_t id;

    if (args[0] >= '0' && args[0] <= '9') 
        id = (uint32_t)atoi(args);

    else 
    {
        id = thread_get_id_by_name(args);
        if (id == THREAD_NOT_FOUND) 
        {
            terminal_setcolor(vga_color_t(VGA_COLOR_RED, VGA_COLOR_BLACK));
            printf("Error: No thread named '%s' found.\n", args);
            terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            return;
        }

        if (id == THREAD_AMBIGUOUS) 
        {
            terminal_setcolor(vga_color_t(VGA_COLOR_RED, VGA_COLOR_BLACK));
            printf("Error: Multiple threads named '%s' found.\n", args);
            terminal_setcolor(vga_color_t(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
            printf("Please use pkill <ID> instead. (Check 'ps' for IDs)\n");
            terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            return;
        }
    }
   
    if (thread_kill(id)) printf("Thread '%s' (ID: %d) terminated.\n", args, id);
    else 
    {
        terminal_setcolor(vga_color_t(VGA_COLOR_RED, VGA_COLOR_BLACK));
        printf("Error: Could not kill thread %d. (Is it a system thread?)\n", id);
        terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    }
}

/**
 * cmd_paginginfo: Displays to the user the paging info
 */
static void cmd_paginginfo() 
{
    struct paging_stats stats;
    paging_get_stats(&stats);
    
    printf("\n--- Paging & Physical Memory ---\n");

    uint32_t total_mb = (stats.total_frames * 4) / 1024;
    printf("Total Frames:  %d (%d MB)\n", (int)stats.total_frames, (int)total_mb);

    int used_pct = (stats.total_frames > 0) ? (int)((stats.used_frames * 100) / stats.total_frames) : 0;
    printf("Used Frames:   %d (%d%%)\n", (int)stats.used_frames, used_pct);

    int free_pct = (stats.total_frames > 0) ? (int)((stats.free_frames * 100) / stats.total_frames) : 0;
    printf("Free Frames:   %d (%d%%)\n", (int)stats.free_frames, free_pct);

    printf("Mapped Pages:  %d\n", (int)stats.mapped_pages);
    
    printf("--------------------------------\n");
}

/**
 * cmd_testpaging: Performs a test to check if the paging is working or not
 */
static void cmd_testpaging() 
{
    printf("\n--- Testing Paging Functionality ---\n");
    
    printf(" [1] Allocating physical frame... ");
    void* frame = pfa_alloc_frame();
    if (frame == NULL) 
    {
        printf("FAILED - Out of memory\n");
        return;
    }
    printf("OK (Phys: 0x%x)\n", (uint32_t)frame);
    
    void* test_vaddr = (void*)0xE0000000;
    printf(" [2] Mapping virtual page 0x%x... ", (uint32_t)test_vaddr);
    paging_map_page(test_vaddr, frame, PTE_PRESENT | PTE_RW, true);
    printf("OK\n");        
    
    printf(" [3] Verification: ");
    void* phys = paging_get_physical_address(test_vaddr);
    if (phys == frame) 
        printf("MATCH (0x%x == 0x%x)\n", (uint32_t)phys, (uint32_t)frame);

    else
    {
        terminal_setcolor(vga_color_t(VGA_COLOR_RED, VGA_COLOR_BLACK));
        printf("FAILED (Got 0x%x)\n", (uint32_t)phys);
        terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    }
    
    printf(" [4] Unmapping virtual page... ");
    paging_unmap_page(test_vaddr);
    printf("OK\n");
    
    printf(" [5] Freeing physical frame... ");
    pfa_free_frame(frame);
    printf("OK\n");
    
    terminal_setcolor(vga_color_t(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    printf("\n[SUCCESS] Paging test completed!\n");
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

/**
 * cmd_memstat: Displays to the user the memory stats
 */
static void cmd_memstat()
{
    struct paging_stats p_stats;
    paging_get_stats(&p_stats);
    size_t virt_heap = VMM::get_total_allocated();

    printf("\n --- keonOS Memory Statistics ---\n");

    printf("Physical RAM:\n");
    printf("    Total: %u KB (%u MB)\n", (p_stats.total_frames * 4), (p_stats.total_frames * 4) / 1024);
    printf("    Used: %u KB\n", (p_stats.used_frames * 4));
    printf("    Free: %u KB\n", (p_stats.free_frames * 4));

    printf("\nVirtual Kernel Heap:\n");
    printf("    Current Reservation: %u KB\n", virt_heap / 1024);

    struct heap_stats h_stats;
    kheap_get_stats(&h_stats);
    printf("    Heap Block: %d\n", h_stats.block_count);
    printf("    Heap Free Space: %u KB\n", h_stats.free_size / 1024);
    printf("--------------------------------\n\n");
}

/**
 * cmd_dump: Dumps the requested system/memory address
 */
static void cmd_dump(const char* args) 
{
    uintptr_t addr = strtoul(args, NULL, 16); 
    uint8_t* ptr = (uint8_t*)addr;

    printf("Dumping memory at 0x%x:\n", addr);
    for (int i = 0; i < 64; i++) 
    {
        if (i % 16 == 0 && i != 0) printf("\n");
        printf("%02x ", ptr[i]);
    }
    printf("\n");
}

/**
 * cmd_ls: Displays the contents of a directory
 */
static void cmd_ls(const char* args) 
{
    char path[256];

    if (!args || args[0] == '\0') strcpy(path, "/");
        
    else if (args[0] != '/') 
    {
        path[0] = '/';
        strcpy(path + 1, args);
    } 

    else strcpy(path, args);
        

    VFSNode* dir = vfs_open(path);
    
    if (dir) 
    {
        if (dir->type == VFS_DIRECTORY) 
        {
            uint32_t i = 0;
            vfs_dirent* de;

            while ((de = vfs_readdir(dir, i++)))
                printf("%s  ", de->name);
            
            printf("\n");
        } 

        else printf("ls: '%s' is not a directory\n", path);
        vfs_close(dir);
    } 

    else printf("ls: cannot access '%s': No such directory\n", path);
    
}


/**
 * cmd_ls: Displays the contents of a file
 */
static void cmd_cat(const char* args) 
{
    char path[256];
    if (!args || args[0] == '\0') 
    {
        printf("Usage: cat <filename>\n");
        return;
    }

    if (args[0] != '/') 
    {
        path[0] = '/';
        strcpy(path + 1, args);
    } 
    else strcpy(path, args);
    
    VFSNode* file = vfs_open(path);
    if (file) 
    {
        if (file->type == VFS_FILE) 
        {
            uint8_t buffer[513];
            uint32_t offset = 0;
            uint32_t bytes_read;

            while ((bytes_read = vfs_read(file, offset, 512, buffer)) > 0) 
            {
                buffer[bytes_read] = '\0';
                printf("%s", (char*)buffer);
                offset += bytes_read;
            }
            printf("\n");
        }
        else printf("cat: %s: Is a directory\n", path);
        vfs_close(file);
    }

    else printf("cat: %s: No such file or directory\n", path);
}

/**
 * shell_tab_completion: Provides basic command autocompletion
 * Matches current buffer against the command_list.
 */
static void shell_tab_completion() 
{
    if (buffer_pos == 0) return;

    const char* match = nullptr;
    int matches_found = 0;

    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        if (strncmp(input_buffer, command_list[i], buffer_pos) == 0) {
            match = command_list[i];
            matches_found++;
        }
    }

    if (matches_found == 1)
    {
        const char* remaining = match + buffer_pos;
        while (*remaining)
        {
            input_buffer[buffer_pos++] = *remaining;
            printf("%c", *remaining);
            remaining++;
        }
    }

    else if (matches_found > 1) 
    {
        printf("\n");
        for (size_t i = 0; i < COMMAND_COUNT; i++) 
            if (strncmp(input_buffer, command_list[i], buffer_pos) == 0)
                printf("%s  ", command_list[i]);
        
        shell_prompt();
        printf("%s", input_buffer);
    }
}

/**
 * cmd_uptime: Displays to the user the system uptime
 */
static void cmd_uptime() 
{
    uint32_t ticks = timer_get_ticks();
    uint32_t seconds = ticks / 100;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;

    printf("System uptime: %d hours, %d minutes, %d seconds (%d ticks)\n", 
            hours, minutes % 60, seconds % 60, ticks);
}

/**
 * cmd_ps: Displays to the user all the available/running processes
 */
static void cmd_ps() 
{ 
    thread_print_list(); 
}


/**
 * shell_execute: Parses the input string and calls the corresponding function
 */
void shell_execute(const char* command) 
{
    
    while (*command == ' ') command++;
    if (strlen(command) == 0) return; 
    
    char cmd[32];
    const char* args = command;
    int i = 0;
    
    while (*args && *args != ' ' && i < 31) 
        cmd[i++] = *args++;

    cmd[i] = '\0';
    
    
    while (*args == ' ') args++;

    if (strcmp(cmd, "help") == 0)		 		cmd_help(args);
    else if (strcmp(cmd, "clear") == 0) 		cmd_clear();
    else if (strcmp(cmd, "echo") == 0) 			cmd_echo(args);
	else if (strcmp(cmd, "info") == 0) 			cmd_info();
    else if (strcmp(cmd, "testheap") == 0) 		cmd_testheap();
    else if (strcmp(cmd, "meminfo") == 0) 		cmd_meminfo();
    else if (strcmp(cmd, "reboot") == 0) 		cmd_reboot();
    else if (strcmp(cmd, "halt") == 0) 			cmd_halt();
    else if (strcmp(cmd, "paginginfo") == 0) 	cmd_paginginfo();
	else if (strcmp(cmd, "testpaging") == 0) 	cmd_testpaging();
    else if (strcmp(cmd, "memstat") == 0)       cmd_memstat();
    else if (strcmp(cmd, "dump") == 0)          cmd_dump(args);
    else if (strcmp(cmd, "uptime") == 0)        cmd_uptime();
    else if (strcmp(cmd, "ps") == 0)            cmd_ps();
    else if (strcmp(cmd, "pkill") == 0)         cmd_pkill(args);
    else if (strcmp(cmd, "ls") == 0)            cmd_ls(args);
    else if (strcmp(cmd, "cat") == 0)            cmd_cat(args);

	else printf("Unknown command: %s\nType 'help' for available commands", command);
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}


/**
 * shell_run: Main shell loop
 * Handles keyboard input, history (Up/Down arrows), and backspace.
 */
void shell_run() 
{
    shell_prompt();
    
    static char history[MAX_HISTORY][SHELL_BUFFER_SIZE];
    static int history_count = 0;
    static int history_index = -1;
    
    
    while (true) 
	{
        char c = keyboard_getchar();
        if (c == 0) continue;
        
        if (c == '\n') 
		{    
            printf("\n");
            input_buffer[buffer_pos] = '\0';
            
            if (buffer_pos > 0) 
            {
                for (int i = MAX_HISTORY - 1; i > 0; i--) 
                strcpy(history[i], history[i-1]);
                
                strcpy(history[0], input_buffer);
                if (history_count < MAX_HISTORY) history_count++;
            }
            
            shell_execute(input_buffer);
            
            buffer_pos = 0;
            history_index = -1;
            memset(input_buffer, 0, SHELL_BUFFER_SIZE);
            shell_prompt();
        }

		else if (c == '\b') 
		{    
            if (buffer_pos > 0) 
			{
                buffer_pos--;
                input_buffer[buffer_pos] = '\0';
                terminal_putchar('\b');
            }
        } 

        else if (c == '\t') shell_tab_completion();
		
		else if (buffer_pos < SHELL_BUFFER_SIZE - 1 && c >= 32 && c <= 126) 
        {
            input_buffer[buffer_pos++] = c;
            printf("%c", c);
        }

       else if (c == KEY_UP) 
        {
            if (history_count > 0 && history_index < history_count - 1) 
            {
                history_index++;
                while (buffer_pos > 0) { terminal_putchar('\b'); buffer_pos--; }
                
                strcpy(input_buffer, history[history_index]);
                buffer_pos = strlen(input_buffer);
                printf("%s", input_buffer);
            }
        }

        else if (c == KEY_DOWN) 
        {
            if (history_index > 0) 
            {
                history_index--;
                while (buffer_pos > 0) { terminal_putchar('\b'); buffer_pos--; }
                
                strcpy(input_buffer, history[history_index]);
                buffer_pos = strlen(input_buffer);
                printf("%s", input_buffer);
            }

            else if (history_index == 0) 
            {
                history_index = -1;
                while (buffer_pos > 0) { terminal_putchar('\b'); buffer_pos--; }
                memset(input_buffer, 0, SHELL_BUFFER_SIZE);
            }
        }
    }
}