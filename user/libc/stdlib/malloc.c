#include <stdlib.h>
#include <sys/syscall.h>

extern long syscall1(long n, long a1);

void* sbrk(long increment) {
    return (void*)syscall1(SYS_SBRK, increment);
}

void* malloc(size_t size) {
    if (size == 0) return NULL;
    // Simple bump allocator: just get memory from kernel
    void* ptr = sbrk(size);
    if ((long)ptr == -1) return NULL;
    return ptr;
}

void free(void* ptr) {
    (void)ptr; // No-op for now
}
