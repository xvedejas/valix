#include <keyboard.h>
#include <data.h>
#include <threading.h>
#include <interrupts.h>
#include <main.h>
#include <mm.h>

// THIS IS A TEMPORARY DRIVER
// this driver is for DEBUGGING only, so it's made to be very simple.
// the final driver will not be implemented in the kernel, and it will
// likely keep a buffer of keystrokes so fewer are missed.
// you have been warned.

volatile Keystroke lastStroke = (Keystroke){ .scancode = 0, .flags = 0 };
volatile u8 modifierFlags;

void keyboardInstall()
{
    modifierFlags = 0;
    irqInstallHandler(1, keyboardHandler);
}

typedef enum
{
    CTRL = 0,
    ALT = 1,
    SHIFT = 2
} ModifierFlagBit;

typedef enum
{
    isReleased,
    isEscaped
} KeyFlagBit;

void keyboardHandler(Regs *r)
{
    Keystroke key;
    key.flags = 0;
    key.scancode = inb(0x60);
    
    if (key.scancode == 0xE0)
    {
        key.scancode = inb(0x60);
        key.flags |= bit(isEscaped);
    }
    
    if (key.scancode & 0x80)
    {
        key.scancode &= ~0x80;
        key.flags |= bit(isReleased);
    }
    
    lastStroke = key;
}

u8 kbdqwerty[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,    /* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, /* Right shift */
    '*', 0, /* Alt */ ' ', 0, /* Caps lock */ '\x82', /* F1 key ... > */
    '\x83', '\x84', '\x85', '\x86', '\x87', '\x88', '\x89', '\x8A', '\x8B', /* < ... F10 */
    0, /* Num lock*/
    0, /* Scroll Lock */
    '\x8E',    /* Home key */
    '\x8F',    /* Up Arrow */
    '\x90',    /* Page Up */
    '-',
    '\x91',    /* Left Arrow */
    0,
    '\x92',    /* Right Arrow */
    '+',
    '\x93',    /* 79 - End key*/
    '\x94',    /* Down Arrow */
    '\x95',    /* Page Down */
    '\x96',    /* Insert Key */
    '\x97',    /* Delete Key */
    0, 0, 0,
    '\x98',    /* F11 Key */
    '\x99',    /* F12 Key */
    0,    /* All other keys are undefined */
};

u8 kbdqwertyshift[128] =
{
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,    /* Left shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, /* Right shift */
    '*', 0, /* Alt */ ' ', 0, /* Caps lock */ '\x82', /* F1 key ... > */
    '\x83', '\x84', '\x85', '\x86', '\x87', '\x88', '\x89', '\x8A', '\x8B', /* < ... F10 */
    0, /* Num lock*/
    0, /* Scroll Lock */
    '\x8E',    /* Home key */
    '\x8F',    /* Up Arrow */
    '\x90',    /* Page Up */
    '-',
    '\x91',    /* Left Arrow */
    0,
    '\x92',    /* Right Arrow */
    '+',
    '\x93',    /* 79 - End key*/
    '\x94',    /* Down Arrow */
    '\x95',    /* Page Down */
    '\x96',    /* Insert Key */
    '\x97',    /* Delete Key */
    0, 0, 0,
    '\x98',    /* F11 Key */
    '\x99',    /* F12 Key */
    0,    /* All other keys are undefined */
};


u8 translateQWERTY(Keystroke key)
{
    u8 value = 0;
    switch (key.scancode)
    {
        /* CTRL */
        case 0x1D:
        {
            if (key.flags & bit(isReleased))
                modifierFlags &= ~bit(CTRL);
            else
                modifierFlags |= bit(CTRL);
        } break;
        /* ALT */
        case 0x38:
        {
            if (key.flags & bit(isReleased))
                modifierFlags &= ~bit(ALT);
            else
                modifierFlags |= bit(ALT);
        } break;
        /* SHIFT */
        case 0x2A:
        case 0x36:
        {
            if (key.flags & bit(isReleased))
                modifierFlags &= ~bit(SHIFT);
            else
                modifierFlags |= bit(SHIFT);
        } break;
        /* Printable Characters */
        default:
        {
            if (key.flags & bit(isReleased))
                return 0;
            if (modifierFlags & bit(SHIFT))
                value = kbdqwertyshift[key.scancode];
            else
                value = kbdqwerty[key.scancode];
        } break;
    }
    return value;
}

u8 getchar()
{
    u8 c;
    while (true)
    {
        /* Input from serial port */
        if (inb(0x3f8 + 5) & 0x01)
            return inb(0x3f8);
            
        Keystroke key = lastStroke;
        lastStroke.scancode = 0;
        
        if (key.scancode == 0x00)
            continue;
        
        c = translateQWERTY(key);
        if (c != 0)
            return c;
    }
}

String getstring()
{
    /* Every 16 bytes we realloc the string */
    Size size = 16;
    String s = malloc(size);
    Size i = 0;
    Size end = 0;
    u8 c;
    for (c = getchar(); c != '\n' && c != '\r'; c = getchar())
    {
        switch (c)
        {
            case (u8)'\x91': // left arrow
            {
                if (!i) break;
                i--;
                putch(c);
            } break;
            case (u8)'\x92': // right arrow
            {
                if (i == end) break;
                i++;
                putch(c);
            } break;
            case (u8)'\x8E': // home
            {
                int n = i;
                while (n--)
                    putch('\x91'); // left
                i = 0;
            } break;
            case (u8)'\x93': // end
            {
                Size n = end;
                while (n --> i)
                    putch('\x92'); // right
                i = end;
            } break;
            case '\b': // backspace
            {
                if (i == 0) break;
                if (i == end)
                {
                    s[i] = 0;
                    end--;
                }
                else
                {
                    s[i] = ' ';
                }
                putch(c);
                i--;
            } break;
            default:
            {
                if (i >= size - 1)
                {
                    size += 16;
                    s = realloc(s, size);
                }
                putch(c);
                if (i == end)
                    end++;
                s[i] = c;
                i++;
            } break;
        }        
    }
    putch('\n');
    s[i] = '\0';
    return s;
}

