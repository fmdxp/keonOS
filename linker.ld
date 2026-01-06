ENTRY(_start)

SECTIONS
{
	. = 1M;

	.text BLOCK(4K) : ALIGN(4K)
	{
		KEEP(*(.multiboot))
		*(.text)
		*(.gnu.linkonce.t*)
	}

	.rodata BLOCK(4K) : ALIGN(4K)
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

		*(.rodata)
		*(.gnu.linkonce.r*)
	}

	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
		*(.gnu.linkonce.d*)
	}

	.bss BLOCK(4K) : ALIGN(4K)
	{
		sbss = .;
		*(COMMON)
		*(.bss)
		*(.gnu.linkonce.b*)
		ebss = .;
	}

	.stack BLOCK(4k) : ALIGN(4K)
	{
		_stack_bottom = .;
		. += 16K;
		_stack_top = .;
	}

	_kernel_end = .;

	/DISCARD/ :
	{
		*(.comment)
	}
}