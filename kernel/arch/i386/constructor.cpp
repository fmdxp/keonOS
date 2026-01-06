#include <kernel/arch/i386/constructor.h>
#include <stdio.h>

extern "C" constructor start_ctors;
extern "C" constructor end_ctors;

extern "C" void initialize_constructors()
{
	for (constructor* i = &start_ctors; i < &end_ctors; i++)
		(*i)();
}