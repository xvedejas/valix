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

public setjmp
setjmp:
    mov [ecx+4], ebx
    mov [ecx+8], esi
    mov [ecx+12], edi
    mov [ecx+16], ebp
    mov [ecx+20], esp
    mov eax, [esp] ; return address
    mov dword [ecx], eax
    xor eax, eax
    ret
 
public longjmp
longjmp:
    mov ebx, [ecx+4]
    mov esi, [ecx+8]
    mov edi, [ecx+12]
    mov ebp, [ecx+16]
    mov esp, [ecx+20]
    add esp, 4 ; fix esp to account for ignored return address on stack
    mov eax, edx
    jmp dword [ecx]
