#include <kernel/arch/i386/gdt.h>

gdt_entry gdt[5];
gdt_ptr	  gp;


void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
	gdt[num].base_low		= (base & 0xFFFF);
	gdt[num].base_middle 	= (base >> 16) & 0xFF;
	gdt[num].base_high		= (base >> 24) & 0xFF; 

	gdt[num].limit_low		= (limit & 0xFFFF);
	gdt[num].granularity	= (limit >> 16) & 0x0F;

	gdt[num].granularity	|= gran & 0xF0;
	gdt[num].access			= access;
}


void gdt_init()
{
	gp.limit	= (sizeof(gdt_entry) * 5) - 1;
	gp.base		= reinterpret_cast<uint32_t>(&gdt);

	gdt_set_gate(0, 0, 0, 0, 0); 					// 0x00: Null Segment
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);		// 0x08: Kernel Code Segment (Base=0, Limit=4GB, Access=0x9A, Gran=0xCF)
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);		// 0x10: Kernel Data Segment (Base=0, Limit=4GB, Access=0x92, Gran=0xCF)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);		// 0x18: User Code Segment
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);		// 0x20: User Data Segment

	// Load the GDT

	gdt_flush(reinterpret_cast<uint32_t>(&gp));
}