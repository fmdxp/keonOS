#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <drivers/multiboot.h>

#if defined(__linux__)
#error "You are not using a cross-compiler, keonOS can't compile. Aborting..."
#endif

#if !defined(__i386__)
#error "keonOS needs to be compiled with a ix86-elf (cross-)compiler. Aborting..."
#endif

extern "C" void kernel_main(void);

#endif		// KERNEL_H