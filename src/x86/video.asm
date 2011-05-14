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

func fillRect
    ; Buffer buffer, Rect rect, u32 color
    ; arguments:
    bufferMem    equ [ebp+8]
    bufferBpp    equ [ebp+12]
    bufferWidth  equ [ebp+16]
    bufferHeight equ [ebp+20]
    rectX        equ [ebp+24]
    rectY        equ [ebp+28]
    rectW        equ [ebp+32]
    rectH        equ [ebp+36]
    color        equ [ebp+40]
    
    mov edi, bufferMem
    
    mov ecx, rectH
    ; ecx now holds count-down
    
    ; mem += (buffer.width * y + x) * buffer.bpp / 8
    mov ebx, bufferBpp
    shr ebx, 3         ; / 8
    mov eax, bufferWidth
    mul dword rectY
    add eax, rectX
    mul ebx            ; * buffer.bpp / 8
    add edi, eax       ; mem +=
    ; edi now properly incremented
    
    mov ebx, color
    
    xor edx, edx ; up-count
@@: mov eax, ebx ; color
    
    ; We want to alpha blend, get current color at edi
    mov eax, [edi]
    ; bottom color in eax, top in ebx
    invoke alphaBlend, eax, ebx
    ; new desired in eax
    stosd

    inc edx
    cmp edx, rectW ; test to see if we are at end of horizontal
    jne @b
        ; at end of horizontal, edi += (buffer.width - rect.width) * buffer.bpp / 8
        mov eax, bufferWidth  ; * buffer.width
        sub eax, rectW
        mul dword bufferBpp
        shr eax, 3         ; / 8
        add edi, eax
        
        xor edx, edx       ; reset up-count
    loop @b    ; dec ecx, jump if ecx != 0
endfunc

; void copyRect(Buffer dest, Buffer src, Rect rect)
func copyRect
    ; arguments:
    destMem    equ [ebp+8]
    destBpp    equ [ebp+12]
    destWidth  equ [ebp+16]
    destHeight equ [ebp+20]
    srcMem     equ [ebp+24]
    srcBpp     equ [ebp+28]
    srcWidth   equ [ebp+32]
    srcHeight  equ [ebp+36]
    srcRectX   equ [ebp+40]
    srcRectY   equ [ebp+44]
    srcRectW   equ [ebp+48]
    srcRectH   equ [ebp+52]
    xDest      equ [ebp+56]
    yDest      equ [ebp+60]
    
    mov edi, destMem
    mov esi, srcMem
    
    ; dest.mem += (buffer.width * yDest + xDest) * buffer.bpp / 8
    mov ebx, destBpp
    shr ebx, 3         ; / 8
    mov eax, destWidth
    mul dword yDest
    add eax, xDest
    mul ebx            ; * dest.bpp / 8
    add edi, eax       ; mem +=
    ; edi now properly incremented
    
    ; src.mem += (buffer.width * y + x) * buffer.bpp / 8
    mov ebx, srcBpp
    shr ebx, 3         ; / 8
    mov eax, srcWidth
    mul dword srcRectY
    add eax, srcRectX
    mul ebx            ; * src.bpp / 8
    add esi, eax       ; mem +=
    ; esi now properly incremented
    
    mov ecx, srcRectH  ; ecx = rect.height
    ; ecx now holds count-down
    
    xor edx, edx ; up-count
.top:
    lodsd
    mov ebx, [edi] ; eax now holds upper color, ebx holds lower color
    
    cmp dword destBpp, 24 ; if (dest.bpp == 24)
    jne @f
        and ebx, 0xFFFFFF ; make sure the 24-bit is considered 100% opaque
    @@: 
    
    invoke alphaBlend, ebx, eax ; blend, then store
    
    cmp dword destBpp, 32 ; if (dest.bpp == 32)
    jne @f
        stosd
        jmp .endif2
    @@: ; else if (dest.bpp == 24)
        stosb
        shr eax, 8
        stosb
        shr eax, 8
        stosb
    .endif2:
    
    inc edx
    cmp edx, srcRectW ; test to see if we are at end of horizontal
    jne .top
        ; at end of horizontal, edi += (dest.width - rect.width) * dest.bpp / 8
        mov eax, destWidth  ; * dest.width
        sub eax, srcRectW
        mul dword destBpp ; dest.bpp
        shr eax, 3         ; / 8
        add edi, eax
        
        ; at end of horizontal, esi += (src.width - rect.width) * 4
        mov eax, srcWidth  ; * src.width
        sub eax, srcRectW
        shl eax, 2
        add esi, eax
        
        xor edx, edx       ; reset up-count
    loop .top    ; dec ecx, jump if ecx != 0
endfunc
