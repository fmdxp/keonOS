; *****************************************************************************
; * keonOS - kernel/arch/x86_64/asm/switch.asm
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

[BITS 64]
global switch_context
global user_thread_entry

switch_context:
    pushfq
    push r15
    push r14
    push r13
    push r12
    push rbx
    push rbp

    mov [rdi], rsp
    mov rsp, rsi

    pop rbp
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    popfq

    ret

user_thread_entry:
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    iretq