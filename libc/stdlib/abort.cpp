#include <kernel/panic.h>
#include <kernel/error.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((__noreturn__))
void abort(void)
{
	#if defined(__is_libk)
		panic(KernelError::K_ERR_UNKNOWN_ERROR, "Kernel called 'abort()'", 0xDEAD);
	#else
		printf("Panic: User process called abort()\n");
		while (1) asm volatile("hlt");
		// In Userspace we'd use SIGABRT
	#endif
		__builtin_unreachable();
}