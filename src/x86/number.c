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
#include <number.h>
#include <math.h>
#include <mm.h>
#include <types.h>
#include <string.h>

extern bool __attribute__((fastcall)) mulOverflow(u32 a, u32 b);

void integerAdd(Object *process)
{
    u32 b = pop()->number->data[0];
    u32 a = pop()->number->data[0];
    a += b;
    if (a < b) // overflow
        panic("Integer overflow; Not implemented");
    push(integerNew(a));
}

void integerSub(Object *process)
{
    u32 b = pop()->number->data[0];
    u32 a = pop()->number->data[0];
    a -= b;
    if (a > b) // underflow
        panic("Integer underflow; Not implemented");
    push(integerNew(a));
}

void integerMul(Object *process)
{
    u32 b = pop()->number->data[0];
    u32 a = pop()->number->data[0];
    
    /* mulOverflow returns whether there was an overflow */
    if (mulOverflow(a, b))
        panic("Integer overflow; Not implemented");
    push(integerNew(a * b));
}

void integerDiv(Object *process)
{
    u32 b = pop()->number->data[0];
    u32 a = pop()->number->data[0];
    
    if (b == 0)
        call(process, divideByZeroException, "raise", 0);
    
    if ((double)(a / b) == (double)a / (double)b)
        push(integerNew(a / b));
    else
        panic("Fractional numbers not yet implemented");
}

void integerMod(Object *process)
{
    u32 b = pop()->number->data[0];
    u32 a = pop()->number->data[0];
    
    if (b == 0)
        call(process, divideByZeroException, "raise", 0);
    
    push(integerNew(a % b));
}

void integerAsString(Object *process)
{
    u32 integer = pop()->number->data[0];
    Size digits = 1 + floorlog10(integer);
    String buffer = malloc(sizeof(char) * digits);
    itoa(integer, buffer, 10);
    Object *result = stringNew(strlen(buffer), buffer);
    push(result);
}

void integerAnd(Object *process)
{
    u32 a = pop()->number->data[0];
    u32 b = pop()->number->data[0];
    push(integerNew(a & b));
}

void integerOr(Object *process)
{
    u32 a = pop()->number->data[0];
    u32 b = pop()->number->data[0];
    push(integerNew(a | b));
}

void integerXor(Object *process)
{
    u32 a = pop()->number->data[0];
    u32 b = pop()->number->data[0];
    push(integerNew(a ^ b));
}

void integerExp(Object *process)
{
}

void integerFactorial(Object *process)
{
}

void integerToDo(Object *process)
{
    Object *block = pop();
    u32 stop = pop()->number->data[0];
    u32 start = pop()->number->data[0];
    for (; start < stop; start++)
    {
        /* Call apply: on block */
        push(integerNew(start));
        push(block);
        vApply(process);
        pop();
    }
    push(NULL);
}
