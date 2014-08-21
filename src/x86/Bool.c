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
#include <Bool.h>
#include <String.h>
#include <mm.h>
#include <cstring.h>

void booleanInstall()
{
    Object *booleanMT = object_send(methodTableMT, symbol("new:"), 10);
    Object *boolean = object_send(objectProto, newSymbol);
    boolean->methodTable = booleanMT;
    
    trueObject = object_send(boolean, symbol("new"));
    falseObject = object_send(boolean, symbol("new"));
    
    methodTable_addClosure(booleanMT, symbol("new"),
		closure_newInternal(closureProto, boolean_new, 1));
    methodTable_addClosure(booleanMT, symbol("ifTrue:"),
		closure_newInternal(closureProto, boolean_ifTrue, 2));
    methodTable_addClosure(booleanMT, symbol("ifFalse:"),
		closure_newInternal(closureProto, boolean_ifFalse, 2));
    methodTable_addClosure(booleanMT, symbol("ifTrue:ifFalse:"),
		closure_newInternal(closureProto, boolean_ifTrueifFalse, 3));
    methodTable_addClosure(booleanMT, symbol("and:"),
		closure_newInternal(closureProto, boolean_and, 2));
    methodTable_addClosure(booleanMT, symbol("or:"),
		closure_newInternal(closureProto, boolean_or, 2));
    methodTable_addClosure(booleanMT, symbol("xor:"),
		closure_newInternal(closureProto, boolean_xor, 2));
    methodTable_addClosure(booleanMT, symbol("=="),
		closure_newInternal(closureProto, boolean_eq, 2));
    methodTable_addClosure(booleanMT, symbol("not"),
		closure_newInternal(closureProto, boolean_not, 1));
    methodTable_addClosure(booleanMT, symbol("toString"),
		closure_newInternal(closureProto, boolean_toString, 1));
}

Object *boolean_new(Object *self)
{
	panic("Cannot use boolean as a prototype");
	return NULL;
}

Object *boolean_toString(Object *self)
{
	if (self == trueObject)
		return string_new(stringProto, strdup("true"));
	if (self == falseObject)
		return string_new(stringProto, strdup("false"));
	return string_new(stringProto, strdup("bool"));
	/// todo: delegate to super
	/*
	else
		return super(self, "toString");
		*/
}

Object *boolean_ifTrue(Object *self, Object *block)
{
	if (self == trueObject)
		return send(block, "eval");
	else
		return NULL;
}

Object *boolean_ifFalse(Object *self, Object *block)
{
	if (self == falseObject)
		return send(block, "eval");
	else
		return NULL;
}

Object *boolean_ifTrueifFalse(Object *self, Object *trueBlock,
							                Object *falseBlock)
{
	printf("trueBlock %S, falseBlock %S\n", trueBlock, falseBlock);
	if (self == trueObject)
		return send(trueBlock, "eval");
	else
		return send(falseBlock, "eval");
}

Object *boolean_and(Object *self, Object *other)
{
	if (self == trueObject)
		return other;
	else
		return self;
}

Object *boolean_or(Object *self, Object *other)
{
	if (self == trueObject)
		return self;
	else
		return other;
}

Object *boolean_xor(Object *self, Object *other)
{
	if (self == trueObject)
		return send(other, "not");
	else
		return other;
}

Object *boolean_eq(Object *self, Object *other)
{
	if (self == trueObject)
		return other;
	else
		return send(other, "not");
}

Object *boolean_not(Object *self, Object *other)
{
	if (self == trueObject)
		return falseObject;
	else
		return trueObject;
}
