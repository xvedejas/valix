/*  Copyright (C) 2012 Xander Vedejas <xvedejas@gmail.com>
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
#include <video.h>
#include <cstring.h>
#include <fonts.h>

#define red(color) (color & 0xFF)
#define green(color) ((color >> 8) & 0xFF)
#define blue(color) ((color >> 16) & 0xFF)
#define alpha(color) ((color >> 24) & 0xFF)

const u32 fontWidth = 5,
          fontHeight = 9,
          fontHSpace = 2,
          fontVSpace = 1;

u32 fontX,
    fontY;

void videoInstall(MultibootStructure *multiboot)
{
    // assert the presence of vbe info
    assert(multiboot->flags & bit(12), "Grub provides no framebuffer info");
    assert(multiboot->framebufferType == 1, "Grub doesn't provide RGB video");
    
    framebuffer.mem = (u8*)(u32)multiboot->framebufferAddr;
    framebuffer.bpp = multiboot->framebufferBpp;
    framebuffer.width = multiboot->framebufferWidth;
    framebuffer.height = multiboot->framebufferHeight;
    
    /* Top-left corners of first character */
    fontX = 1,
    fontY = 1;
    videoInstalled = true;
}

inline u32 getColor(Buffer buffer, u32 pos)
{
    assert(pos < buffersize(buffer), "Index outside of range of video buffer");
    
	u32 color = 0x0;
    
	switch (buffer.bpp)
    {
		case 32:
		
		color |= buffer.mem[pos + 3] << 24;
		
		case 24:
		
		color |= buffer.mem[pos + 2] << 16;
        
        case 16:
        
		color |= buffer.mem[pos + 1] << 8;
        
        case 8:
        
		color |= buffer.mem[pos + 0];
		
		break;
		default:
		
		panic("pixel size %i not supported\n", buffer.bpp);
		
		break;
	}
	
    return color;
}

inline void setColor(Buffer buffer, u32 pos, u32 color)
{
    assert(pos < buffersize(buffer), "Index outside of range of video buffer");
    
    u32 oldColor  = getColor(buffer, pos);
    u32 newColor = alphaBlend(oldColor, color);
    
    switch (buffer.bpp)
    {
		case 32:
		
		buffer.mem[pos + 3] = newColor >> 24;
		
		case 24:
		
		buffer.mem[pos + 2] = (newColor >> 16) & 0xFF;
        
        case 16:
        
		buffer.mem[pos + 1] = (newColor >> 8) & 0xFF;
        
        case 8:
        
		buffer.mem[pos + 0] = newColor & 0xFF;
		
		break;
		default:
		
		panic("pixel size not supported\n");
		
		break;
	}
}

inline void putPixel(Buffer buffer, u32 x, u32 y, u32 color)
{
    //assert(x < buffer.width, "Index outside of range of video buffer");
    //assert(y < buffer.height, "Index outside of range of video buffer");
    setColor(buffer, x * xstep(buffer) + y * ystep(buffer), color);
}

u32 getPixel(Buffer buffer, u32 x, u32 y)
{
    //assert(x < buffer.width, "Index outside of range of video buffer");
    //assert(y < buffer.height, "Index outside of range of video buffer");
    return getColor(buffer, x * xstep(buffer) + y * ystep(buffer));
}

void drawChar(Buffer buffer, char c, u32 x, u32 y, u32 color)
{
    u8 *bitmap = (u8*)fontsIndex[(u32)c];
    u32 i, j;
    for (i = 0; i < fontHeight; i++)
    {
        for (j = 0; j < fontWidth; j++)
        {
            u8 alpha = bitmap[j + i * fontWidth];
            putPixel(buffer, x + j, y + i, color | (alpha << 24));
        }
    }
}

void scrollBufferUp(Buffer buffer, Size distance, u32 background)
{
    Size x, y;
    for (y = 0; y < buffer.height - distance; y++)
    {
        for (x = 0; x < buffer.width; x++)
        {
            putPixel(buffer, x, y, getPixel(buffer, x, y + distance));
        }
    }
    for ( ; y < buffer.height; y++)
    {
        for (x = 0; x < buffer.width; x++)
        {
            putPixel(buffer, x, y, background);
        }
    }
}

/* For printing directly to the framebuffer, for now... */
void printChar(char c)
{
    Size width = (fontWidth + fontHSpace);
    Size height = (fontHeight + fontVSpace);
    if (fontX + fontWidth > framebuffer.width)
    {
        fontX = 1;
        fontY += height;
    }
    if (fontY + fontHeight > framebuffer.height)
    {
        
        scrollBufferUp(framebuffer, height, 0x0);
        fontY -= height;
    }
    switch (c)
    {
        case '\n':
            fontX = 0;
            fontY += height;
        break;
        case '\t':
            printChar(' ');
            printChar(' ');
            printChar(' ');
            printChar(' ');
        return;
        case ' ':
            fontX += width;
        break;
        case '\b':
            if (fontX < fontWidth)
            {
                fontY -= height;
                fontX = framebuffer.width - width - (framebuffer.width % width);
                drawChar(framebuffer, c, fontX, fontY, 0x0);
            }
            else
            {
                fontX -= width;
                drawChar(framebuffer, c, fontX, fontY, 0x0);
            }
        break;
        default:
            drawChar(framebuffer, c, fontX, fontY, 0xFFFFFF);
            fontX += width;
        break;
    }
    
}
