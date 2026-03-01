/*
 * keonOS - kernel/shell.cpp
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


#include <kernel/arch/x86_64/paging.h>
#include <kernel/arch/x86_64/thread.h>

#include <kernel/constants.h>
#include <kernel/kernel.h>
#include <kernel/shell.h>

#include <mm/heap.h>
#include <mm/vmm.h>

#include <fs/ramfs.h>
#include <fs/ramfs_vfs.h>
#include <fs/vfs.h>
#include <fs/vfs_node.h>

#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <drivers/timer.h>
#include <drivers/vga.h>
#include <exec/kex_loader.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <sys/syscall.h>
#include <sys/errno.h>

static bool is_user_mode() 
{
    uint16_t cs;
    asm volatile ("mov %%cs, %0" : "=r"(cs));
    return (cs & 0x3) != 0;
}

static void shell_setcolor(vga_color_t color)
{
    if (is_user_mode()) 
    {
        syscall(SYS_VGA, 1, color.fg, color.bg, 0, 0, 0); // 1 = set color
    }
    else 
    {
#if defined(__is_libk)
        terminal_setcolor(color);
#endif
    }
}

static void shell_clear()
{
    if (is_user_mode()) 
    {
        syscall(SYS_VGA, 0, 0, 0, 0, 0, 0); // 0 = clear
    }
    else 
    {
#if defined(__is_libk)
        terminal_clear();
#endif
    }
}

/*
 * keonOS Interactive Shell
 * This module handles user input, command parsing, and system interaction. Kernel Only, tho.
 */


static char input_buffer[SHELL_BUFFER_SIZE];
static int buffer_pos = 0;
static char current_working_directory[256] = "/";
extern VFSNode* cwd_node;
static char history[MAX_HISTORY][SHELL_BUFFER_SIZE];
static int history_count = 0;
static int history_index = -1;

// List of supported commands for the Tab-completion system
static const char* command_list[] = 
{
    "help", "clear", "echo", "info", "testheap", "meminfo", 
    "reboot", "halt", "paginginfo", "testpaging", "memstat", "dump",
	"uptime", "ps", "pkill", "ls", "cat", "cd", "mkdir", "touch", "rm",
    "sleep", "pid", "stat"
};
#define COMMAND_COUNT (sizeof(command_list) / sizeof(char*))


// Initializes the shell buffer and state
void shell_init()
{
    buffer_pos = 0;
    memset(input_buffer, 0, SHELL_BUFFER_SIZE);
    history_count = 0;
    history_index = -1;
    memset(history, 0, sizeof(history));
}


