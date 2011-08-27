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
#include <Number.h>
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
        call(process, vmError, "raise:", symbolNew("Integer overflow not implemented"));
    push(integerNew(a));
}

void integerSub(Object *process)
{
    u32 b = pop()->number->data[0];
    u32 a = pop()->number->data[0];
    a -= b;
    if (a > b) // underflow
        call(process, vmError, "raise:", symbolNew("Integer underflow not implemented"));
    push(integerNew(a));
}

void integerMul(Object *process)
{
    u32 b = pop()->number->data[0];
    u32 a = pop()->number->data[0];
    
    /* mulOverflow returns whether there was an overflow */
    if (mulOverflow(a, b))
        call(process, vmError, "raise:", symbolNew("Integer overflow not implemented"));
    push(integerNew(a * b));
}

void integerDiv(Object *process)
{
    u32 b = pop()->number->data[0];
    u32 a = pop()->number->data[0];
    
    if (b == 0)
        call(process, divideByZeroException, "raise");
    
    if ((double)(a / b) == (double)a / (double)b)
        push(integerNew(a / b));
    else
        call(process, vmError, "raise:", symbolNew("NotImplemented"));
}

void integerMod(Object *process)
{
    u32 b = pop()->number->data[0];
    u32 a = pop()->number->data[0];
    
    if (b == 0)
        call(process, divideByZeroException, "raise");
    
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
    call(process, vmError, "raise:", symbolNew("NotImplemented"));
}

void integerFactorial(Object *process)
{
    call(process, vmError, "raise:", symbolNew("NotImplemented"));
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

void integerToByDo(Object *process)
{
    Object *block = pop();
    u32 stop = pop()->number->data[0];
    u32 step = pop()->number->data[0];
    u32 start = pop()->number->data[0];
    for (; start < stop; start += step)
    {
        /* Call apply: on block */
        push(integerNew(start));
        push(block);
        vApply(process);
        pop();
    }
    push(NULL);
}

void integerEq(Object *process)
{
    Object *arg = pop();
    Object *integer = pop();
    if (arg->number->data[0] == integer->number->data[0])
        push(trueObject);
    else
        push(falseObject);
}

void integerShiftLeft(Object *process)
{
    call(process, vmError, "raise:", symbolNew("NotImplemented"));
}

void integerShiftRight(Object *process)
{
    u32 b = pop()->number->data[0];
    u32 a = pop()->number->data[0];
    push(integerNew(a >> b));
}

void integerAtBit(Object *process)
{
    u32 b = pop()->number->data[0];
    u32 a = pop()->number->data[0];
    push(integerNew(a & bit(b)));
}

void integerNot(Object *process)
{
    u32 a = pop()->number->data[0];
    push(integerNew(~a));
}

void integerHighestBit(Object *process)
{
    u32 a = pop()->number->data[0];
    push(integerNew(floorlog2(a)));
}

void integerLowestBit(Object *process)
{
    u32 a = pop()->number->data[0];
    push(integerNew(lowestBit(a)));
}

void integerCeilingLog(Object *process)
{
    call(process, vmError, "raise:", symbolNew("NotImplemented"));
}

void integerFloorLog(Object *process)
{
    call(process, vmError, "raise:", symbolNew("NotImplemented"));
}

void integerSetup()
{
    integerProto = objectNew();
    methodList integerEntries =
    {
        "+", integerAdd,
        "-", integerSub,
        "*", integerMul,
        "/", integerDiv,
        "%", integerMod,
        "asString", integerAsString,
        "and:", integerAnd,
        "or:", integerOr,
        "xor:", integerXor,
        "^", integerExp,
        "factorial", integerFactorial,
        "to:do:", integerToDo,
        "to:by:do:", integerToByDo,
        "==", integerEq,
        "<<", integerShiftLeft,
        ">>", integerShiftRight,
        "atBit:", integerAtBit,
        "not", integerNot, // one's compliment
        "highestBit", integerHighestBit,
        "lowestBit", integerLowestBit,
        "ceilingLog:", integerCeilingLog,
        "floorLog:", integerFloorLog,
    };
    setInternalMethods(integerProto, 22, integerEntries);
}
