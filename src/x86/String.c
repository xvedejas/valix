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

void stringInstall()
{
    stringProto = object_send(objectProto, symbol("new"));
    
    /* Add a method table */
    Object *stringMT = methodTable_new(methodTableMT, 2);
    stringProto->methodTable = stringMT;
    
    // string.new
    methodTable_addClosure(stringMT, symbol("new:"),
        closure_newInternal(closureProto, string_new, "ooS"));
    // string.toString
    methodTable_addClosure(stringMT, symbol("toString"),
        closure_newInternal(closureProto, string_toString, "oo"));
}
