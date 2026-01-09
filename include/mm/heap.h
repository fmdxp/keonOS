#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

struct heap_block
{
	size_t size;
	bool free;
	struct heap_block* next;
};

struct heap_stats
{
	size_t total_size;
	size_t used_size;
	size_t free_size;
	size_t block_count;
	size_t free_block_count;
};

bool kheap_init(void* start_addrm, size_t size);
void* kmalloc(size_t size);
void kfree(void* ptr);
void kheap_get_stats(struct heap_stats* stats);

inline void* operator new(size_t, void* p) throw() {
    return p;
}

void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
void operator delete(void* ptr, size_t size) noexcept;

heap_block* get_kheap_start();

#endif		// HEAP_H