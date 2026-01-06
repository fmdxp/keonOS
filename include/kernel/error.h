#ifndef KERNEL_ERROR_H
#define KERNEL_ERROR_H

#include <drivers/vga.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


enum class KernelError : uint32_t
{
    K_OK = 0,
    K_ERR_MULTIBOOT_FAILED,
	K_ERR_SYSTEM_INIT_FAILED,
	K_ERR_SYSTEM_PAGING_ENABLE_FAILED,
	K_ERR_SYSTEM_TIMER_INIT_FAILED,
	K_ERR_PAGE_FAULT,
	K_ERR_GENERAL_PROTECTION,
	K_ERR_DIVIDE_BY_ZERO,
	K_ERR_INVALID_OPCODE,
	K_ERR_OUT_OF_MEMORY,
	K_ERR_DEVICE_FAILURE,
    K_ERR_STACK_SMASHED,
	K_ERR_UNKNOWN_ERROR,
};

extern "C" void panic(KernelError error, const char* message, uint32_t error_code);

inline const char* kerror_to_str(KernelError err)
{
    switch(err)
    {
        case KernelError::K_OK: 								return "No Error: OK";
		case KernelError::K_ERR_MULTIBOOT_FAILED:               return "Not a Multiboot compliant MAGIC.";
        case KernelError::K_ERR_SYSTEM_INIT_FAILED: 			return "Failed system initialization.";
		case KernelError::K_ERR_SYSTEM_PAGING_ENABLE_FAILED:	return "Failed system paging initialization.";
		case KernelError::K_ERR_SYSTEM_TIMER_INIT_FAILED:		return "Failed system timer initialization.";
        case KernelError::K_ERR_PAGE_FAULT: 					return "Page Fault";
        case KernelError::K_ERR_GENERAL_PROTECTION: 			return "General Protection Fault";
        case KernelError::K_ERR_DIVIDE_BY_ZERO: 				return "Divide by Zero";
        case KernelError::K_ERR_INVALID_OPCODE: 				return "Invalid Opcode";
        case KernelError::K_ERR_OUT_OF_MEMORY: 					return "Out of Memory";
        case KernelError::K_ERR_DEVICE_FAILURE: 				return "Device Failure";
		case KernelError::K_ERR_STACK_SMASHED:                  return "Stack smashed!";
        case KernelError::K_ERR_UNKNOWN_ERROR:					return "Unknown Error";
        default: 												return "Unknown Kernel Error";
    }
}

inline void check_error(KernelError err, const char* msg = nullptr, uint32_t code = 0)
{
    if (err != KernelError::K_OK) panic(err, msg, code);
}

inline void log_error(KernelError err)
{
    const char* str = kerror_to_str(err);
    terminal_setcolor(vga_color_t(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    printf("%s", str);
    terminal_setcolor(vga_color_t(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

inline void handle_error(KernelError err, uint32_t error_code)
{
    const char* str = kerror_to_str(err);
    char buf[32];
    itoa(error_code, buf, 16);
    printf("%s", str);
    printf(" (code: 0x");
    printf("%s", buf);
    printf(")");
}

#endif 		// KERNEL_ERROR_H