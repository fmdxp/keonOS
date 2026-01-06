#ifndef CONSTANTS_H
#define CONSTANTS_H

// OS VERSION CONSTANTS

#define OS_NAME				"keonOS"
#define OS_VERSION_MAJOR	"0"
#define OS_VERSION_MINOR	"1"
#define OS_VERSION_PATCH	" "			// Uses letters

#define OS_VERSION_STRING 	OS_NAME " v" OS_VERSION_MAJOR "." OS_VERSION_MINOR OS_VERSION_PATCH



// C - CPP CONSTANTS

#ifndef NULL
	#ifdef __cplusplus
		#define NULL nullptr
	#else
		#define NULL ((void*)0)
	#endif
#endif



// VGA CONSTANTS

#define VGA_WIDTH		80
#define VGA_HEIGHT		25
#define VGA_MEMORY		0xB8000



// PAGING CONSTANTS

#define PAGE_SIZE 0x1000
#define PAGE_SIZE_4KB 4096

#define PTE_PRESENT   0x001  
#define PTE_RW        0x002  
#define PTE_USER      0x004  
#define PTE_PWT       0x008  
#define PTE_PCD       0x010  
#define PTE_ACCESSED  0x020  
#define PTE_DIRTY     0x040  
#define PTE_PAT       0x080  
#define PTE_GLOBAL    0x100  

#define PDE_PRESENT   0x001
#define PDE_RW        0x002
#define PDE_USER      0x004
#define PDE_PWT       0x008
#define PDE_PCD       0x010
#define PDE_ACCESSED  0x020
#define PDE_PAT       0x080
#define PDE_4MB       0x080  

#define PAGE_DIRECTORY_SIZE 1024
#define PAGE_TABLE_SIZE 1024



// IDT CONSTANTS

#define IDT_GATE_FLAGS 0x8E



// KEYBOARD CONSTANTS

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_BUFFER_SIZE 256

#define KEY_UP   0x11
#define KEY_DOWN 0x12



// SHELL CONSTANTS

#define SHELL_BUFFER_SIZE 	1024
#define SHELL_MAX_ARGS		32
#define MAX_HISTORY 		10



// SERIAL CONSTANTS

#define COM1 0x3F8



// TIMER CONSTANTS

#define PIT_CHANNEL0_DATA 0x40
#define PIT_CHANNEL1_DATA 0x41
#define PIT_CHANNEL2_DATA 0x42
#define PIT_COMMAND       0x43
#define PIT_FREQUENCY 1193182

#define PIC1_COMMAND 0x20
#define PIC_EOI      0x20



// THREAD CONSTANTS

#define THREAD_NOT_FOUND 0xFFFFFFFF
#define THREAD_AMBIGUOUS 0xFFFFFFFE



#endif 		// CONSTANTS_H
