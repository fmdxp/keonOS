/*
 * keonOS - user/hello.c
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>


int main(int argc, char** argv) 
{
    printf("Hello from KeonOS's libc!\n");
    
    // Test getpid
    int pid = getpid();
    printf("Current PID: %d\n", pid);
    
    // Test stat on root
    struct stat st;
    if (stat("/", &st) == 0) printf("Root stat: inode=%ld, size=%ld, mode=%o\n", st.st_ino, st.st_size, st.st_mode);
    else printf("Failed to stat /\n");
    
    // Test directory listing
    printf("\nListing root directory:\n");
    DIR* dir = opendir("/");
    if (dir) 
    {
        struct dirent* de;
        while ((de = readdir(dir)) != NULL) 
            printf("  %s [%s]\n", de->d_name, (de->d_type == DT_DIR) ? "DIR" : "FILE");
        
        closedir(dir);
    } 
    else printf("Failed to open root directory\n");
    

    // Test sleep
    printf("Sleeping for 1 second...\n");
    sleep(1);
    printf("Woke up!\n");

    return 42;
}
