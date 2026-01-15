/*
 * keonOS - mm/vmm.cpp
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
#include <kernel/constants.h>
#include <mm/vmm.h>

uintptr_t VMM::kernel_dynamic_break = 0;
static size_t vmm_allocated_bytes = 0;

void* VMM::allocate(size_t pages, uint32_t flags)
{
	if (kernel_dynamic_break == 0) return nullptr;

	void* start_addr = (void*)kernel_dynamic_break;

	for (size_t i = 0; i < pages; i++)
	{
		void* phys_frame = pfa_alloc_frame();
		if (!phys_frame) return nullptr;
		paging_map_page((void*)(kernel_dynamic_break + (i * PAGE_SIZE)), phys_frame, (uint64_t)flags);
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

void* VMM::sbrk(size_t increment_bytes)
{
	if (kernel_dynamic_break == 0) return (void*)-1;
	uintptr_t old_break = kernel_dynamic_break;

	if (increment_bytes == 0) return (void*)old_break;

	size_t pages_needed = (increment_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
	vmm_allocated_bytes += (pages_needed * PAGE_SIZE);

	for (size_t i = 0; i < pages_needed; i++)
	{
		void* phys_frame = pfa_alloc_frame();
		if (!phys_frame) return (void*)-1;

		paging_map_page((void*)kernel_dynamic_break, phys_frame, (uint64_t)(PTE_PRESENT | PTE_RW));
		kernel_dynamic_break += PAGE_SIZE;
	}
	return (void*)old_break;
}

size_t VMM::get_total_allocated()
{
	return vmm_allocated_bytes;
}

void* VMM::map_physical_region(uintptr_t phys_addr, size_t pages, uint32_t flags)
{
    void* start_addr = (void*)kernel_dynamic_break;

    for (size_t i = 0; i < pages; i++)
    {
        paging_map_page((void*)(kernel_dynamic_break), (void*)(phys_addr + (i * PAGE_SIZE)), (uint64_t)flags);
        kernel_dynamic_break += PAGE_SIZE;
    }

    return start_addr;
}