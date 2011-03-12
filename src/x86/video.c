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
#include <video.h>
#include <string.h>

void videoInstall(MultibootStructure *multiboot)
{
    // assert the presence of vbe info
    if (!(multiboot->flags & bit(12)))
        panic("Grub provides no framebuffer info");

    framebuffer.mem = (u32*)(u32)multiboot->framebufferAddr;
    framebuffer.bpp = multiboot->framebufferBpp;
    framebuffer.width = multiboot->framebufferWidth;
    framebuffer.height = multiboot->framebufferHeight;
}

Buffer newBuffer(u32 width, u32 height)
{
    Buffer buffer =
    {
        (u32*)malloc(sizeof(u32) * width * height), 32,
        width, height
    };
    clrBuffer(buffer);
    return buffer;
}

Buffer newBitmapBuffer(u32 *bitmap, u32 width, u32 height)
{
    return (Buffer)
    {   bitmap,
        32,
        width, height
    };
}

double ceil(double x)
{
    volatile register smax i;
    i = (smax)x;
    if (x == i) return x;
    if (i > 0) return (double)(i+1);
    else return (double)(i);
}

double floor(double x)
{
    volatile register smax i;
    i = (smax)x;
    if (x == i) return x;
    if (i > 0) return (double)(i);
    else return (double)(i-1);
}

double round(double x)
{
    if (x - floor(x) >= 0.5)
        return ceil(x);
    return floor(x);
}

u32 alphaBlendOld(u32 background, u32 color)
{
    u32 alpha = color >> 24;
    u32 invbgalpha = 0xFF - (background >> 24); // inverse background alpha

    if (alpha == 0x00) return color;
    if (alpha == 0xFF) return background;

    u32 rb = (((color & 0xFF00FF) * (0xFF - alpha)) +
        ((background & 0xFF00FF) * alpha * (invbgalpha / 0xFF))) & 0xFF00FF00;
    u32 g = (((color & 0x00FF00) * (0xFF - alpha)) +
        ((background & 0x00FF00) * alpha * (invbgalpha / 0xFF))) & 0x00FF0000;

    u32 a = min(color, background) & 0xFF000000;

    return a | ((rb | g) >> 8);
}

void drawLine(Buffer buffer, Point a, Point b, u32 color)
{
    double x0 = a.x,
        y0 = a.y,
        x1 = b.x,
        y1 = b.y;
    
    double slope = (y0 - y1) / (x0 - x1);
    
    // If our slope is between -1 and 1
    if (slope < 1 && slope > -1)
    {   
        if (x0 > x1)
        {
            drawLine(buffer, b, a, color);
            return;
        }
        int x;
        for (x = x0; x <= x1; x++)
        {
            
            double y = slope * delta( x, x0 ) + y0;
            
            u32 top = ceil(y),
                bottom = floor(y);
            
            double alpha = (0xFF - (color >> 24));
            
            u8 topAlpha = round(alpha * (ceil(y) - y)),
                bottomAlpha = round(alpha * (y - floor(y)));
            
            u32 topColor = (topAlpha << 24) | (color & 0xFFFFFF),
                bottomColor = (bottomAlpha << 24) | (color & 0xFFFFFF);
        
            buffer.mem[x + top * buffer.width] = topColor;
            buffer.mem[x + bottom * buffer.width] = bottomColor;
        }
    }
    // Otherwise, switch x's and y's
    else
    {
        slope = 1 / slope;
        if (y0 > y1)
        {
            drawLine(buffer, b, a, color);
            return;
        }
        int y;
        for (y = y0; y <= y1; y++)
        {
            double x = slope * delta( y, y0 ) + x0;
            
            u32 right = ceil(x),
                left = floor(x);
            
            double alpha = (0xFF - (color >> 24));
            
            u8 rightAlpha = round(alpha * (ceil(x) - x)),
                leftAlpha = round(alpha * (x - floor(x)));
            
            u32 rightColor = (rightAlpha << 24) | (color & 0xFFFFFF),
                leftColor = (leftAlpha << 24) | (color & 0xFFFFFF);
            
            buffer.mem[left + y * buffer.width] = leftColor;
            buffer.mem[right + y * buffer.width] = rightColor;
        }
    }
}
