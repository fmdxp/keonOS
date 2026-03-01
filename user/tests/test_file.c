/*
 * keonOS - user/tests/test_file.c
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int main(int argc, char** argv) {
    printf("=== TEST_FILE: File I/O and Stat Test ===\n");

    const char* filename = "/test_file.txt";
    const char* content = "This is a test file for KeonOS.\n";
    
    // 1. Create/Write file
    printf("Creating file %s...\n", filename);
    int fd = open(filename, O_CREAT | O_WRONLY);
    if (fd < 0) {
        printf("FAIL: open(O_CREAT) failed\n");
        return 1;
    }
    
    int len = strlen(content);
    int written = write(fd, content, len);
    if (written != len) {
        printf("FAIL: write() wrote %d bytes, expected %d\n", written, len);
        close(fd);
        return 1;
    }
    close(fd);
    printf("PASS: File created and written.\n");

    // 2. Stat file
    printf("Statting file %s...\n", filename);
    struct stat st;
    if (stat(filename, &st) != 0) {
        printf("FAIL: stat() failed\n");
        return 1;
    }
    
    if (st.st_size != len) {
        printf("FAIL: stat size %ld != %d\n", st.st_size, len);
    } else {
        printf("PASS: stat() size correct (%ld).\n", st.st_size);
    }

    // 3. Read file
    printf("Reading file %s...\n", filename);
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("FAIL: open(O_RDONLY) failed\n");
        return 1;
    }
    
    char buffer[64];
    int read_bytes = read(fd, buffer, 63);
    if (read_bytes != len) {
        printf("FAIL: read() read %d bytes, expected %d\n", read_bytes, len);
    } else {
        buffer[read_bytes] = 0;
        if (strcmp(buffer, content) == 0) {
            printf("PASS: Content verification successful.\n");
        } else {
             printf("FAIL: Content mismatch.\n");
             printf("Exp: %s\nGot: %s\n", content, buffer);
        }
    }
    close(fd);

    printf("=== TEST_FILE Completed ===\n");
    return 0;
}