// Displays the shell prompt with branding
static void shell_prompt()
{
    putchar('\n');
	shell_setcolor(vga_color_t(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
	printf("root@keonOS");
    shell_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    printf(":");
    shell_setcolor(vga_color_t(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    printf("%s", current_working_directory);
	shell_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
	printf("$ ");
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
        shell_setcolor(vga_color_t(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        printf("\n--- Developer & Debugging Commands ---\n");
        shell_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        
        printf("  testheap   - Stress test the kernel heap allocator\n");
        printf("  testpaging - Verify virtual memory mapping/unmapping\n");
        printf("  paginginfo - Display physical frame and page table stats\n");
        printf("  memstat    - Detailed summary of physical and virtual memory\n");
        printf("  dump <hex> - Hexdump 64 bytes starting from memory address\n");
        printf("\n");
    } 
    else 
    {
        shell_setcolor(vga_color_t(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        printf("\n--- keonOS Available Commands ---\n");
        shell_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

        printf("  help       - Show this help message (use --dev for more)\n");
        printf("  info       - Show OS version and system branding\n");
        printf("  uptime     - Display time elapsed since boot\n");
        printf("  clear      - Clear the terminal screen\n");
        printf("  reboot     - Perform a cold system restart\n");
        printf("  halt       - Stop all CPU execution safely\n\n");

        printf("  ps         - List all active kernel threads/processes\n");
        printf("  pkill <id> - Terminate a thread by ID or Name\n\n");

        printf("  ls <path>  - List directory contents\n");
        printf("  cd <path>  - Change current working directory\n");
        printf("  cat <file> - Print file contents to standard output\n");
        printf("  touch <f>  - Create a new empty file\n");
        printf("  rm <f>     - Deletes a file\n");
        printf("  mkdir <d>  - Create a new directory\n");
        printf("  echo <msg> - Print text or arguments to screen\n");

        printf("  sleep <ms> - Sleep for milliseconds\n");
        printf("  pid        - Show current Process ID\n");
        printf("  stat <f>   - Show file statistics\n");

        printf("\nTip: Press [TAB] for autocompletion and [UP/DOWN] for history.\n");
    }
}



static void resolve_path(char* out, const char* arg) 
{
    if (arg[0] == '/') strncpy(out, arg, 511);
    else 
    {
        strncpy(out, current_working_directory, 511);
        int len = strlen(out);
        if (len > 0 && out[len-1] != '/') strcat(out, "/");
        strcat(out, arg);
    }
}


/*
 * cmd_clear: Clears the screen.
 * Uses the VGA driver to clear the screen.
 */
static void cmd_clear()
{
	shell_clear();
}

/*
 * cmd_echo: Writes a custom message to the screen.
 * Uses the printf (libc) function as __is_klib.
 */
static void cmd_echo(const char* args) 
{
    if (!args || args[0] == '\0') {
        printf("\n");
        return;
    }

    char message[256];
    char filename[128];
    bool redirect = false;

    const char* gt = strchr(args, '>');
    if (gt) 
    {
        redirect = true;
        size_t msg_len = gt - args;
        if (msg_len > 255) msg_len = 255;
        strncpy(message, args, msg_len);
        message[msg_len] = '\0';
        
        char* end = message + strlen(message) - 1;
        while(end > message && isspace(*end)) *end-- = '\0';

        const char* file_part = gt + 1;
        while(*file_part && isspace(*file_part)) file_part++;
        strncpy(filename, file_part, 127);
        filename[127] = '\0';
    }
    else 
    {
        strncpy(message, args, 255);
        message[255] = '\0';
    }

    if (redirect) 
    {
        char full_path[512];
        resolve_path(full_path, filename);

#if defined(__is_libk)
        if (is_user_mode()) 
        {
            int fd = syscall(SYS_OPEN, (uint64_t)full_path, 1, 0, 0, 0, 0); // O_CREAT=1
            if (fd >= 0) 
            {
                syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)message, strlen(message), 0, 0, 0);
                syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)"\n", 1, 0, 0, 0);
                syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
            }
            else printf("echo: cannot write to %s\n", filename);
            return;
        }

        VFSNode* n = vfs_open(full_path);
        if (!n) n = vfs_create(full_path, 0);

        if (n) 
        {
            vfs_write(n, n->size, strlen(message), (uint8_t*)message);
            vfs_write(n, n->size, 1, (uint8_t*)"\n");
            vfs_close(n);
        } 
        else printf("echo: error while creating file %s\n", filename);
#else
        int fd = syscall(SYS_OPEN, (uint64_t)full_path, 1, 0, 0, 0, 0); // O_CREAT=1
        if (fd >= 0) 
        {
            syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)message, strlen(message), 0, 0, 0);
            syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)"\n", 1, 0, 0, 0);
            syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
        }
        else printf("echo: cannot write to %s\n", filename);
#endif
    }
    else printf("%s\n", message);
}


/**
 * cmd_info: Displays the user the system info
 */
