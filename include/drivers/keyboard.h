#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <kernel/constants.h>

bool keyboard_init();
char keyboard_getchar();
bool keyboard_has_input();
char keyboard_peek(); 


extern "C" void irq1_handler();
extern "C" void outb(uint16_t port, uint8_t value);
extern "C" uint8_t inb(uint16_t port);

extern "C" void keyboard_handler();

#endif		// KEYBOARD_H