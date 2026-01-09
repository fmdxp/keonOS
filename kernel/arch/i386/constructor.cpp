#include <kernel/arch/i386/constructor.h>
#include <stdio.h>

extern "C" constructor start_ctors;
extern "C" constructor end_ctors;

extern "C" void initialize_constructors()
{
	for (constructor* i = &start_ctors; i < &end_ctors; i++)
		(*i)();
}

extern "C" 
{
	int __cxa_guard_acquire(long *g) { return !*(char *)(g); }
	void __cxa_guard_release(long *g) { *(char *)(g) = 1; }
	void __cxa_guard_abort(long *g) { (void)g; }

	void* __dso_handle = nullptr;
	int __cxa_atexit(void (*f)(void *), void *p, void *d) 
	{
		(void)f; (void)p; (void)d;
		return 0;
	}
}