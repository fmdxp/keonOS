#ifndef KSHELL_H
#define KSHELL_H

#include <stdint.h>

void shell_init();
void shell_run();
void shell_execute(const char* command);

#endif		// KSHELL_H