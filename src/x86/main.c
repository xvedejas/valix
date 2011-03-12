/*  Copyright (C) 2011 Xander Vedejas <xvedejas@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Maintained by:
 *      Xander Vedėjas <xvedejas@gmail.com>
 */

#include <data.h>
#include <interrupts.h>
#include <main.h>
#include <string.h>
#include <stdarg.h>
#include <threading.h>
#include <types.h>
#include <parser.h>
#include <video.h>

const Size systemStackSize = 0x1000;

u8 inportb(u16 port)
{
    u8 rv;
    asm volatile("inb %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

void outportb(u16 port, u8 data)
{
    asm volatile("outb %1, %0" :: "dN" (port), "a" (data));
}

u16 inportw(u16 port)
{
    u16 rv;
    asm volatile("inw %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

void outportw(u16 port, u16 data)
{
    asm volatile("outw %1, %0" :: "dN" (port), "a" (data));
}

u32 inportd(u16 port)
{
    u32 rv;
    asm volatile("inl %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

void outportd(u16 port, u32 data)
{
    asm volatile("outl %1, %0" :: "dN" (port), "a" (data));
}

void debugInstall()
{   
    outportb(0x3f8 + 1, 0x00);
    outportb(0x3f8 + 3, 0x80);
    outportb(0x3f8 + 0, 0x03);
    outportb(0x3f8 + 1, 0x00);
    outportb(0x3f8 + 3, 0x03);
    outportb(0x3f8 + 2, 0xC7);
    outportb(0x3f8 + 4, 0x0B);
}

void putch(u8 c)
{   
    while ((inportb(0x3f8 + 5) & 0x20) == 0);
        outportb(0x3f8, c);
}

void put(String str)
{   
    int i = 0;
    while (*(str + i))
    {   
        while ((inportb(0x3f8 + 5) & 0x20) == 0);
        outportb(0x3f8, *(str + i++));
    }
}

void printf(const char *format, ...)
{   if (format == NULL)
        put("<<NULLSTR>>");

    char numBuff[20];
    memset(numBuff, 0, sizeof(numBuff));
    
    va_list argptr;
    va_start(argptr, format);
    for (; *format; format++)
    {   
        if (*format == '%')
        {   
            format++;
            switch(*format)
            {   
                case 'X': case 'x': case 'h':
                {   
                    u32 d = va_arg(argptr, u32);
                    put("0x");
                    itoa(d, numBuff, 16);
                    put(numBuff);
                } break;
                case 'b':
                {   
                    u32 d = va_arg(argptr, u32);
                    put("0b");
                    itoa(d, numBuff, 2);
                    put(numBuff);
                } break;
                case 'i': case 'd':
                {   
                    u32 d = va_arg(argptr, u32);
                    itoa(d, numBuff, 10);
                    put(numBuff);
                } break;
                case 's':
                {   
                    String s = va_arg(argptr, String);
                    if (s == NULL)
                        put("<<NULLSTR>>");
                    else
                        put(s);
                } break;
                case 'c':
                {   
                    u8 c = va_arg(argptr, u32);
                    putch(c);
                } break;
                case '%':
                {   
                    putch('%');
                } break;
                case '\0': return;
                default: break;
            }
        }
        else
            putch(*format);
    }
    va_end(argptr);
}

void _panic(String file, u32 line)
{
    printf("\n\nPineapple pieces in brine...\n");
    printf("File: '%s'\nLine: %i\nThread: %s %i\n", file, line, getCurrentThread()->name, getCurrentThread()->pid);
    halt();
}

void timerHandler(Regs *r)
{
    timerTicks++;
    schedule();
}

void timerPhase(int hz)
{   
    int divisor = 1193180 / hz;       /* Calculate our divisor */
    outportb(0x43, 0x36);             /* Set our command byte 0x36 */
    outportb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    outportb(0x40, divisor >> 8);     /* Set high byte of divisor */
}

u32 time()
{   
    return timerTicks; /* get fancier later, with formatting possibly */
}

void timerWait(int ticks) /* probably don't want to use this, use sleep() */
{   
    u32 eticks;
    eticks = timerTicks + ticks;
    while (timerTicks < eticks);
}

void timerInstall()
{
    timerTicks = 0;
    irqInstallHandler(0, timerHandler);
    timerPhase(systemClockFreq); /* set to milisecond precision */
}

ThreadFunc myThread()
{
    parse();
}

/* This is the very first C function to be called. Here we initialize the various
 * parts of the system with *Install() functions. */
void kmain(u32 magic, MultibootStructure *multiboot, void *stackPointer)
{
    gdtInstall();
    idtInstall();
    isrsInstall();
    irqInstall();
    timerInstall();
    debugInstall();
    printf("Valix OS Alpha - Built on " __DATE__ " " __TIME__
        "\nCompiled with gcc " __VERSION__ "...\n");
    mmInstall(multiboot);
    threadingInstall(stackPointer);
    videoInstall(multiboot);
    //keyboardInstall();
    asm volatile("sti;");
    //FileCoreInit();
    
    spawn("parser", myThread);
    
    return; /* kills kernel thread */
}
