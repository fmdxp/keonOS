/*
 * keonOS - user/tools/klbtool.c
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
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

#define S_ARMAG		"!<arch>\n"
#define S_SARMAG	8
#define ARFMAG		"`\n"

struct ar_hdr {
	char ar_name[16];
	char ar_date[12];
	char ar_uid[6];
	char ar_gid[6];
	char ar_mode[8];
	char ar_size[10];
	char ar_fmag[2];
};

int list_klb(const char* filename) {
    int fd = open(filename, 0);
    if (fd < 0) {
        printf("Error: Cannot open %s\n", filename);
        return 1;
    }
    
    char magic[S_SARMAG];
    if (read(fd, magic, S_SARMAG) != S_SARMAG || strncmp(magic, S_ARMAG, S_SARMAG) != 0) {
        printf("Error: Not a valid .klb archive\n");
        close(fd);
        return 1;
    }
    
    printf("Listing archive: %s\n", filename);
    
    struct ar_hdr hdr;
    while (read(fd, &hdr, sizeof(hdr)) == sizeof(hdr)) {
        if (strncmp(hdr.ar_fmag, ARFMAG, 2) != 0) {
            printf("Error: Corrupt archive header\n");
            break;
        }
        
        long size = strtol(hdr.ar_size, NULL, 10);
        
        char name[17];
        memcpy(name, hdr.ar_name, 16);
        name[16] = 0;
        
        // Remove trailing slash used in some ar formats
        char* slash = strchr(name, '/');
        if (slash) *slash = 0;
        
        printf("  %s (%ld bytes)\n", name, size);
        
        // Advance to next file (padded to even byte)
        long offset = size + (size % 2);
        
        // Since we don't have lseek yet... oh wait we do have lseek usage in read loop? 
        // We don't have lseek syscall! 
        // We need to implement lseek or read dummy bytes.
        // For now, read dummy bytes.
        
        char* dummy = (char*)malloc(offset);
        read(fd, dummy, offset);
        free(dummy);
    }
    
    close(fd);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: klbtool [list|create] <file.klb>\n");
        return 1;
    }
    
    if (strcmp(argv[1], "list") == 0) {
        return list_klb(argv[2]);
    }
    
    printf("Unknown command: %s\n", argv[1]);
    return 1;
}
