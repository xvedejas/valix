/*  Copyright (C) 2012 Xander Vedejas <xvedejas@gmail.com>
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

MethodTable *methodTableDataNew(Size number)
{
    Size buckets = number + (number >> 1);
    MethodTable *table = malloc(sizeof(MethodTable) +
        sizeof(MethodTableBucket) * buckets);
    table->capacity = number;
    table->entries = 0;
    memset(table->buckets, 0, sizeof(MethodTableBucket) * buckets);
    table->size = buckets;
    return table;
}

void methodTableDataAdd(MethodTable *table, Object *symbol, Object *method)
{
	table->entries++;
    assert(table->entries <= table->capacity,
        "methodTable error, did you make the methodTable large enough?");
    assert(table != NULL && symbol != NULL && method != NULL, "NULL error");
    Size capacity = table->capacity;
    assert(table->size != 0, "Method table %x is of size zero", table);
    Size hash = valueHash(symbol) % capacity;
    MethodTableBucket *buckets = table->buckets;
    while (buckets[hash][0] != NULL)
        hash = (hash + 1) % capacity;
    buckets[hash][0] = symbol;
    buckets[hash][1] = method;
}

Object *methodTableDataGet(MethodTable *table, Object *symbol)
{
	assert(table != NULL && symbol != NULL, "NULL error");
    Size capacity = table->capacity;
    assert(table->size != 0, "Method table %x is of size zero", table);
    Size hash = valueHash(symbol) % capacity;
    MethodTableBucket *buckets = table->buckets;
    Size start = hash;
    while (buckets[hash][0] != symbol)
    {
        if (buckets[hash][0] == NULL)
            return NULL;
        else
            hash = (hash + 1) % capacity;
        if (hash == start)
			return NULL;
	}
    return buckets[hash][1];
}

void methodTableDataDebug(MethodTable *table)
{
    printf(" ===[MethodTable %x capacity %i entries %i]===\n", table,
		table->capacity, table->entries);
    Size i, count = 0;
    for (i = 0; i < table->capacity; i++)
    {
        if (table->buckets[i][0] != NULL)
        {
            printf("key %18s value %S\n",
                table->buckets[i][0]->data,
                table->buckets[i][1]);
            count++;
		}
    }
    assert(count == table->entries, "methodTable error");
    printf(" ===[DONE]===\n");
}
