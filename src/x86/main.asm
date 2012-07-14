;  Copyright (C) 2012 Xander Vedėjas <xvedejas@gmail.com>
;
;  This program is free software: you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, either version 3 of the License, or
;  (at your option) any later version.
;
;  This program is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with this program.  If not, see <http://www.gnu.org/licenses/>.
;
;  Let's put all the menial stuff here like the ISRs, GDT, etc.
format ELF
include '../../inc/x86/asm.inc'
section '.text'
public start
public reboot
public halt
extern kmain
extern endThread
extern withinISR

header_magic equ 0x1BADB002
header_flags equ 7

; This part MUST be 4byte aligned
align 4
mboot:
    ; This is the GRUB Multiboot header
    dd header_magic
    dd header_flags
    dd -(header_magic + header_flags) ; header checksum
    dd 0  ; unused
    dd 0  ; unused
    dd 0  ; unused
    dd 0  ; unused
    dd 0  ; unused
    dd 0   ; graphics mode type, 0 = gfx, 1 = text
    dd 800 ; width
    dd 600 ; height
    dd 24  ; depth, number of bits per pixel
    
start:
    cli
    mov esp, stackPointer
    push esp
    push ebx ; multiboot info
    push eax ; multiboot magic, always equals 0x2BADBOO2
    push endThread ; this will be called when kmain returns
    jmp kmain

halt:
    cli
    hlt

startGdt:
    gdtLimitLow1    : dw 0
    gdtBaseLow1     : dw 0
    gdtBaseMiddle1  : db 0
    gdtAccess1      : db 0
    gdtGranularity1 : db 0
    gdtBaseHigh1    : db 0
    
    gdtLimitLow2    : dw 0xFFFF
    gdtBaseLow2     : dw 0
    gdtBaseMiddle2  : db 0
    gdtAccess2      : db 0x9A
    gdtGranularity2 : db 0xCF
    gdtBaseHigh2    : db 0
    
    gdtLimitLow3    : dw 0xFFFF
    gdtBaseLow3     : dw 0
    gdtBaseMiddle3  : db 0
    gdtAccess3      : db 0x92
    gdtGranularity3 : db 0xCF
    gdtBaseHigh3    : db 0
endGdt:
gp:
    gpLimit: rw 1
    gpBase: rb 1
public gdtInstall
gdtInstall:
    mov eax, endGdt
    sub eax, startGdt
    dec eax
    mov [gpLimit], eax
    mov eax, startGdt
    mov [gpBase], eax
gdtFlush:
    lgdt [gp]
    mov ax, 0x10 ; 0x10 is the offset in the GDT to our data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:flush2   ; 0x08 is the offset to our code segment: Far jump!
flush2:
    ret
    
extern idtp
public idtLoad
idtLoad:
    lidt [idtp]
    ret
rept 32 i:0
{
    public isr#i
    isr#i:
        cli
        push 0
        push i
        jmp isrCommonStub
}
    
extern faultHandler
; This is our common ISR stub. It saves the processor state, sets
; up for kernel mode segments, calls the C-level fault handler,
; and finally restores the stack frame.
isrCommonStub:
    pusha
    push ds
    push es
    push fs
    push gs
    mov eax, esp   ; Push us the stack
    push eax
    mov eax, faultHandler
    mov dword [withinISR], 1
    call eax       ; A special call, preserves the 'eip' register
    mov dword [withinISR], 0
    pop eax
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8     ; Cleans up the pushed error code and pushed ISR number
    sti
    iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP
    
rept 16 i:0
{
    public irq#i
    irq#i:
        cli
        push 0
        push 32 + i
        jmp irqCommonStub
}

extern irqHandler
irqCommonStub:
    pusha
    push ds
    push es
    push fs
    push gs
    mov eax, esp
    push eax
    mov eax, irqHandler
    mov dword [withinISR], 1
    call eax
    mov dword [withinISR], 0
    pop eax
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    sti
    iret

reboot:
    mov al, 0x02
@@: mov bl, al
    and bl, 0x02
    cmp bl, 0
    je @f
    in al, 0x64
    jmp @b
@@: mov al, 0xFE
    out 0x64, al
    cli
    hlt

section '.bss'
    rd 0x1000
stackPointer:
