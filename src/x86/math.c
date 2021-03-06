 /*  Copyright (C) 2014 Xander Vedejas <xvedejas@gmail.com>
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
 *      Xander Vedejas <xvedejas@gmail.com>
 */
#include <math.h>

// some thanks to http://graphics.stanford.edu/~seander/bithacks.html

u32 floorlog2(u32 value)
{
    register u32 v = value; // 32-bit value to find the log2 of 
    const u32 b[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
    const u32 S[] = {1, 2, 4, 8, 16};
    register unsigned int result = 0;
    
    if (v & b[4])
    {
        v >>= S[4];
        result |= S[4];
    }
    if (v & b[3])
    {
        v >>= S[3];
        result |= S[3];
    }
    if (v & b[2])
    {
        v >>= S[2];
        result |= S[2];
    }
    if (v & b[1])
    {
        v >>= S[1];
        result |= S[1];
    }
    if (v & b[0])
    {
        v >>= S[0];
        result |= S[0];
    }
    return result;
}

/* The following function plus one gives the number of digits in a base-10
 * representation of the value. */
u32 floorlog10(u32 value)
{
    if (value == 0) return value;
    register u32 v = value; // non-zero 32-bit integer value to compute the log base 10 of 
    u32 t;          // temporary

    static u32 const powersOf10[] = 
        {1, 10, 100, 1000, 10000, 100000,
         1000000, 10000000, 100000000, 1000000000};

    t = (floorlog2(v) + 1) * 1233 >> 12; // (use a lg2 method from above)
    return t - (v < powersOf10[t]);
}

u32 lowestBit(u32 value)
{
    register u32 v = value; // 32-bit word input to count zero bits on right
    register u32 c = 32; // c will be the number of zero bits on the right
    v &= -(s32)(v);
    if (v) c--;
    if (v & 0x0000FFFF) c -= 16;
    if (v & 0x00FF00FF) c -= 8;
    if (v & 0x0F0F0F0F) c -= 4;
    if (v & 0x33333333) c -= 2;
    if (v & 0x55555555) c -= 1;
    return c;
}
