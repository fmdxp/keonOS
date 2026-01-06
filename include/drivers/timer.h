#ifndef TIMER_H
#define TIMER_H

#include <kernel/constants.h>
#include <stdint.h>


bool timer_init(uint32_t frequency);
extern "C" void timer_handler();  
uint32_t timer_get_ticks();
void timer_sleep(uint32_t milliseconds);

#endif		// TIMER_H