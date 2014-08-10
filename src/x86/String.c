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
 
#include <String.h>
#include <cstring.h>
#include <mm.h>

Object *string_new(Object *self, String val)
{
    Object *new = object_send(self, symbol("new"));
    Size len = strlen(val);
    StringData *stringData = malloc(sizeof(StringData) + sizeof(char) * len);
    new->data = stringData;
    stringData->len = len;
    memcpy(stringData->string, val, len);
    return new;
}

Object *string_toString(Object *self)
{
	return self;
}

Object *string_concat(Object *self, Object *other)
{
    StringData *selfData = self->data;
    StringData *otherData = other->data;
    Size len = selfData->len;
    Size otherlen = otherData->len;
    Size newlen = len + otherData->len;
    
    StringData *newData = malloc(sizeof(StringData) + sizeof(char) * newlen);
    newData->len = newlen;
    
    memcpy(newData->string, selfData->string, len);
    memcpy(newData->string + len, otherData->string, otherlen);
    
    Object *newString = object_send(self, symbol("new"));
    newString->data = newData;
    return newString;
}

void stringInstall()
{
    stringProto = object_send(objectProto, symbol("new"));
    
    /* Add a method table */
    Object *stringMT = methodTable_new(methodTableMT, 3);
    stringProto->methodTable = stringMT;
    
    // string.new
    methodTable_addClosure(stringMT, symbol("new:"),
        closure_newInternal(closureProto, string_new, 2));
    // string.toString
    methodTable_addClosure(stringMT, symbol("toString"),
        closure_newInternal(closureProto, string_toString, 1));
    // string +
    methodTable_addClosure(stringMT, symbol("+"),
        closure_newInternal(closureProto, string_concat, 2));
}
