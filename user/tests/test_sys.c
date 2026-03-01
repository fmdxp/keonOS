/*
 * keonOS - user/tests/test_sys.c
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
#include <unistd.h>

int main(int argc, char** argv) {
    printf("=== TEST_SYS: System Calls Test ===\n");

    // 1. GetPID
    int pid = getpid();
    printf("Current PID: %d\n", pid);
    if (pid <= 0) printf("WARN: Strange PID\n");
    else printf("PASS: getpid() returned sensible value.\n");

    // 2. Sleep
    printf("Testing sleep(1)... (Should wait approx 1 sec)\n");
    sleep(1);
    printf("PASS: Woke up from sleep.\n");

    // 3. Malloc (sbrk)
    printf("Testing malloc(1MB)...\n");
    char* big_buf = (char*)malloc(1024 * 1024);
    if (!big_buf) {
        printf("FAIL: malloc failed\n");
    } else {
        printf("PASS: malloc(1MB) returned %p\n", big_buf);
        
        // Touch memory to ensure it's mapped
        printf("Touching memory...\n");
        big_buf[0] = 'A';
        big_buf[1024*1024-1] = 'Z';
        
        if (big_buf[0] == 'A' && big_buf[1024*1024-1] == 'Z') {
             printf("PASS: Memory access successful.\n");
        } else {
             printf("FAIL: Memory content corruption.\n");
        }
        
        free(big_buf);
        printf("PASS: freed memory.\n");
    }

    printf("=== TEST_SYS Completed ===\n");
    return 0;
}