static void cmd_info()
{
    shell_setcolor(vga_color_t(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
    printf("\n%s\n", OS_VERSION_STRING);
    shell_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
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
    
    shell_setcolor(vga_color_t(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    printf("\nHeap test completed!");
    shell_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}


/**
 * cmd_meminfo: Displays the user the kheap info
 */
static void cmd_meminfo() 
{
    struct heap_stats stats;
    kheap_get_stats(&stats);
    
    printf("\n --- Kernel Heap Information --- \n");

    printf("Heap Start: 0x%x\n", get_kheap_start());
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
    if (is_user_mode()) 
    {
        syscall(SYS_REBOOT, 0, 0, 0, 0, 0, 0);
    }
    else 
    {
#if defined(__is_libk)
        asm volatile("cli");
        outb(0x64, 0xFE);
        for (volatile int i = 0; i < 1000000; i++);
        outb(0xCF9, 0x06);
        struct { uint16_t limit; uint32_t base; } invalid_idt = {0, 0};
        asm volatile("lidt %0" : : "m" (invalid_idt));
        asm volatile("int $0x00");
#else
        syscall(SYS_REBOOT, 0, 0, 0, 0, 0, 0);
#endif
    }
    printf("Reboot failed! System halted.");
    while (1) asm volatile("hlt");
}

/**
 * cmd_halt: Halts the system
 */
static void cmd_halt() 
{
    printf("Halting system...\n");
    if (is_user_mode()) 
    {
        syscall(SYS_EXIT, 0, 0, 0, 0, 0, 0);
    }
    else 
    {
        printf("System halted. Press Ctrl+Alt+Del to restart.");
        while (1) asm volatile("hlt");
    }
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

    uint32_t id = (uint32_t)atoi(args);

    if (is_user_mode()) 
    {
        if (syscall(SYS_KILL, (uint64_t)id, 0, 0, 0, 0, 0) == 0) printf("Thread %d terminated.\n", id);
        else printf("Error: Could not kill thread %d.\n", id);
        return;
    }

#if defined(__is_libk)
    if (args[0] < '0' || args[0] > '9') 
    {
        id = thread_get_id_by_name(args);
        if (id == THREAD_NOT_FOUND) { printf("Error: No thread named '%s' found.\n", args); return; }
        if (id == THREAD_AMBIGUOUS) { printf("Error: Multiple threads named '%s' found.\n", args); return; }
    }
   
    if (thread_kill(id)) printf("Thread %d terminated.\n", id);
    else printf("Error: Could not kill thread %d.\n", id);
#endif
}

/**
 * cmd_sleep: Sleeps for a given amount of milliseconds
 */
static void cmd_sleep(const char* args) 
{
    if (!args || args[0] == '\0') 
    {
        printf("Usage: sleep <ms>\n");
        return;
    }

    uint32_t ms = (uint32_t)atoi(args);

    if (is_user_mode()) 
    {
        syscall(SYS_SLEEP, (uint64_t)ms, 0, 0, 0, 0, 0);
    }
    else 
    {
#if defined(__is_libk)
        thread_sleep(ms);
#endif
    }
    printf("Slept for %d ms.\n", ms);
}

/**
 * cmd_pid: Displays the current process ID
 */
static void cmd_pid() 
{
    if (is_user_mode()) 
    {
        uint64_t pid = syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0);
        printf("Current PID: %d\n", (int)pid);
    }
    else 
    {
#if defined(__is_libk)
        thread_t* current = thread_get_current();
        if (current) printf("Current PID: %d (Kernel Thread)\n", current->id);
#endif
    }
}

/**
 * cmd_stat: Displays file statistics
 */
static void cmd_stat(const char* args) 
{
    if (!args || args[0] == '\0') 
    {
        printf("Usage: stat <path>\n");
        return;
    }

    char full_path[512];
    resolve_path(full_path, args);

    struct stat_struct {
        uint64_t st_dev;
        uint64_t st_ino;
        uint32_t st_mode;
        uint32_t st_nlink;
        uint32_t st_uid;
        uint32_t st_gid;
        uint64_t st_rdev;
        uint64_t st_size;
        uint64_t st_blksize;
        uint64_t st_blocks;
        uint64_t st_atime;
        uint64_t st_mtime;
        uint64_t st_ctime;
    } st;

    if (is_user_mode()) 
    {
        if (syscall(SYS_STAT, (uint64_t)full_path, (uint64_t)&st, 0, 0, 0, 0) == 0) 
        {
            printf("File: %s\n", args);
            printf("Size: %ld bytes\n", st.st_size);
            printf("Inode: %ld\n", st.st_ino);
            printf("Mode: %o\n", st.st_mode);
        }
        else printf("stat: cannot stat '%s'\n", args);
    }
    else 
    {
#if defined(__is_libk)
        VFSNode* node = vfs_open(full_path);
        if (node) 
        {
            printf("File: %s\n", args);
            printf("Size: %d bytes\n", node->size);
            printf("Inode: %d\n", node->inode);
            printf("Type: %d\n", node->type);
            vfs_close(node);
        }
        else printf("stat: cannot stat '%s'\n", args);
#endif
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
    printf("OK (Phys: 0x%lx)\n", (uintptr_t)frame);
    
    void* test_vaddr = (void*)0xE0000000;
    printf(" [2] Mapping virtual page 0x%lx... ", (uintptr_t)test_vaddr);
    paging_map_page(test_vaddr, frame, PTE_PRESENT | PTE_RW);
    printf("OK\n");        
    
    printf(" [3] Verification: ");
    void* phys = paging_get_physical_address(test_vaddr);
    if (phys == frame) 
        printf("MATCH (0x%lx == 0x%lx)\n", (uintptr_t)phys, (uintptr_t)frame);

    else
    {
        shell_setcolor(vga_color_t(VGA_COLOR_RED, VGA_COLOR_BLACK));
        printf("FAILED (Got 0x%lx)\n", (uintptr_t)phys);
        shell_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    }
    
    printf(" [4] Unmapping virtual page... ");
    paging_unmap_page(test_vaddr);
    printf("OK\n");
    
    printf(" [5] Freeing physical frame... ");
    pfa_free_frame(frame);
    printf("OK\n");
    
    shell_setcolor(vga_color_t(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    printf("\n[SUCCESS] Paging test completed!\n");
    shell_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
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
#if defined(__is_libk)
    VFSNode* dir = nullptr;
    char path[512];

    if (is_user_mode())
    {
        if (!args || args[0] == '\0') strcpy(path, current_working_directory);
        else resolve_path(path, args);

        int fd = (int)syscall(SYS_OPEN, (uintptr_t)path, 0, 0, 0, 0, 0);
        if (fd < 0) { printf("ls: cannot access '%s'\n", args); return; }

        struct vfs_dirent {
            char name[128];
            uint32_t inode;
            uint32_t type;
        } de;

        int i = 0;
        while (syscall(SYS_READDIR, (uint64_t)fd, (uint64_t)i++, (uint64_t)&de, 0, 0, 0) > 0)
            printf("%s  ", de.name);
        printf("\n");
        syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
        return;
    }

    if (!args || args[0] == '\0') 
    { 
        dir = (cwd_node) ? cwd_node : vfs_root;
    }
    else 
    {
        resolve_path(path, args);
        dir = vfs_open(path);
    }

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
        else printf("%s\n", args);
        if (dir != cwd_node && dir != vfs_root) vfs_close(dir);
    } 
    else printf("ls: cannot access '%s': No such file or directory\n", args);
#else
    char path[512];
    if (!args || args[0] == '\0') strcpy(path, current_working_directory);
    else resolve_path(path, args);

    int fd = (int)syscall(SYS_OPEN, (uintptr_t)path, 0, 0, 0, 0, 0);
    if (fd < 0) { printf("ls: cannot access '%s'\n", args); return; }

    struct vfs_dirent {
        char name[128];
        uint32_t inode;
        uint32_t type;
    } de;

    int i = 0;
    while (syscall(SYS_READDIR, (uint64_t)fd, (uint64_t)i++, (uint64_t)&de, 0, 0, 0) > 0)
        printf("%s  ", de.name);
    printf("\n");
    syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
#endif
}

/**
 * cmd_cat: Displays the contents of a file
 */
static void cmd_cat(const char* args) 
{
    char path[512];
    if (!args || args[0] == '\0') 
    {
        printf("Usage: cat <filename>\n");
        return;
    }

    resolve_path(path, args);

#if defined(__is_libk)
    if (is_user_mode()) 
    {
        int fd = (int)syscall(SYS_OPEN, (uintptr_t)path, 0, 0, 0, 0, 0);
        if (fd >= 0) 
        {
            uint8_t buffer[512];
            int bytes_read;
            while ((bytes_read = (int)syscall(SYS_READ, (uint64_t)fd, (uintptr_t)buffer, 512, 0, 0, 0)) > 0) 
            {
                for (int i = 0; i < bytes_read; i++) putchar(buffer[i]);
            }
            printf("\n");
            syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
        }
        else printf("cat: %s: No such file or directory\n", path);
        return;
    }
    
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
#else
    int fd = (int)syscall(SYS_OPEN, (uintptr_t)path, 0, 0, 0, 0, 0);
    if (fd >= 0) 
    {
        uint8_t buffer[512];
        int bytes_read;
        while ((bytes_read = (int)syscall(SYS_READ, (uint64_t)fd, (uintptr_t)buffer, 512, 0, 0, 0)) > 0) 
        {
            for (int i = 0; i < bytes_read; i++) putchar(buffer[i]);
        }
        printf("\n");
        syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
    }
    else printf("cat: %s: No such file or directory\n", path);
#endif
}

/**
 * shell_tab_completion: Provides basic command autocompletion
 * Matches current buffer against the command_list.
 */
static void shell_tab_completion() 
{
    if (buffer_pos == 0) return;

    input_buffer[buffer_pos] = '\0';
    char* space_ptr = strchr(input_buffer, ' ');
    
    if (space_ptr == NULL) 
    {
        const char* matches[COMMAND_COUNT];
        int matches_found = 0;

        for (size_t i = 0; i < COMMAND_COUNT; i++) 
        {
            if (strncmp(input_buffer, command_list[i], buffer_pos) == 0)
                matches[matches_found++] = command_list[i];
        }

        if (matches_found == 1) 
        {
            const char* remaining = matches[0] + buffer_pos;
            while (*remaining) 
            {
                input_buffer[buffer_pos++] = *remaining;
                printf("%c", *remaining++);
            }
            input_buffer[buffer_pos++] = ' ';
            printf(" ");
        } 
        else if (matches_found > 1) 
        {
            printf("\n");
            for (int i = 0; i < matches_found; i++)
                printf("%s  ", matches[i]);
            
            shell_prompt();
            printf("%s", input_buffer);
        }
    }
    else 
    {
        const char* file_part = space_ptr + 1;
        while(*file_part == ' ') file_part++;
        
        char path_temp[256];
        strncpy(path_temp, file_part, 255);
        path_temp[255] = '\0';

        char* last_slash = strrchr(path_temp, '/');
        const char* search_term = file_part;

        char search_dir_path[256];
        if (last_slash) 
        {
            search_term = strrchr(file_part, '/') + 1;
            if (last_slash == path_temp) strcpy(search_dir_path, "/");
            else 
            {
                *last_slash = '\0';
                strcpy(search_dir_path, path_temp);
            }
        }
        else strcpy(search_dir_path, current_working_directory);

        int part_len = strlen(search_term);
        char matched_names[64][128];
        uint8_t file_types[64];
        int f_matches_found = 0;

        if (is_user_mode())
        {
            int fd = (int)syscall(SYS_OPEN, (uint64_t)search_dir_path, 0, 0, 0, 0, 0);
            if (fd >= 0)
            {
                struct vfs_dirent {
                    char name[128];
                    uint32_t inode;
                    uint32_t type;
                } de;

                int i = 0;
                while (syscall(SYS_READDIR, (uint64_t)fd, (uint64_t)i++, (uint64_t)&de, 0, 0, 0) > 0 && f_matches_found < 64)
                {
                    if (strncmp(search_term, de.name, part_len) == 0) 
                    {
                        strcpy(matched_names[f_matches_found], de.name);
                        file_types[f_matches_found] = de.type;
                        f_matches_found++;
                    }
                }
                syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
            }
        }
        else 
        {
#if defined(__is_libk)
            VFSNode* search_dir = (file_part[0] == '/') ? vfs_root : (cwd_node ? cwd_node : vfs_root);
            bool should_close_dir = false;

            if (last_slash) 
            {
                if (last_slash != path_temp) 
                {
                    VFSNode* target = vfs_open(path_temp);
                    if (target && target->type == VFS_DIRECTORY) 
                    {
                        search_dir = target;
                        should_close_dir = true;
                    }
                    else { if (target) vfs_close(target); return; }
                }
            }

            uint32_t i = 0;
            vfs_dirent* de;
            while ((de = vfs_readdir(search_dir, i++)) && f_matches_found < 64) 
            {
                if (strncmp(search_term, de->name, part_len) == 0) 
                {
                    strcpy(matched_names[f_matches_found], de->name);
                    file_types[f_matches_found] = de->type;
                    f_matches_found++;
                }
            }
            if (should_close_dir && search_dir != vfs_root && search_dir != cwd_node) vfs_close(search_dir);
#endif
        }

        if (f_matches_found == 1) 
        {
            for(int j = 0; j < part_len; j++) 
            {
                printf("\b \b");
                buffer_pos--;
            }

            const char* completion = matched_names[0];
            while (*completion) 
            {
                input_buffer[buffer_pos++] = *completion;
                printf("%c", *completion++);
            }

            if (file_types[0] == 2) // VFS_DIRECTORY=2
            {
                input_buffer[buffer_pos++] = '/';
                printf("/");
            }
            else 
            {
                input_buffer[buffer_pos++] = ' ';
                printf(" ");
            }
        }
        else if (f_matches_found > 1) 
        {
            printf("\n");
            for (int j = 0; j < f_matches_found; j++) 
            {
                if (file_types[j] == 2) shell_setcolor(vga_color_t(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
                printf("%s%s  ", matched_names[j], (file_types[j] == 2 ? "/" : ""));
                shell_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            }
            shell_prompt();
            printf("%s", input_buffer);
        }
    }
}

/**
 * cmd_uptime: Displays to the user the system uptime
 */
static void cmd_uptime() 
{
    if (is_user_mode()) 
    {
        uint32_t ticks = (uint32_t)syscall(SYS_UPTIME, 0, 0, 0, 0, 0, 0);
        uint32_t seconds = ticks / 100;
        printf("System uptime: %d hours, %d minutes, %d seconds (%d ticks)\n", 
                seconds / 3600, (seconds / 60) % 60, seconds % 60, ticks);
        return;
    }
#if defined(__is_libk)
    uint32_t ticks = timer_get_ticks();
    uint32_t seconds = ticks / 100;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;

    printf("System uptime: %d hours, %d minutes, %d seconds (%d ticks)\n", 
            hours, minutes % 60, seconds % 60, ticks);
#endif
}

/**
 * cmd_ps: Displays to the user all the available/running processes
 */
static void cmd_ps() 
{ 
    if (is_user_mode()) 
    {
        syscall(SYS_PS, 0, 0, 0, 0, 0, 0);
        return;
    }
#if defined(__is_libk)
    thread_print_list(); 
#endif
}


/**
 * cmd_cd: Changes the directory to the requested
 */
static void cmd_cd(const char* args) 
{
    if (!args || args[0] == '\0' || strcmp(args, "~") == 0) 
    {
        strcpy(current_working_directory, "/");
#if defined(__is_libk)
        if (!is_user_mode())
        {
            if (cwd_node && cwd_node != vfs_root) vfs_close(cwd_node);
            cwd_node = vfs_root;
        }
#endif
        return;
    }

    char path[512];
    resolve_path(path, args);

#if defined(__is_libk)
    if (is_user_mode())
    {
        int fd = (int)syscall(SYS_OPEN, (uintptr_t)path, 0, 0, 0, 0, 0);
        if (fd >= 0)
        {
            syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
            if (strcmp(args, "..") == 0) 
            {
                if (strcmp(current_working_directory, "/") != 0) 
                {
                    char* last = strrchr(current_working_directory, '/');
                    if (last == current_working_directory) strcpy(current_working_directory, "/");
                    else if (last) *last = '\0';
                }
            } 
            else if (strcmp(args, ".") != 0) strncpy(current_working_directory, path, 255);
        }
        else printf("cd: %s: No such file or directory\n", args);
        return;
    }

    VFSNode* new_dir = vfs_open(path);
    if (new_dir) 
    {
        if (new_dir->type == VFS_DIRECTORY) 
        {
            if (cwd_node && cwd_node != vfs_root) vfs_close(cwd_node);
            cwd_node = new_dir;

            if (strcmp(args, "..") == 0) 
            {
                if (strcmp(current_working_directory, "/") != 0) 
                {
                    char* last = strrchr(current_working_directory, '/');
                    if (last == current_working_directory) strcpy(current_working_directory, "/");
                    else if (last) *last = '\0';
                }
            } 
            else if (strcmp(args, ".") == 0) {}
            else strncpy(current_working_directory, path, 255);
        } 
        else 
        {
            printf("cd: %s: Not a directory\n", args);
            vfs_close(new_dir);
        }
    } 
    else printf("cd: %s: No such file or directory\n", args);
#endif
}

/**
 * cmd_mkdir: Creates a directory
 */
static void cmd_mkdir(const char* args) 
{
    if (!args || args[0] == '\0') 
    {
        printf("mkdir: missing operand\n");
        return;
    }

    char full_path[512];
    resolve_path(full_path, args);

#if defined(__is_libk)
    if (is_user_mode())
    {
        if (syscall(SYS_MKDIR, (uintptr_t)full_path, 0755, 0, 0, 0, 0) != 0) printf("mkdir: cannot create directory '%s'\n", args);
        return;
    }
    if (vfs_mkdir(full_path, 0755) != 0) printf("mkdir: cannot create directory '%s'\n", args);
#endif
}

/**
 * cmd_touch: Creates a file
 */
static void cmd_touch(const char* args) 
{
    if (!args || args[0] == '\0') 
    {
        printf("touch: missing file operand\n");
        return;
    }

    char full_path[512];
    resolve_path(full_path, args);

#if defined(__is_libk)
    if (is_user_mode())
    {
        int fd = (int)syscall(SYS_OPEN, (uintptr_t)full_path, 1, 0, 0, 0, 0); // 1 = O_CREAT
        if (fd >= 0) syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
        else printf("touch: cannot create '%s'\n", args);
        return;
    }
    VFSNode* n = vfs_create(full_path, 0); 
    if (n) vfs_close(n);
    else printf("touch: cannot create '%s'\n", args);
#endif
}

/**
 * cmd_rm: Removes a file or directory
 */
static void cmd_rm(const char* args) 
{
    if (!args || args[0] == '\0') 
    {
        printf("rm: missing operand\n");
        return;
    }

    char full_path[512];
    resolve_path(full_path, args);
#if defined(__is_libk)
    if (is_user_mode())
    {
        if (syscall(SYS_UNLINK, (uintptr_t)full_path, 0, 0, 0, 0, 0) != 0) printf("rm: cannot remove '%s': No such file or directory\n", args);
        return;
    }
    if (!vfs_unlink(full_path)) printf("rm: cannot remove '%s': No such file or directory\n", args);
#endif
}

/**
 * shell_execute: Parses the input string and calls the corresponding function
 */
void shell_execute(const char* command) 
{
    while (*command == ' ') command++;
    if (strlen(command) == 0) return; 
    
    char cmd[256];
    const char* args = command;
    int i = 0;
    
    while (*args && *args != ' ' && i < 255) 
        cmd[i++] = *args++;
    cmd[i] = '\0';
    
    while (*args == ' ') args++;

    char args_buffer[256];
    strncpy(args_buffer, args, 255);
    args_buffer[255] = '\0';

    int len = strlen(args_buffer);
    while (len > 0 && (args_buffer[len-1] == ' ' || args_buffer[len-1] == '\t' || args_buffer[len-1] == '\r')) {
        args_buffer[len-1] = '\0';
        len--;
    }

    const char* clean_args = args_buffer;

    if (strcmp(cmd, "help") == 0)		 		cmd_help(clean_args);
    else if (strcmp(cmd, "clear") == 0) 		cmd_clear();
    else if (strcmp(cmd, "echo") == 0) 			cmd_echo(clean_args);
	else if (strcmp(cmd, "info") == 0) 			cmd_info();

#if defined(__is_libk)
    else if (!is_user_mode() && strcmp(cmd, "testheap") == 0) 	cmd_testheap();
    else if (!is_user_mode() && strcmp(cmd, "meminfo") == 0) 	cmd_meminfo();
#endif

    else if (strcmp(cmd, "reboot") == 0) 		cmd_reboot();
    else if (strcmp(cmd, "halt") == 0) 			cmd_halt();

#if defined(__is_libk)
    else if (!is_user_mode() && strcmp(cmd, "paginginfo") == 0) 	cmd_paginginfo();
	else if (!is_user_mode() && strcmp(cmd, "testpaging") == 0) 	cmd_testpaging();
    else if (!is_user_mode() && strcmp(cmd, "memstat") == 0)     cmd_memstat();
    else if (!is_user_mode() && strcmp(cmd, "dump") == 0)        cmd_dump(clean_args);
#endif

    else if (strcmp(cmd, "uptime") == 0)        cmd_uptime();
    else if (strcmp(cmd, "ps") == 0)            cmd_ps();
    else if (strcmp(cmd, "pkill") == 0)         cmd_pkill(clean_args);
    else if (strcmp(cmd, "ls") == 0)            cmd_ls(clean_args);
    else if (strcmp(cmd, "cat") == 0)           cmd_cat(clean_args);
    else if (strcmp(cmd, "cd") == 0)            cmd_cd(clean_args);
    else if (strcmp(cmd, "touch") == 0)         cmd_touch(clean_args);
    else if (strcmp(cmd, "mkdir") == 0)         cmd_mkdir(clean_args);
    else if (strcmp(cmd, "rm") == 0)            cmd_rm(clean_args); 
    else if (strcmp(cmd, "sleep") == 0)         cmd_sleep(clean_args);
    else if (strcmp(cmd, "pid") == 0)           cmd_pid();
    else if (strcmp(cmd, "stat") == 0)          cmd_stat(clean_args);

    else 
    {
        char* kargv[16];
        kargv[0] = cmd;
        int kargc = 1;
        char* p = args_buffer;
        while (*p && kargc < 15) {
             kargv[kargc++] = p;
             while(*p && *p != ' ') p++;
             if (*p) *p++ = 0;
             while(*p == ' ') p++;
        }
        kargv[kargc] = nullptr; 

        int pid = kex_load(cmd, kargc, kargv);
        if (pid > 0) 
        {
             // Wait for process
             while (thread_get_by_id(pid) != nullptr) 
             {
                 thread_sleep(20);
             }
             // Process context switch will return here when thread is gone
        }
        else 
        {
             printf("Unknown command: %s\nType 'help' for available commands", command);
        }
    }
    shell_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}


/**
 * shell_run: Main shell loop
 * Handles keyboard input, history (Up/Down arrows), and backspace.
 */
void shell_run() 
{
    shell_prompt();
    int cursor_pos = 0;
    
    while (true) 
    {
        char c = getchar();
        if (c == 0) continue;

        if (c == '\n' || c == '\r') 
        {
            printf("\n");
            input_buffer[buffer_pos] = '\0';

            if (buffer_pos > 0) 
            {
                if (history_count == 0 || strcmp(input_buffer, history[0]) != 0) 
                {
                    for (int i = MAX_HISTORY - 1; i > 0; i--)
                        memcpy(history[i], history[i-1], SHELL_BUFFER_SIZE);

                    strcpy(history[0], input_buffer);
                    if (history_count < MAX_HISTORY) history_count++;
                }
                shell_execute(input_buffer);
            }
            buffer_pos = 0;
            cursor_pos = 0;
            history_index = -1;
            shell_prompt();
        }

        else if (c == '\b') 
        {    
            if (cursor_pos > 0) 
            {
                cursor_pos--;
                buffer_pos--;
                
                for (int i = cursor_pos; i < buffer_pos; i++) {
                    input_buffer[i] = input_buffer[i+1];
                }
                input_buffer[buffer_pos] = '\0';
                
                terminal_move_cursor(-1);

                #if defined(__is_libk)
                    serial_move_cursor(-1);
                #endif
                for (int i = cursor_pos; i < buffer_pos; i++) printf("%c", input_buffer[i]);
                printf(" "); 
                terminal_move_cursor(-(buffer_pos - cursor_pos + 1));
                #if defined(__is_libk)
                    serial_move_cursor(-(buffer_pos - cursor_pos + 1));
                #endif  
            }
        }

        else if (c == '\t') 
        {
            if (cursor_pos == buffer_pos) {
                shell_tab_completion();
                cursor_pos = buffer_pos;
            }
        }
        
        else if (buffer_pos < SHELL_BUFFER_SIZE - 1 && c >= 32 && c <= 126) 
        {
            for (int i = buffer_pos; i > cursor_pos; i--) {
                input_buffer[i] = input_buffer[i-1];
            }
            input_buffer[cursor_pos] = c;
            buffer_pos++;
            cursor_pos++;
            
            for (int i = cursor_pos - 1; i < buffer_pos; i++) printf("%c", input_buffer[i]);
            terminal_move_cursor(-(buffer_pos - cursor_pos));
            #if defined(__is_libk)
                serial_move_cursor(-(buffer_pos - cursor_pos));
            #endif
        }

        else if (c == KEY_UP) 
        {
            if (history_count > 0 && history_index < history_count - 1) 
            {
                history_index++;
                terminal_move_cursor(-cursor_pos);
                #if defined(__is_libk)
                    serial_move_cursor(-cursor_pos);
                #endif
                for (int i = 0; i < buffer_pos; i++) printf(" ");
                terminal_move_cursor(-buffer_pos);
                #if defined(__is_libk)
                    serial_move_cursor(-buffer_pos);
                #endif
                
                strcpy(input_buffer, history[history_index]);
                buffer_pos = strlen(input_buffer);
                cursor_pos = buffer_pos;
                printf("%s", input_buffer);
            }
        }

        else if (c == KEY_DOWN) 
        {
            if (history_index > 0) 
            {
                history_index--;
                terminal_move_cursor(-cursor_pos);
                #if defined(__is_libk)
                    serial_move_cursor(-cursor_pos);
                #endif
                for (int i = 0; i < buffer_pos; i++) printf(" ");
                terminal_move_cursor(-buffer_pos);
                #if defined(__is_libk)
                    serial_move_cursor(-buffer_pos);
                #endif
                
                strcpy(input_buffer, history[history_index]);
                buffer_pos = strlen(input_buffer);
                cursor_pos = buffer_pos;
                printf("%s", input_buffer);
            }
            else if (history_index == 0) 
            {
                history_index = -1;
                terminal_move_cursor(-cursor_pos);
                #if defined(__is_libk)
                    serial_move_cursor(-cursor_pos);
                #endif
                for (int i = 0; i < buffer_pos; i++) printf(" ");
                terminal_move_cursor(-buffer_pos);
                #if defined(__is_libk)
                    serial_move_cursor(-buffer_pos);
                #endif
                input_buffer[0] = '\0';
                buffer_pos = 0;
                cursor_pos = 0;
            }
        }
        
        else if (c == KEY_LEFT)
        {
            if (cursor_pos > 0)
            {
                cursor_pos--;
                terminal_move_cursor(-1);
                #if defined(__is_libk)
                    serial_move_cursor(-1);
                #endif
            }
        }
        
        else if (c == KEY_RIGHT)
        {
            if (cursor_pos < buffer_pos)
            {
                cursor_pos++;
                terminal_move_cursor(1);
                #if defined(__is_libk)
                    serial_move_cursor(1);
                #endif
            }
        }
    }
}