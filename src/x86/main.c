/*  Copyright (C) 2012 Xander Vedejas <xvedejas@gmail.com>
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
#include <cstring.h>
#include <stdarg.h>
#include <threading.h>
#include <types.h>
#include <parser.h>
#include <parser_tests.h>
#include <video.h>
#include <pci.h>
#include <keyboard.h>
#include <lexer.h>
#include <vm.h>
#include <acpi.h>
#include <rtl8139.h>

bool withinISR = false;
const Size systemStackSize = 0x1000;

u8 inb(u16 port)
{
    u8 rv;
    asm volatile("inb %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

void outb(u16 port, u8 data)
{
    asm volatile("outb %1, %0" :: "dN" (port), "a" (data));
}

u16 inw(u16 port)
{
    u16 rv;
    asm volatile("inw %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

void inws(u16 port, u16 *buffer, Size size)
{
    Size i;
    for (i = 0; i < size; i++)
        buffer[i] = inw(port);
}

void outw(u16 port, u16 data)
{
    asm volatile("outw %1, %0" :: "dN" (port), "a" (data));
}

void outws(u16 port, u16 *data, Size size)
{
    Size i;
    for (i = 0; i < size; i++)
        outw(port, data[i]);
}

u32 ind(u16 port)
{
    u32 rv;
    asm volatile("inl %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

void outd(u16 port, u32 data)
{
    asm volatile("outl %1, %0" :: "dN" (port), "a" (data));
}

/* Lets us print to serial out */
void debugInstall()
{   
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x80);
    outb(0x3f8 + 0, 0x03);
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x03);
    outb(0x3f8 + 2, 0xC7);
    outb(0x3f8 + 4, 0x0B);
}

void putch(u8 c)
{   
    while ((inb(0x3f8 + 5) & 0x20) == 0);
    if (c < 0x80)
        outb(0x3f8, c);
    else
        printf("[%x]", c);
    if (videoInstalled)
        printChar(c);
}

void put(String str)
{   
    int i = 0;
    while (str[i]) putch(str[i++]);
}

void putPadding(String str, Size padding)
{
	Size len;
	put(str);
	for (len = strlen(str); len < padding; len++)
		putch(' ');
}

volatile Size indention = 0;

void printf(const char *format, ...)
{
	Size i;
	for (i = 0; i < indention; i++)
	    put("--");
	if (format == NULL)
	{
        put("<<NULLSTR>>");
        return;
	}

    char numBuff[20];
    memset(numBuff, 0, sizeof(numBuff));
    
    Size padding = 0;
    bool inSpecifier = false;
    
    va_list argptr;
    va_start(argptr, format);
    
    while (*format)
    {   
        if (*format == '%')
        {   
            format++;
            inSpecifier = true;
        }
        else
        {
			putch(*format);
			format++;
		}
        
        padding = 0;
        while (inSpecifier)
		{
			inSpecifier = false;
			switch(*format)
			{   
				case '1' ... '9':
				{
					/* Print at least this width (pad with spaces) */
					String fmtNew;
					padding = strtoul((char*)format, &fmtNew, 10);
					format = fmtNew;
					inSpecifier = true;
				} continue;
				case 'X': case 'x': case 'h':
				{   
					u32 d = va_arg(argptr, u32);
					put("0x");
					itoa(d, numBuff, 16);
					putPadding(numBuff, padding);
					format++;
				} break;
				case 'b':
				{   
					u32 d = va_arg(argptr, u32);
					put("0b");
					itoa(d, numBuff, 2);
					putPadding(numBuff, padding);
					format++;
				} break;
				case 'i': case 'd':
				{   
					u32 d = va_arg(argptr, u32);
					itoa(d, numBuff, 10);
					putPadding(numBuff, padding);
					format++;
				} break;
				case 's':
				{   
					String s = va_arg(argptr, String);
					if (s == NULL)
						putPadding("<<NULLSTR>>", padding);
					else
						putPadding(s, padding);
					format++;
				} break;
				case 'S': // vm string object
				{
					Object *str = va_arg(argptr, Object*);
					Size i;
					StringData *data = str->data;
					Size len = data->len;
					String s = data->string;
					for (i = 0; i < len; i++)
						putch(s[i]);
					for ( ; i < padding; i++)
						putch(' ');
					format++;
				} break;
				case 'c':
				{   
					u8 c = va_arg(argptr, u32);
					putch(c);
					format++;
				} break;
				case '%':
				{   
					putch('%');
					format++;
				} break;
				case '\0': return;
				default: break;
			}
		}
    }
    va_end(argptr);
}

