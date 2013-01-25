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
 */
#include <main.h>
#include <parser.h>

// note that this code would not work because array has not been declared.
String input1 = "array = (1, 2, 3).";
// This is the expected output, typed by hand, for the given input.
u8 *output1 = (u8*)"\x01""array\x00\x00\x81""1\x00\x81""2\x00\x81""3\x00\x86\x03\x8B\x00\x8A";
// 01      number of symbols defined
// array   name of first symbol
// 00      end of first symbol
// 00      parseBlockHeader: no variables defined
// 81      integer
// 1 00    value given by string "1"
// 81 2 00 integer 2
// 81 3 00 integer 3
// 86      array
// 03      with three elements
// 8B 00   assignment to symbol 0
// 8A      stop
// (bytecode key in parser.h)

void compile_test()
{
    u8 *result = compile(input1);
    Size i;
    if (result != NULL)
    {
        for (i = 0; result[i] != EOF; i++)
        {
            if (result[i] == output1[i])
                printf("-> %x\n", result[i]);
            else
            {
                printf("!! %x != %x\n", result[i], output1[i]);
                panic("failed test 1\n");
                break;
            }
        }
    }
    printf("test passed!\n");
}
