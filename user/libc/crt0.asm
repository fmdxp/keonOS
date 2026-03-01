;
 ; keonOS - user/libc/crt0.asm
 ; Copyright (C) 2025-2026 fmdxp
 ;
 ; This program is free software: you can redistribute it and/or modify
 ; it under the terms of the GNU General Public License as published by
 ; the Free Software Foundation, either version 3 of the License, or
 ; (at your option) any later version.
 ;
 ; ADDITIONAL TERMS (Per Section 7 of the GNU GPLv3):
 ; - Original author attributions must be preserved in all copies.
 ; - Modified versions must be marked as different from the original.
 ; - The name "keonOS" or "fmdxp" cannot be used for publicity without permission.
 ;
 ; This program is distributed in the hope that it will be useful,
 ; but WITHOUT ANY WARRANTY; without even the implied warranty of
 ; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 ; See the GNU General Public License for more details.
 ;

[BITS 64]
global _start
extern main
extern exit

section .note.kex
    dd 7                ; namesz
    dd 4                ; descsz
    dd 0x1001           ; type (NT_KEX_VERSION)
    db "KeonOS", 0      ; name (padded to 8 bytes, but 7+1=8)
    db 0                ; padding
    db "KEX1"           ; desc

section .text
_start:
    ; Initialize arguments (TODO: Get from stack when loader supports it)
    xor rdi, rdi ; argc = 0
    xor rsi, rsi ; argv = NULL

    call main

    ; Exit with return value from main
    mov rdi, rax
    call exit
