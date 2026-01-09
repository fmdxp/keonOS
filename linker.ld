ENTRY(_start)

SECTIONS
{
	. = 1M;
	_kernel_physical_start = .;

	.boot ALIGN(4K) :
	{
		KEEP(*(.multiboot))
		*(.boot)
	}

	. += 0xC0000000;


	.text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000)
	{
    	_kernel_virtual_start = .;
		*(.text)
	}

	.rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xC0000000)
	{
		start_ctors = .;
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array))
		KEEP(*(SORT(.ctors*)))
		end_ctors = .;

		start_dtors = .;
		KEEP(*(SORT(.fini_array*)))
		KEEP(*(.fini_array))
        KEEP(*(SORT(.dtors*)))
		end_dtors = .;

		*(.rodata*)
		*(.vtable)
		*(.vftable)
	}

	.data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000)
	{
		*(.data)
	}

	.bss ALIGN(4K) : AT(ADDR(.bss) - 0xC0000000)
	{
		sbss = .;
		*(COMMON)
		*(.bss)
		ebss = .;
	}

	.stack ALIGN(4K) : AT(ADDR(.stack) - 0xC0000000)
	{
		_stack_bottom = .;
		. += 16K;
		_stack_top = .;
	}

	. = ALIGN(4K);
	_kernel_end = .;
	_kernel_physical_end = . - 0xC0000000;


	/DISCARD/ :
	{
		*(.comment)
	}

}