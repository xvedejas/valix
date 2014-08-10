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
#include <String.h>
#include <Array.h>
#include <data.h>
#include <mm.h>

/* The method 'map' expects the following from a subclass of sequence:
 * - a method of iterating through each item of that subclass once
 * - a method of knowing the count of items in that subclass
 * - a method of creating new sequences from a collection of items
 */
Object *sequence_map(Object *self, Object *block)
{
	Object *iterator = send(self, "iter");
	Size count = (Size)send(send(self, "count"), "toCInt");
	Object *array[count];
	Size i = 0;
	Object *item;
	while ((item = send(iterator, "next")) != NULL)
	{
		/// todo: use error handling instead of null value to end iteration
		array[i++] = send(block, ":", item);
	}
	return send(self, "new:", array);
}

// The following methods make sense only for ordered collections:
//
// reverse
// 

// The following methods make sense only for indexed collections:
//
// at:
// at:put:
// from:
// to:
// from:to:

// The following methods make sense only for "hashed" (?) collections:
//
// union:
// intersect:
// difference:

Object *array_new(Object *self, Object **objects, Size length)
{
    Object *array = object_new(self);
    ArrayData *data = malloc(sizeof(ArrayData) + sizeof(Object*) * length);
    Size i;
    for (i = 0; i < length; i++)
        data->objects[i] = objects[i];
    data->len = length;
    array->data = data;
    return array;
}

Object *sequence_toString(Object *self)
{
    ArrayData *data = self->data;
    Size i;
    Size len = data->len;
    Object *string = NULL;
    for (i = 0; i < len; i++)
    {
        Object *nextString = send(data->objects[i], "toString");
        if (string != NULL)
        {
            // Concatenate the two strings
            string = send(string, "+", nextString);
        }
        else
        {
            string = nextString;
        }
    }
    return string;
}

void arrayInstall()
{
	Object *sequenceMT = methodTable_new(methodTableMT, 3);
	sequenceProto = object_new(objectProto);
    sequenceProto->methodTable = sequenceMT;
	
	methodTable_addClosure(sequenceMT, symbol("new"),
		closure_newInternal(closureProto, newDisallowed, 1));
	methodTable_addClosure(sequenceMT, symbol("map:"),
		closure_newInternal(closureProto, sequence_map, 2));
	methodTable_addClosure(sequenceMT, symbol("toString"),
		closure_newInternal(closureProto, sequence_toString, 1));
    
    Object *arrayMT = methodTable_new(methodTableMT, 1);
    arrayProto = object_new(sequenceProto);
    arrayProto->methodTable = arrayMT;
    
    // Todo: have an array creation routine that can take anything iterable
    methodTable_addClosure(arrayMT, symbol("new"),
		closure_newInternal(closureProto, newDisallowed, 1));
    /*
    methodTable_addClosure(arrayMT, symbol("at:"),
		closure_newInternal(closureProto, array_at, 2));
    methodTable_addClosure(arrayMT, symbol("at:put:"),
		closure_newInternal(closureProto, array_atput, 3));
    methodTable_addClosure(arrayMT, symbol("reverse"),
		closure_newInternal(closureProto, array_reverse, 1));
    methodTable_addClosure(arrayMT, symbol("from:"),
		closure_newInternal(closureProto, array_from, 2));
    methodTable_addClosure(arrayMT, symbol("to:"),
		closure_newInternal(closureProto, array_to, 2));
    methodTable_addClosure(arrayMT, symbol("from:to:"),
		closure_newInternal(closureProto, array_fromto, 3));
    methodTable_addClosure(arrayMT, symbol("sort"),
		closure_newInternal(closureProto, array_sort, 1));
    methodTable_addClosure(arrayMT, symbol("copy"),
		closure_newInternal(closureProto, array_copy, 1));*/
}
