[BITS 32]

global paging_load_directory
global paging_enable
global paging_disable

paging_load_directory:
    push ebp
    mov ebp, esp
    mov eax, [esp + 8]  
    mov cr3, eax
    mov esp, ebp
    pop ebp
    ret

paging_enable:
    mov eax, cr0      
    or eax, 0x80000000  
    mov cr0, eax      
    jmp .flush

.flush:
    ret

paging_disable:
    mov eax, cr0       
    and eax, 0x7FFFFFFF 
    mov cr0, eax        
    ret

