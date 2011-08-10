;  Copyright (C) 2010 Xander Vedėjas <xvedejas@gmail.com>
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
    mov [ecx], ebp
    mov [ecx+4], esi
    mov [ecx+8], edi
    mov [ecx+12], ebx
    mov eax, [esp]
    mov [ecx+16], eax
    xor eax, eax
    ret

public longjmp
longjmp:
    mov ebp, [ecx]
    mov esi, [ecx+4]
    mov edi, [ecx+8]
    mov ebx, [ecx+12]
    mov [esp+8], eax
    jmp dword [ecx+16]