void _panic(char *file, u32 line)
{
    printf("\n\nPineapple pieces in brine...\n");
    printf("File: '%s'\nLine: %i\nThread: %s %i\n", file, line,
        getCurrentThread()->name, getCurrentThread()->pid);
    while (true) halt();
}

extern void schedule();
void timerHandler(Regs *r)
{
    timerTicks++;
    schedule();
}

void timerPhase(int hz)
{   
    int divisor = 1193180 / hz;       /* Calculate our divisor */
    outb(0x43, 0x36);             /* Set our command byte 0x36 */
    outb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    outb(0x40, divisor >> 8);     /* Set high byte of divisor */
}

u32 time()
{   
    return timerTicks;
}

void timerInstall()
{
    timerTicks = 0;
    irqInstallHandler(0, timerHandler);
    timerPhase(systemClockFreq);
}

void pci()
{
    u16 bus, dev, func;
    for (bus = 0; bus < 256; bus++)
    {
        for (dev = 0; dev < 32; dev++)
        {
            for (func = 0; func < 8; func++)
            {
                if (!pciCheckDeviceExists(bus, dev, func))
                    continue;

                // Get the basic PCI header information
                PciConfigHeaderBasic header =
                    pciGetBasicConfigHeader(bus, dev, func);

                // ONLY mess with the bars for header type 0
                if (header.headerType)
                    continue;

                //printf("\n[%x", (void*)(u32)header.vendorId);
                //printf(":%x] ", (void*)(u32)header.deviceId);
                //printf("rev: %u ", (void*)(u32)header.revisionId);
                //printf("class: %x\n\n", (void*)(u32)header.classBase);
                
                printf("device %x\n", header.deviceId);
                if (header.deviceId == 0x8139)
                {
                    //rtl_install(&header);
                    //rtl8139_probe(&header);
                    //rtl8139_invoke_interrupt(&header);
                }
                
                /*
                u8 bar;
                printf("BAR         Type      Value\n");
                printf("------------------------------------------------------------\n");
                for (bar = 0; bar <= 5; bar++)
                {
                    if (pciGetBarValue(bus, dev, func, bar) == 0)
                        continue;

                    printf("%u  ", bar);
                    if (pciGetBarType(bus, dev, func, bar) == pciMemory)
                        printf("Memory  ");
                    else
                        printf("I/O     ");

                    printf("%x", pciGetBarValue(bus, dev, func, bar));

                    printf("\n");
                }*/
            }
        }
    }
    printf("\nDone.\n");
}

ThreadFunc testVM()
{
    printf("mem used: %x\n", memUsed());
    String input = "| str | str = \"Hello, World!\n\". Console print: str.";
    printf("\n%s\n", input);
    u8 *bytecode = compile(input);
    printf("compiled.\n");
    interpretBytecode(bytecode); // uncomment to test interpreter
    printf("mem used: %x\n", memUsed());
}

/* This is the very first C function to be called. Here we initialize the various
 * parts of the system with *Install() functions. */
void kmain(u32 magic, MultibootStructure *multiboot, void *stackPointer)
{
    videoInstalled = false;
    debugInstall();
    printf("Valix OS Pre-Alpha - Built on " __DATE__ " " __TIME__
        "\nCompiled with gcc " __VERSION__ "...\n");
    gdtInstall();
    idtInstall();
    isrsInstall();
    irqInstall();
    timerInstall();
    //acpiInstall(); /// seems to have a bug
    videoInstall(multiboot);
    printf("video installed\n");
    mmInstall(multiboot);
    printf("mm installed\n");
    threadingInstall(stackPointer); // must be after mmInstall
    printf("threading installed\n");    
    
    vmInstall(); // must be after mmInstall
    printf("vm installed\n");
    keyboardInstall(); // should be after mmInstall (for use of getstring)
    printf("keyboard installed\n");
    asm volatile("sti;");
    printf("Welcome to Valix Pre-Alpha 1. Type \"help\" for usage information.\n\n");
    
    //compile_test();
    
    spawn("VM interactive shell", testVM);
    while (true);
    
    //pci();
    
    return; /* kills kernel thread */
}
