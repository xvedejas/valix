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
#include <interrupts.h>
#include <string.h>

/* This array is actually an array of function pointers. We use
*  this to handle custom IRQ handlers for a given IRQ */
void *irqRoutines[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* Use this function to set an entry in the IDT. */
void idtSetGate(u8 num, u64 base, u16 sel, u8 flags)
{
    /* The interrupt routine's base address */
    idt[num].baseLow = (base & 0xFFFF);
    idt[num].baseHigh = (base >> 16) & 0xFFFF;

    /* The segment or 'selector' that this IDT entry will use
    *  is set here, along with any access flags */
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

/* Installs the IDT */
void idtInstall()
{
    idtp.limit = (sizeof(IdtEntry) * 256) - 1;
    idtp.base = (unsigned int)&idt;

    memset((u8*)&idt, 0, sizeof(IdtEntry) * 256);
    idtLoad();
}

String exceptionMessages[] =
{
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",

    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",        "Reserved",
    "Reserved",        "Reserved",
    "Reserved",
    
    "Reserved",        "Reserved",
    "Reserved",        "Reserved",
    "Reserved",        "Reserved",
    "Reserved",        "Reserved"
};

/* This installs a custom IRQ handler for the given IRQ */
void irqInstallHandler(int irq, void (*handler)(Regs *r))
{
    irqRoutines[irq] = handler;
}

/* This clears the handler for a given IRQ */
void irqUninstallHandler(int irq)
{
    irqRoutines[irq] = 0;
}

void irqRemap(void)
{
    outportb(0x20, 0x11); // PIC1: Initialization Command
    outportb(0xA0, 0x11); // PIC2: Initialization Command
    outportb(0x21, 0x20); // PIC1: IRQ0-7 starts at 0x20 (Interrupt 32)
    outportb(0xA1, 0x28); // PCI2: IRQ8-15 starts at 0x28 (Intterupt 40)
    outportb(0x21, 0x04); // PCI1:
    outportb(0xA1, 0x02); // PIC2:
    outportb(0x21, 0x01); // PIC1: x86 Mode
    outportb(0xA1, 0x01); // PCI2: x86 Mode
    outportb(0x21, 0x0);
    outportb(0xA1, 0x0);
}

#define idtSetIrqGate(n, m) idtSetGate(n, (unsigned)irq##m, 0x08, 0x8E)
void irqInstall()
{
    irqRemap();
    idtSetIrqGate(32, 0);  idtSetIrqGate(33, 1);  idtSetIrqGate(34, 2);
    idtSetIrqGate(35, 3);  idtSetIrqGate(36, 4);  idtSetIrqGate(37, 5);
    idtSetIrqGate(38, 6);  idtSetIrqGate(39, 7);  idtSetIrqGate(40, 8);
    idtSetIrqGate(41, 9);  idtSetIrqGate(42, 10); idtSetIrqGate(43, 11);
    idtSetIrqGate(44, 12); idtSetIrqGate(45, 13); idtSetIrqGate(46, 14);
    idtSetIrqGate(47, 15);
}

void irqHandler(Regs *r)
{
    /* This is a blank function pointer */
    void (*handler)(Regs *r);
    handler = irqRoutines[r->intNo - 32];
    if (handler)
        handler(r);
    if (r->intNo >= 40)
        outportb(0xA0, 0x20);
    /* send an EOI to the master interrupt controller */
    outportb(0x20, 0x20);
}

#define idtSetIsrGate(n) idtSetGate(n, (unsigned)isr##n, 0x08, 0x8E)
void isrsInstall()
{
    idtSetIsrGate(0);  idtSetIsrGate(1);  idtSetIsrGate(2);  idtSetIsrGate(3);
    idtSetIsrGate(4);  idtSetIsrGate(5);  idtSetIsrGate(6);  idtSetIsrGate(7);
    idtSetIsrGate(8);  idtSetIsrGate(9);  idtSetIsrGate(10); idtSetIsrGate(11);
    idtSetIsrGate(12); idtSetIsrGate(13); idtSetIsrGate(14); idtSetIsrGate(15);
    idtSetIsrGate(16); idtSetIsrGate(17); idtSetIsrGate(18); idtSetIsrGate(19);
    idtSetIsrGate(20); idtSetIsrGate(21); idtSetIsrGate(22); idtSetIsrGate(23);
    idtSetIsrGate(24); idtSetIsrGate(25); idtSetIsrGate(26); idtSetIsrGate(27);
    idtSetIsrGate(28); idtSetIsrGate(29); idtSetIsrGate(30); idtSetIsrGate(31);
}

void dumpRegs(Regs *r)
{
    printf("\ngs:  %x ", r->gs);
    printf("fs:  %x ", r->fs);
    printf("es:  %x ", r->es);
    printf("ds:  %x\n", r->ds);

    printf("edi: %x ", r->edi);
    printf("esi: %x ", r->esi);
    printf("ebp: %x ", r->ebp);
    printf("esp: %x\n", r->esp);

    printf("ebx: %x ", r->ebx);
    printf("edx: %x ", r->edx);
    printf("ecx: %x ", r->ecx);
    printf("eax: %x\n", r->eax);

    printf("IP: %x\n", r->eip);
}

void faultHandler(Regs *r)
{
    dumpRegs(r);
    if (r->intNo < 32)
    {   printf("----\n%s\n", exceptionMessages[r->intNo]);
        _panic("unknown", 0);
    }
}

