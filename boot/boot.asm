section .multiboot
align 4
	; Multiboot header
    dd 0x1BADB002             			; MAGIC
    dd 0x00000003             			; FLAGS (ALIGNED + MEMINFO)
    dd -(0x1BADB002 + 0x00000003) 		; CHECKSUM

section .text
global _start
extern kernel_main

; Symbols defined in the linker script
extern _stack_top
extern _stack_bottom

global multiboot_magic
global multiboot_info_ptr

_start:
	; Initialize the kernel stack pointer (ESP)
    ; We align it to 16 bytes as per the System V ABI requirement
	mov esp, _stack_top
	and esp, -16

	; Save Multiboot information passed by the bootloader in EAX and EBX
    ; EAX contains the magic number, EBX contains the pointer to the info structure
	mov [multiboot_magic], eax
    mov [multiboot_info_ptr], ebx

	; Transfer control to the C++ kernel entry point
	call kernel_main

	; If the kernel returns, disable interrupts and enter an infinite halt loop
	cli

.hang:
	hlt
	jmp .hang


section .bss
align 4
; Reserve storage for Multiboot data to be accessed from C++ code
multiboot_magic:    resd 1
multiboot_info_ptr: resd 1