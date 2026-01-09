#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

class VMM
{
public:
	static void* allocate(size_t pages, uint32_t flags);
	static void free(void* virt_addr, size_t pages);
	static void* sbrk(size_t increment_bytes);
	static size_t get_total_allocated();
	static uintptr_t kernel_dynamic_break;
	static void* map_physical_region(uintptr_t phys_addr, size_t pages, uint32_t flags);
};


#endif		// VMM_H