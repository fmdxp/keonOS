; *****************************************************************************
; * keonOS - boot/boot.asm
; * Copyright (C) 2025-2026 fmdxp
; *
; * This program is free software: you can redistribute it and/or modify
; * it under the terms of the GNU General Public License as published by
; * the Free Software Foundation, either version 3 of the License, or
; * (at your option) any later version.
; *
; * ADDITIONAL TERMS (Per Section 7 of the GNU GPLv3):
; * - Original author attributions must be preserved in all copies.
; * - Modified versions must be marked as different from the original.
; * - The name "keonOS" or "fmdxp" cannot be used for publicity without permission.
; *
; * This program is distributed in the hope that it will be useful,
; * but WITHOUT ANY WARRANTY; without even the implied warranty of
; * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
; * See the GNU General Public License for more details.
; *****************************************************************************


bits 32

section .multiboot
align 8

multiboot_header_start:
    dd 0xe85250d6
    dd 0             			
    dd multiboot_header_end - multiboot_header_start
    dd -(0xe85250d6 + 0 + (multiboot_header_end - multiboot_header_start))

    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size

multiboot_header_end:


section .boot
global _start
extern kernel_main

global multiboot_magic
global multiboot_info_ptr

extern _stack_top
extern _stack_bottom


VM_OFFSET       equ 0xFFFFFFFF80000000

_start:
    mov esi, ebx
    mov edi, eax

	mov esp, (_stack_top - VM_OFFSET)

	mov eax, pdpt_table - VM_OFFSET
    or eax, 0b11
    mov [(pml4_table - VM_OFFSET)], eax       
    mov [(pml4_table - VM_OFFSET) + 511*8], eax

    mov eax, pd_table - VM_OFFSET
    or eax, 0b11
    mov [(pdpt_table - VM_OFFSET)], eax
    mov [(pdpt_table - VM_OFFSET) + 510*8], eax

    mov ecx, 0


.map_pd:
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000011 
    mov [(pd_table - VM_OFFSET) + ecx*8], eax
    inc ecx
    cmp ecx, 512
    jne .map_pd

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, pml4_table - VM_OFFSET
    mov cr3, eax
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64.ptr - VM_OFFSET]

    push gdt64.code
    push long_mode_high
    retf


bits 64
long_mode_high:
    lgdt [gdt64_ptr_virt]

    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rsp, _stack_top
    sub rsp, 8
    
    mov rdi, rdi
    mov rsi, rsi

    mov [multiboot_magic], edi
    mov [multiboot_info_ptr], rsi

    call kernel_main

.hang:
    hlt
    jmp .hang


section .bss
align 4096
pml4_table: resb 4096
pdpt_table: resb 4096
pd_table:   resb 4096

multiboot_magic:    resd 1
multiboot_info_ptr: resq 1


section .rodata
gdt64:
    dq 0
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)
.ptr:
    dw $ - gdt64 - 1
    dq gdt64 - VM_OFFSET
    
gdt64_ptr_virt:
    dw $ - gdt64 - 1
    dq gdt64