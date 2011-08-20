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
extrn assemblyPanic

func alphaBlend
    background equ [ebp+8]
    color      equ [ebp+12]
    
    mov eax, color
    mov ebx, background
    mov ecx, color
    shr ecx, 24
    jnz @f
    endfunc
@@: mov ch, cl
    not ch
    cmp cl, 0xFF
    jne @f
    mov eax, ebx
    endfunc
@@: mov edx, ebx
    shr edx, 24
    mov dh, dl
    not dh
    
    ; eax holds color
    xor ebx, ebx
    and eax, 0xFF
    mul ch
    shr eax, 8
    mov ebx, eax
    mov eax, background
    and eax, 0xFF
    mul cl
    shr eax, 8
    mul dh
    shr eax, 8
    add eax, ebx
    and eax, 0xFF
    push eax ; blue
    
    xor eax,eax
    xor ebx,ebx
    mov eax, color
    shr eax, 8
    and eax, 0xFF
    mul ch
    mov ebx, eax
    mov eax, background
    shr eax, 8
    and eax, 0xFF
    mul cl
    shr eax, 8
    mul dh
    add eax, ebx
    and eax, 0xFF00
    push eax ; green
    
    xor eax,eax
    xor ebx,ebx
    mov eax, color
    shr eax, 16
    and eax, 0xFF
    mul ch
    mov ebx, eax
    mov eax, background
    shr eax, 16
    and eax, 0xFF
    mul cl
    shr eax, 8
    mul dh
    add eax, ebx
    shl eax, 8
    and eax, 0xFF0000
    ; red
    
    pop ebx
    or eax, ebx
    pop ebx
    or eax, ebx
    
    mov ecx, color
    cmp ecx, background
    jle @f
    mov edx, color
    jmp .finish
@@: mov edx, background
.finish:
    and edx, 0xFF000000
    or eax, edx
endfunc
