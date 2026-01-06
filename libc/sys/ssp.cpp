#include <stdint.h>
#include <stdlib.h>
#include <kernel/panic.h>
#include <kernel/error.h>

#if UINTPTR_MAX == UINT32_MAX
	#define STACK_CHK_GUARD 0xe2dee396
#else
	#define STACK_CHK_GUARD 0x595e9fbd94fda766
#endif

extern "C" 
{
	uintptr_t __stack_chk_guard = STACK_CHK_GUARD;

	__attribute__((noreturn))
	void __stack_chk_fail(void)
	{
		panic(KernelError::K_ERR_STACK_SMASHED, "STACK SMASHING DETECTED: The kernel was protected from a buffer overflow.", 0xDEADBEEF);
		while (1) asm("cli; hlt");
	}

}