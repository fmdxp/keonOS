#ifndef CONSTRUCTOR_H
#define CONSTRUCTOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*constructor)();

void initialize_constructors(void);

#ifdef __cplusplus
}
#endif

#endif 		// CONSTRUCTOR_H