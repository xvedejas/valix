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

#include <VarList.h>
#include <mm.h>
#include <data.h>
#include <vm.h>
#include <cstring.h>

VarList *varListDataNew(Size size)
{
    Size buckets = size + (size >> 1);
    VarList *table = malloc(sizeof(VarList) + sizeof(VarBucket) * buckets);
    table->capacity = size;
    table->entries = 0;
    memsetd(table->buckets, 0, sizeof(VarBucket) * buckets >> 4);
    table->size = buckets;
    return table;
}

/* Set the value of a variable in a given world. If the world isn't in this
 * list yet, it returns false. */
bool varListDataSet(VarList *table, Object *var, Object *world, Object *value)
{
    assert(++table->entries <= table->capacity, "varList error");
    Size size = table->size;
    Size hash = valueHash(var) % size;
    VarBucket *buckets = table->buckets;
    while (buckets[hash].var != NULL && buckets[hash].var != var)
        hash = (hash + 1) % size;
    VarBucket *bucket = &buckets[hash];
    bucket->var = var;
    
    /* Find world */
    VarListItem *item = bucket->next;
    while (item != NULL)
    {
        if (item->world == world)
        {
            item->value = value;
            return true;
        }
        item = item->next;
    }
    return false;
}

/* Get the value of a variable in a given world. If the world isn't found or
 * the variable isn't found, returns NULL */
 /// todo: raise errors instead of returning NULL
Object *varListLookup(VarList *table, Object *var, Object *world)
{
    Size size = table->size;
    Size hash = valueHash(var) % size;
    VarBucket *buckets = table->buckets;
    while (buckets[hash].var != var)
        if (buckets[hash].var == NULL)
            return NULL;
        else
            hash = (hash + 1) % size;
    VarListItem *item = buckets[hash].next;
    while (item != NULL)
    {
        if (item->world == world)
            return item->value;
        item = item->next;
    }
    return NULL;
}
