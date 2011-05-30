/*  Copyright (C) 2010 Xander Vedejas <xvedejas@gmail.com>
 *  Conversion functions Copyright 1988 Regents of the University of California
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
#include <types.h>
#include <list.h>
#include <vm.h>
#include <mm.h>
#include <string.h>

/* This list implementation is primarily for use in the VM. It is not
 * necessary to check inputs, the VM should do this for us. */

const Size listDefaultSize = 4;
#define listNextSize(previousSize) (previousSize + (previousSize >> 1))

Object *listNew(Object *self)
{
    Object *list = new(self);
    list->list = malloc(sizeof(List));
    list->list->capacity = listDefaultSize;
    list->list->array = malloc(sizeof(void*) * list->list->capacity);
    list->list->items = 0;
    return list;
}

Object *listNewSize(Object *self, Size prealloc)
{
    Object *list = new(self);
    list->list = malloc(sizeof(List));
    list->list->capacity = max(prealloc, 2);
    list->list->array = malloc(sizeof(void*) * list->list->capacity);
    list->list->items = 0;
    return list;
}

void _resizeList(Object *list)
{
    assert(list->list->capacity == list->list->items, "List error");
    list->list->capacity = listNextSize(list->list->capacity);
    list->list->array = realloc(list->list->array, list->list->capacity * sizeof(void*));
}

/* [1, 2, 3] add: 5 ==> [1, 2, 3, 5] */
Object *listAdd(Object *self, void *value)
{
    if (self->list->capacity == self->list->items)
        _resizeList(self);
    self->list->array[self->list->items++] = value;
    return NULL;
}

/* [1, 2, 3] at: 1  --> 2
 * [1, 2, 3] at: -1 --> 3 */
Object *listAt(Object *self, Object *index)
{
    Size i = (Size)index->data;
    assert(i < self->list->items, "List error");
    return self->list->array[i];
}

/* [1, 2, 3] at: 1 put: 6 ==> [1, 6, 3] */
Object *listAtPut(Object *self, Object *index, void *value)
{
    Size i = (Size)index->data;
    assert(i < self->list->items, "List error");
    self->list->array[i] = value;
    return self;
}

/* List#pop removes an index, for example:
 * [1, 2, 3, 4] pop: 2 ==> [1, 2, 4] --> 3    */
Object *listPop(Object *self, Object *index)
{
    Size i = (Size)index->data;
    void *value = listAt(self, index);
    assert(i < self->list->items, "List error");
    memmove(self->list->array + i, self->list->array + i + 1, self->list->items - 1 - i);
    return value;
}

/* List#insert inserts before index, for example:
 * [1, 2, 3, 4] insert: 5 at: 2 ==> [1, 2, 5, 3, 4]*/
Object *listAtInsert(Object *self, Object *index, void *value)
{
    Size i = (Size)index->data;
    if (self->list->capacity == self->list->items)
        _resizeList(self);
    Size j;
    for (j = self->list->items; j > i; j--)
        self->list->array[j] = self->list->array[j - 1];
    self->list->array[i] = value;
    self->list->items++;
    return self;
}

Object *listAsString(Object *self)
{
    StringBuilder *s = stringBuilderNew("[");
    Size i;
    for (i = 0; i < self->list->items; i++)
    {
        stringBuilderAppend(s, send(self->list->array[i], asStringSymbol)->data);
        if (i + 1 < self->list->items)
            stringBuilderAppend(s, ", ");
    }
    stringBuilderAppendChar(s, ']');
    return stringNew(stringClass, stringBuilderToString(s));
}
