#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

void serial_install();
int is_transmit_empty();
void write_serial(char a);
void serial_putc(char c);

#endif		// SERIAL_H