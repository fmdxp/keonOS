global switch_context

switch_context:
    push ebp
    push ebx
    push esi
    push edi
    pushfd

    mov eax, [esp + 24]     
    mov [eax], esp          
	
    mov eax, [esp + 28]     
    mov esp, [eax]          

    popfd
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret