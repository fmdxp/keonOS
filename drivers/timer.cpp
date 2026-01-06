#include <kernel/arch/i386/idt.h>
#include <drivers/timer.h>
#include <proc/thread.h>

static uint32_t timer_ticks = 0;
static uint32_t timer_hz = 0;

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
    if (!current) return;

    asm volatile("cli");
    current->sleep_ticks = ticks_to_wait;
    current->state = THREAD_SLEEPING;
    asm volatile("sti");

    yield();
}