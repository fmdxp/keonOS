#ifndef PANIC_H
#define PANIC_H

#include <kernel/constants.h>
#include <kernel/error.h>
#include <stdint.h>

void panic(KernelError error, const char* message = nullptr, uint32_t error_code = 0);

#endif 		// PANIC_H 