;  Copyright (C) 2011 Xander Vedėjas <xvedejas@gmail.com>
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
; Instead, we rely on the VM's ability to check types and perform implicit
; conversions by sending messages to objects to obtain the desired C type if an
; object is given instead.
func callInternal
    function equ [ebp+8]
    argc     equ [ebp+12]
    self     equ [ebp+16]
    va_list  equ [ebp+20]
    
    mov ecx, argc
    mov ebx, ecx
    
    cmp ecx, 0
    je @f
    
    mov eax, va_list
    ; eax += (ebx = 4*ecx)
    shl ebx, 2
    add eax, ebx
    
top:    
    lea eax, [eax-4]
    push dword [eax]
    loop top

@@: push dword self
    
    call dword function
    add esp, ebx
    add esp, 4
endfunc
