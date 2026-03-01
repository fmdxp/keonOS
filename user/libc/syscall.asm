[BITS 64]

global syscall0
global syscall1
global syscall2
global syscall3
global syscall4
global syscall5

section .text

; syscall0(num)
syscall0:
    mov rax, rdi
    syscall
    ret

; syscall1(num, arg1)
syscall1:
    mov rax, rdi
    mov rdi, rsi
    syscall
    ret

; syscall2(num, arg1, arg2)
syscall2:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    syscall
    ret

; syscall3(num, arg1, arg2, arg3)
syscall3:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    syscall
    ret

; syscall4(num, arg1, arg2, arg3, arg4)
syscall4:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r10, r8
    syscall
    ret

; syscall5(num, arg1, arg2, arg3, arg4, arg5)
syscall5:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r10, r8
    mov r8, r9
    syscall
    ret
