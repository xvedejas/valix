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
 
#include <main.h>
#include <vm.h>
#include <Number.h>
#include <mm.h>

void numberInstall()
{
    numberProto = object_send(objectProto, symbol("new"));
    
    /* integerProto */
    
    integerProto = object_send(numberProto, symbol("new"));
    Object *integerMT = methodTable_new(methodTableMT, 1);
    integerProto->methodTable = integerMT;
    
    methodTable_addClosure(integerMT, symbol("isInteger"),
        closure_newInternal(closureProto, returnTrue, "oo"));
    
    // integer32Proto = object_send(
}

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
