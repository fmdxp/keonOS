#include <mm/paging.h>
#include <mm/vmm.h>

uintptr_t VMM::kernel_dynamic_break = 0xD0000000;

void* VMM::allocate(size_t pages, uint32_t flags)
{
	void* start_addr = (void*)kernel_dynamic_break;

	for (size_t i = 0; i < pages; i++)
	{
		void* phys_frame = pfa_alloc_frame();
		if (!phys_frame) return nullptr;
		paging_map_page((void*)(kernel_dynamic_break + (i * PAGE_SIZE)), phys_frame, flags, true);
	}
	kernel_dynamic_break += (pages * PAGE_SIZE);
	return start_addr;
}

void VMM::free(void* virt_addr, size_t pages)
{
	uintptr_t addr = (uintptr_t)virt_addr;

	for (size_t i = 0; i < pages; i++)
	{
		uintptr_t current_virt = addr + (i * PAGE_SIZE);
		void* phys_frame = paging_get_physical_address((void*)current_virt);
		if (phys_frame) pfa_free_frame(phys_frame);
		paging_unmap_page((void*)current_virt);
	}
}

static size_t vmm_allocated_bytes = 0;

void* VMM::sbrk(size_t increment_bytes)
{
	static uintptr_t current_break = 0xD0000000;
	uintptr_t old_break = current_break;

	if (increment_bytes == 0) return (void*)old_break;
	size_t pages_needed = (increment_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

	vmm_allocated_bytes += (pages_needed * PAGE_SIZE);

	for (size_t i = 0; i < pages_needed; i++)
	{
		void* phys_frame = pfa_alloc_frame();
		if (!phys_frame) return (void*)-1;
		paging_map_page((void*)current_break, phys_frame, PTE_PRESENT | PTE_RW, true);
		current_break += PAGE_SIZE;
	}
	return (void*)old_break;
}

size_t VMM::get_total_allocated()
{
	return vmm_allocated_bytes;
}