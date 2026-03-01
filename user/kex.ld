ENTRY(_start)

SECTIONS
{
    . = 0x400000 + SIZEOF_HEADERS;

    .note.kex : { KEEP(*(.note.kex)) } :note
    
    .text : { *(.text*) *(.rodata*) } :text
    
    . = ALIGN(0x1000);
    .data : { *(.data*) *(.bss*) } :data
}

PHDRS
{
    note PT_NOTE;
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}
