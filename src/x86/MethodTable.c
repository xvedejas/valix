/*  Copyright (C) 2011 Xander Vedejas <xvedejas@gmail.com>
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

#include <MethodTable.h>
#include <mm.h>
#include <data.h>
#include <vm.h>
#include <cstring.h>

/* The MethodTable data type is a hashtable mapping symbol objects to
 * method objects. Its size is static, so the number of methods to put into the
 * table must be known on allocation. */

MethodTable *methodTableDataNew(Size size)
{
    Size buckets = size + (size >> 1);
    MethodTable *table = malloc(sizeof(MethodTable) +
        sizeof(MethodTableBucket) * buckets);
    table->capacity = size;
    table->entries = 0;
    memsetd(table->buckets, 0, sizeof(MethodTableBucket) * buckets >> 4);
    table->size = buckets;
    return table;
}

void methodTableDataAdd(MethodTable *table, Object *symbol, Object *method)
{
    assert(++table->entries <= table->capacity,
        "methodTable error, did you make the methodTable large enough?");
    Size size = table->size;
    Size hash = valueHash(symbol) % size;
    MethodTableBucket *buckets = table->buckets;
    while (buckets[hash][0] != NULL)
        hash = (hash + 1) % size;
    buckets[hash][0] = symbol;
    buckets[hash][1] = method;
}

Object *methodTableDataGet(MethodTable *table, Object *symbol)
{
    Size size = table->size;
    Size hash = valueHash(symbol) % size;
    MethodTableBucket *buckets = table->buckets;
    while (buckets[hash][0] != symbol)
        if (buckets[hash][0] == NULL)
            return NULL;
        else
            hash = (hash + 1) % size;
    return buckets[hash][1];
}

void methodTableDataDebug(MethodTable *table)
{
    printf(" ===[MethodTable size %i]===\n", table->capacity);
    Size i;
    for (i = 0; i < table->capacity; i++)
    {
        if (table->buckets[i][0] != NULL)
            printf("key %x value %x\n",
                table->buckets[i][0],
                table->buckets[i][1]);
    }
    printf(" ===[DONE]===\n");
}
