section .multiboot
align 4
	; Multiboot header
    dd 0x1BADB002             			; MAGIC
    dd 0x00000003             			; FLAGS (ALIGNED + MEMINFO)
    dd -(0x1BADB002 + 0x00000003) 		; CHECKSUM

section .boot
global _start
extern kernel_main

global multiboot_magic
global multiboot_info_ptr


VM_BASE         equ 0xC0000000
PDE_INDEX       equ (VM_BASE >> 22)

_start:
	mov esp, (_stack_low_top - VM_BASE)

	mov edi, (multiboot_magic - VM_BASE)
	mov [edi], eax
	mov edi, (multiboot_info_ptr - VM_BASE)
	mov [edi], ebx


	mov edi, (boot_page_directory - VM_BASE)

	mov ecx, 0

.setup_pd:
    mov eax, (boot_page_table - VM_BASE)
    mov edx, ecx
    shl edx, 12
    add eax, edx
    or eax, 0x003

    mov [edi + ecx * 4], eax
    mov [edi + (PDE_INDEX + ecx) * 4], eax

    inc ecx
    cmp ecx, 128
    jne .setup_pd

    mov ecx, 0
    mov edx, (boot_page_table - VM_BASE)

.fill_pt:
    mov eax, ecx
    shl eax, 12                                 
    or eax, 0x003  
    mov [edx + ecx * 4], eax

    inc ecx
    cmp ecx, 131072
    jne .fill_pt

    mov eax, (boot_page_directory - VM_BASE)
    mov cr3, eax    

    mov eax, cr0
    or eax, 0x80000001  
    mov cr0, eax

    lea eax, [.higher_half]
    jmp eax



.higher_half:
    mov esp, _stack_low_top
    
    push ebx
    push eax
    call kernel_main

    cli

.hang:
	hlt
	jmp .hang


section .bss
align 4096
boot_page_directory: 	resb 4096
boot_page_table:        resb 524288
_stack_low_bottom: 		resb 16384
_stack_low_top:

global _stack_low_bottom
global _stack_low_top

; Reserve storage for Multiboot data to be accessed from C++ code
multiboot_magic:    resd 1
multiboot_info_ptr: resd 1