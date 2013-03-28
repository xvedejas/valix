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
 
#include <main.h>
#include <vm.h>
#include <Number.h>
#include <mm.h>

Object *integer32_new(Object *self, s32 value)
{
	printf("i32 new\n");
    Object *new = object_send(self, symbol("new"));
    s32 *numberData = malloc(sizeof(s32));
    new->data = numberData;
    numberData[0] = (Size)value;
    return new;
}

Object *integer64_new(Object *self, s64 value)
{
    Object *new = object_send(self, symbol("new"));
    s64 *numberData = malloc(sizeof(s64));
    new->data = numberData;
    numberData[0] = value;
    return new;
}

Object *integer32_add(s32 self, s32 other)
{
	return integer32_new(integer32Proto, self + other);
	/// todo: check for overflow
}

Object *integer32_sub(s32 self, s32 other)
{
	return integer32_new(integer32Proto, self - other);
	/// todo: check for underflow
}

Object *integer32_mul(s32 self, s32 other)
{
	return integer32_new(integer32Proto, self * other);
	/// todo: check for overflow
}

Object *integer32_div(s32 self, s32 other)
{
	return integer32_new(integer32Proto, self / other);
	/// todo: check for zero division
}

Object *integer32_eq(s32 self, s32 other)
{
	if (self == other)
		return trueObject;
	return falseObject;
}

Object *integer32_gt(s32 self, s32 other)
{
	if (self > other)
		return trueObject;
	return falseObject;
}

Object *integer32_lt(s32 self, s32 other)
{
	if (self < other)
		return trueObject;
	return falseObject;
}

Object *integer32_gte(s32 self, s32 other)
{
	if (self >= other)
		return trueObject;
	return falseObject;
}

Object *integer32_lte(s32 self, s32 other)
{
	if (self <= other)
		return trueObject;
	return falseObject;
}

s32 integer32_asS32Int(Object *self)
{
	return *((s32*)self->data);
}

void numberInstall()
{
    numberProto = send(objectProto, "new");
    
    /* integerProto */
    
    integerProto = send(numberProto, "new");
    Object *integerMT = methodTable_new(methodTableMT, 1);
    integerProto->methodTable = integerMT;
    
    methodTable_addClosure(integerMT, symbol("isInteger"),
        closure_newInternal(closureProto, returnTrue, "oo"));
    
    /// todo: automatic conversion between the different subtypes of integer
    
    integer32Proto = send(integerProto, "new");
    Object *integer32MT = methodTable_new(methodTableMT, 12);
    integer32Proto->methodTable = integer32MT;
    
    methodTable_addClosure(integer32MT, symbol("new:"),
        closure_newInternal(closureProto, integer32_new, "os"));
    methodTable_addClosure(integer32MT, symbol("+"),
        closure_newInternal(closureProto, integer32_add, "oss"));
    methodTable_addClosure(integer32MT, symbol("-"),
        closure_newInternal(closureProto, integer32_sub, "oss"));
    methodTable_addClosure(integer32MT, symbol("*"),
        closure_newInternal(closureProto, integer32_mul, "oss"));
    methodTable_addClosure(integer32MT, symbol("/"),
        closure_newInternal(closureProto, integer32_div, "oss"));
    methodTable_addClosure(integer32MT, symbol("=="),
        closure_newInternal(closureProto, integer32_eq, "oss"));
    methodTable_addClosure(integer32MT, symbol(">"),
        closure_newInternal(closureProto, integer32_gt, "oss"));
    methodTable_addClosure(integer32MT, symbol("<"),
        closure_newInternal(closureProto, integer32_lt, "oss"));
    methodTable_addClosure(integer32MT, symbol(">="),
        closure_newInternal(closureProto, integer32_gte, "oss"));
    methodTable_addClosure(integer32MT, symbol("<="),
        closure_newInternal(closureProto, integer32_lte, "oss"));
    methodTable_addClosure(integer32MT, symbol("asS32Int"),
        closure_newInternal(closureProto, integer32_asS32Int, "so"));
    
    // 
}
