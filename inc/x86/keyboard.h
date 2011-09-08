 /*  Copyright (C) 2011 Xander Vedejas <xvedejas@gmail.com>
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
 #ifndef __keyboard_h__
#define __keyboard_h__
#include <main.h>
#include <data.h>
#include <interrupts.h>

typedef struct
{
    u8 scancode;
    /* For the following "flags" field,
     * 
     * bit 0 - is key being released?
     * bit 1 - is key after a 0xE0 escape code?
     */
    u8 flags;
} Keystroke;

void keyboardInstall();
void keyboardHandler(Regs *r);
String getstring();
u8 getchar();

#endif
