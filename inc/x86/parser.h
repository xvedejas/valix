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
 */
#ifndef __parser_h__
#define __parser_h__
#include <main.h>
#include <lexer.h>
#include <vm.h>

extern u8 *compile(String source);
extern const Size maxKeywordCount;

typedef enum
{
    integerBC  = 0x81, // create integer object (convert from string; next n bytes until null)
    doubleBC   = 0x82, // create double object (convert from string; next n bytes until null)
    stringBC   = 0x83, // create string object (next n bytes until null)
    charBC     = 0x84, // create character object (next byte)
    symbolBC   = 0x85, // create symbol object (next n bytes until null)
    arrayBC    = 0x86, // create array object (next byte tells how many elements to pop from stack)
    blockBC    = 0x87, /* create block object
                       * next byte tells how many arguments. Byte after that
                       * tells how many local variables, including the number of
                       * arguments. Immediately afterwards is a list of the
                       * names of arguments and variables. */
    variableBC = 0x88, // push value of variable (next byte)
    messageBC  = 0x89, /* send message
                        * number of arguments is the next byte(s)
                        * message on top of stack, then
                        * args on stack in reverse order, then receiver */
    stopBC     = 0x8A, // yield nil from expression
    setBC      = 0x8B, // set variable (next byte) to data on stack
    endBC      = 0x8C, // ends a block or file
    objectBC   = 0x8D, // object definition
    cascadeBC  = 0x8E, // cascading method calls
    EOFBC      = 0x8F, // end of file
    extendedBC8 = 0xF0, // 8 bits, for 8-bit values that are 0xF0 or greater
    extendedBC16 = 0xF1, // 16 bits
    extendedBC32 = 0xF2, // 32 bits
    extendedBC64 = 0xF3, // 64 bits
    extendedBC128 = 0xF4, // 128 bits
    // etc
} bytecodeCommand;

extern const String bytecodes[];
extern const Size EOF;

#endif
