/*
 * keonOS - drivers/timer.cpp
 * Copyright (C) 2025-2026 fmdxp
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ADDITIONAL TERMS (Per Section 7 of the GNU GPLv3):
 * - Original author attributions must be preserved in all copies.
 * - Modified versions must be marked as different from the original.
 * - The name "keonOS" or "fmdxp" cannot be used for publicity without permission.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 */


#include <kernel/arch/x86_64/thread.h>
#include <kernel/arch/x86_64/idt.h>
#include <drivers/timer.h>
#include <stdio.h>

static volatile uint32_t timer_ticks = 0;
static volatile uint32_t timer_hz = 0;

bool timer_init(uint32_t frequency) 
{
	timer_hz = frequency;
    if (frequency == 0 || frequency > PIT_FREQUENCY) return false;
    
    uint32_t divisor = PIT_FREQUENCY / frequency;    
    outb(PIT_COMMAND, 0x36);
    
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);
    
    outb(PIT_CHANNEL0_DATA, low);
    outb(PIT_CHANNEL0_DATA, high);
    
    timer_ticks = 0;
    return true;
}


extern "C" void timer_handler() 
{
    outb(PIC1_COMMAND, PIC_EOI);
    timer_ticks++;    

    thread_t* current = thread_get_current();

    if (current)
    {
        thread_t* t = current;
        do
        {
            if (t->state == THREAD_SLEEPING) 
            {
                if (t->sleep_ticks > 0)
                    t->sleep_ticks--;
            
                if (t->sleep_ticks == 0)
                    t->state = THREAD_READY;
            }
            t = t->next;
        } while (t != current);
    }
    yield();
}


uint32_t timer_get_ticks() 
{
    return timer_ticks;
}


void timer_sleep(uint32_t milliseconds) 
{
    uint32_t ticks_to_wait = (milliseconds * timer_hz) / 1000;
    
    if (ticks_to_wait == 0 && milliseconds > 0) ticks_to_wait = 1;
    if (ticks_to_wait == 0) return;

    thread_t* current = thread_get_current();
    
    if (!current) 
    {
        uint32_t start = timer_ticks;
        while ((timer_ticks - start) < ticks_to_wait)
            asm volatile("pause");
        return;
    }

    asm volatile("cli");

    current->sleep_ticks = ticks_to_wait;
    current->state = THREAD_SLEEPING;

    asm volatile("sti");
    yield(); 
}