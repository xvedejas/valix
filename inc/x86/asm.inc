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

; This is a FASM include file, which is useful for common assembly macros.

; The following macros allow easy calling of C functions and creation
; of assembly functions that may be called by C code.

extern fix extrn
struct fix struc

macro func name
{
    public name
    name:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
}

macro endfunc
{
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
}

macro invoke name,[arg]
{
    common
        push ecx
        push edx
    reverse
        push arg
    common
        call name
    forward
        add esp, 4
    common
        pop edx
        pop ecx
}
