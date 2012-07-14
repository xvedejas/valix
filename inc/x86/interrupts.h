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
#ifndef __interrupts_h__
#define __interrupts_h__

#include <main.h>

extern void idtLoad();

extern void *irqRoutines[16];

typedef struct regs
{
    u32 gs, fs, es, ds;
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 intNo, errCode;
    u32 eip, cs, eflags, useresp, ss;
} Regs;

/* Defines an IDT entry */
typedef struct idtEntry
{
    u16 baseLow;
    u16 sel;        /* Our kernel segment goes here! */
    u8 always0;     /* This will ALWAYS be set to 0! */
    u8 flags;       /* Set using the above table! */
    u16 baseHigh;
} __attribute__((packed)) IdtEntry;

typedef struct idtPtr
{
    unsigned short limit;
    unsigned int base;
} __attribute__((packed)) IdtPtr;

volatile IdtEntry idt[256];
volatile IdtPtr idtp;

void idtSetGate(u8 num, u64 base, u16 sel, u8 flags);
void idtInstall();
void irqInstallHandler(int irq, void (*handler)(Regs *r));
void irqUninstallHandler(int irq);
void irqRemap(void);
void irqInstall();
void irqHandler(Regs *r);
void isrsInstall();


extern void irq0(); extern void irq1(); extern void irq2(); extern void irq3();
extern void irq4(); extern void irq5(); extern void irq6(); extern void irq7();
extern void irq8(); extern void irq9(); extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();

extern void isr0(); extern void isr1(); extern void isr2(); extern void isr3();
extern void isr4(); extern void isr5(); extern void isr6(); extern void isr7();
extern void isr8(); extern void isr9(); extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();

#endif // INTERRUPTS_H

