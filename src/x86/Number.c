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
 
#include <main.h>
#include <vm.h>
#include <Number.h>
#include <String.h>
#include <cstring.h>
#include <mm.h>

#define value32(obj) (*((u32*)obj->data))

Object *integer32_new(Object *self, s32 value)
{
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

Object *integer32_add(Object *self, Object *other)
{
    printf("Adding %i and %i", value32(self), value32(other));
	return integer32_new(integer32Proto, value32(self) + value32(other));
	/// todo: check for overflow
}

Object *integer32_sub(Object *self, Object *other)
{
	return integer32_new(integer32Proto, value32(self) - value32(other));
	/// todo: check for underflow
}

Object *integer32_mul(Object *self, Object *other)
{
    printf("Multiplying %i and %i", value32(self), value32(other));
	return integer32_new(integer32Proto, value32(self) * value32(other));
	/// todo: check for overflow
}

Object *integer32_div(Object *self, Object *other)
{
	return integer32_new(integer32Proto, value32(self) / value32(other));
	/// todo: check for zero division
}

Object *integer32_eq(Object *self, Object *other)
{
	if (value32(self) == value32(other))
		return trueObject;
	return falseObject;
}

Object *integer32_gt(Object *self, Object *other)
{
	if (value32(self) > value32(other))
		return trueObject;
	return falseObject;
}

Object *integer32_lt(Object *self, Object *other)
{
	if (value32(self) < value32(other))
		return trueObject;
	return falseObject;
}

Object *integer32_gte(Object *self, Object *other)
{
	if (value32(self) >= value32(other))
		return trueObject;
	return falseObject;
}

Object *integer32_lte(Object *self, Object *other)
{
	if (value32(self) <= value32(other))
		return trueObject;
	return falseObject;
}

Object *integer32_toString(Object *self)
{
	char strBuffer[30];
	sprintf(strBuffer, "%i", value32(self));
	return string_new(stringProto, strdup(strBuffer));
}


void numberInstall()
{
    numberProto = send(objectProto, "new");
    
    /* integerProto */
    integerProto = send(numberProto, "new");
    Object *integerMT = methodTable_new(methodTableMT, 1);
    integerProto->methodTable = integerMT;
    
    methodTable_addClosure(integerMT, symbol("isInteger"),
        closure_newInternal(closureProto, returnTrue, 1));
    
    /// todo: automatic conversion between the different subtypes of integer
    
    integer32Proto = send(integerProto, "new");
    Object *integer32MT = methodTable_new(methodTableMT, 11);
    integer32Proto->methodTable = integer32MT;
    
    methodTable_addClosure(integer32MT, symbol("new:"),
        closure_newInternal(closureProto, integer32_new, 1));
    methodTable_addClosure(integer32MT, symbol("+"),
        closure_newInternal(closureProto, integer32_add, 2));
    methodTable_addClosure(integer32MT, symbol("-"),
        closure_newInternal(closureProto, integer32_sub, 2));
    methodTable_addClosure(integer32MT, symbol("*"),
        closure_newInternal(closureProto, integer32_mul, 2));
    methodTable_addClosure(integer32MT, symbol("/"),
        closure_newInternal(closureProto, integer32_div, 2));
    methodTable_addClosure(integer32MT, symbol("=="),
        closure_newInternal(closureProto, integer32_eq, 2));
    methodTable_addClosure(integer32MT, symbol(">"),
        closure_newInternal(closureProto, integer32_gt, 2));
    methodTable_addClosure(integer32MT, symbol("<"),
        closure_newInternal(closureProto, integer32_lt, 2));
    methodTable_addClosure(integer32MT, symbol(">="),
        closure_newInternal(closureProto, integer32_gte, 2));
    methodTable_addClosure(integer32MT, symbol("<="),
        closure_newInternal(closureProto, integer32_lte, 2));
    methodTable_addClosure(integer32MT, symbol("toString"),
        closure_newInternal(closureProto, integer32_toString, 1));
    
    // 
}
