 /*  Copyright (C) 2010 Xander Vedejas <xvedejas@gmail.com>
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
#ifndef __video_h__
#define __video_h__
#include <main.h>
#include <mm.h>

/* Big note on video implementation:
 * Video operations work on buffers. Everything should operate on a 32-bit argb
 * allocated buffer. The only function to use with the 24-bit rgb framebuffer
 * is copyRect, where the framebuffer may be the destination. Nowhere else are
 * operations with 24-bit rgb buffers supported. */

typedef struct buffer
{   u8 *mem;
    u32 bpp; /* bits per pixel */
    u32 width, height;
} Buffer;

typedef struct rect
{   u32 x, y;
    u32 width, height;
} Rect;

Buffer framebuffer;
bool videoInstalled;

#define copyBuffer(dest, src, xDest, yDest) \
    copyRect(dest, src, (Rect){0, 0, src.width, src.height}, xDest, yDest)
#define clrBuffer(buffer) \
    memsetd(buffer.mem, 0xFFFFFFFF, buffer.width * buffer.height)
#define xstep(buffer) (buffer.bpp / 8)
#define ystep(buffer) (xstep(buffer) * buffer.width)
#define buffersize(buffer) (xstep(buffer) * buffer.height * buffer.width)

extern void videoInstall(MultibootStructure *multiboot);
extern Buffer newBuffer(u32 width, u32 height);
extern Buffer newBitmapBuffer(u32 *bitmap, u32 width, u32 height);
extern void putPixel(Buffer buffer, u32 x, u32 y, u32 color);
extern void fillRect(Buffer buffer, Rect rect, u32 color);
extern void copyRect(Buffer dest, Buffer src, Rect srcRect, u32 x, u32 y);
extern u32 alphaBlend(u32 background, u32 color);
void printChar(char c);

#endif
