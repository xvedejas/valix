;  Copyright (C) 2014 Xander Vedejas <xvedejas@gmail.com>
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
format ELF
section '.text'
include '../../inc/x86/asm.inc'

; This function is some magic to basically get past the C type system and pass
; arbitrary arguments of arbitrary type to a function of arbitrary type.
; (Instead, we rely on the VM's ability to check types and perform implicit
; conversions by sending messages to objects to obtain the desired C type if an
; object is given.)
; This function depends heavily on the calling conventions used by the compiler.
func callInternal
    function equ [ebp+8]
    argc     equ [ebp+12] ; including recipient. binary messages have argc=2
    va_list  equ [ebp+16]
    
    ; Note that we assume argc > 0 always
    
    mov ecx, argc         ; ecx = argc
    mov ebx, ecx          ; ebx = argc
    
    mov eax, va_list      ; eax = va_list;
    shl ebx, 2            ; ebx *= 4;
    add eax, ebx          ; eax += ebx;
    
top:                      ; while (ecx --> 0)
                          ; {
    sub eax, 4            ;     eax -= 4
    push dword [eax]      ;     push(eax);
    loop top              ; }
    
    call dword function   ; function();
    add esp, ebx          ; pop(argc);
endfunc
