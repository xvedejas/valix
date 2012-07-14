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
format ELF
section '.text'
include '../../inc/x86/asm.inc'

func memset    
    mov edi, [ebp+8] ; String argument
    mov eax, [ebp+12] ; character argument
    mov ecx, [ebp+16] ; count
    
    rep stosb
    
    mov eax, [ebp+8]
endfunc

func memsetd
    mov edi, [ebp+8] ; String argument
    mov eax, [ebp+12] ; u32 argument
    mov ecx, [ebp+16] ; count
    
    rep stosd
    
    mov eax, [ebp+8]
endfunc

func memcpy
    mov edi, [ebp+8] ; String argument, dest
    mov esi, [ebp+12] ; String argument 2, src
    mov ecx, [ebp+16] ; count
    rep movsb
    mov eax, [ebp+8]
endfunc

func memcpyd
    mov edi, [ebp+8] ; String argument, dest
    mov esi, [ebp+12] ; String argument 2, src
    mov ecx, [ebp+16] ; count
    
    rep movsd
    
    mov eax, [ebp+8]
endfunc

func strcpy
    mov edi, [ebp+8] ; String argument, dest
    mov esi, [ebp+12] ; String argument 2, src
    
@@: lodsb
    stosb
    cmp al, 0
    jne @b
    
    mov eax, [ebp+8]
endfunc

func strcat    
    mov esi, [ebp+8] ; String argument, dest
    
@@: lodsb
    cmp al, 0
    jne @b
    dec esi
    
    mov edi, esi
    mov esi, [ebp+12] ; String argument 2, src
    
@@: lodsb
    stosb
    cmp al, 0
    jne @b
    
    mov eax, [ebp+8]
endfunc
