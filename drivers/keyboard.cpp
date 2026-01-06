#include <drivers/keyboard.h>
#include <proc/thread.h>
#include <stdint.h>

static bool shift_pressed = false;
static bool caps_lock = false;

static const char scancode_to_ascii[] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, 
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, 
    '*',
    0, 
    ' ', 
    0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};


static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile int buffer_read_pos = 0;
static volatile int buffer_write_pos = 0;


bool keyboard_init() 
{
    buffer_read_pos = 0;
    buffer_write_pos = 0;

    while (inb(0x64) & 1) inb(0x60);
    outb(0x64, 0xAE);
    outb(0x60, 0xED);
    return true;
}


static char get_shift_variant(char c) 
{
    switch (c) 
    {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case ';': return ':';
        case '\'': return '"';
        case '`': return '~';
        case '\\': return '|';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        default: return c;
    }
}


bool keyboard_has_input() 
{
    return buffer_read_pos != buffer_write_pos;
}


char keyboard_peek() 
{
    if (!keyboard_has_input()) return 0;
    return keyboard_buffer[buffer_read_pos];
}


char keyboard_getchar() 
{    
    while (!keyboard_has_input())
    {
        thread_get_current()->state = THREAD_BLOCKED;
        yield();
    }
    asm volatile("cli");
    char c = keyboard_buffer[buffer_read_pos];
    buffer_read_pos = (buffer_read_pos + 1) % KEYBOARD_BUFFER_SIZE;
    asm volatile("sti");
    return c;
}


extern "C" void keyboard_handler() 
{
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    if (scancode & 0x80)
    { 
        uint8_t release_scancode = scancode & 0x7F;
        if (release_scancode == 0x2A || release_scancode == 0x36) shift_pressed = false;
        return;
    }

    char ascii = 0;

    switch (scancode)
    {
        case 0x2A: case 0x36:
            shift_pressed = true;
            return;
        
        case 0x3A:
            caps_lock = !caps_lock;
            return;
        
        case 0x0E:
            ascii = '\b';
            break;

        default:
            if (scancode < sizeof(scancode_to_ascii))
            {
                ascii = scancode_to_ascii[scancode];
                if (ascii >= 'a' && ascii <= 'z')
                {
                    if (ascii >= 'a' && ascii <= 'z')
                    {
                        if (shift_pressed ^ caps_lock) ascii -= 32;
                    }
                    
                    else if (shift_pressed) 
                        ascii = get_shift_variant(ascii);
                }   
            }
            break;
    }

    if (ascii != 0)
    {
        keyboard_buffer[buffer_write_pos] = ascii;
        buffer_write_pos = (buffer_write_pos + 1) % KEYBOARD_BUFFER_SIZE;
        thread_wakeup_blocked();
    }
}
