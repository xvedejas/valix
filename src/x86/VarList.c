/*  Copyright (C) 2014 Xander Vedejas <xvedejas@gmail.com>
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
#include <linkedlists.h>

/* Note on implementation:
 * 
 * This is a hash table with an open-addressing scheme. It is static at 150% of
 * the minimum size to hold all entries.
 * 
 * See World.h for semantics.
 * 
 *  */

VarList *varListNew(Size capacity, void **symbols)
{
	/* Given an array of symbols, create a new varList with those
	 * symbols undefined */
    Size size = capacity + (capacity >> 1); // hashtable size is 150% capacity
    VarList *table = malloc(sizeof(VarList) + sizeof(VarBucket) * size);
    table->capacity = capacity;
    VarBucket *buckets = table->buckets;
    table->size = size;
    
    Size hash;
    Size i;
    for (i = 0; i < capacity; i++)
    {
        void *var = symbols[i];
        hash = valueHash(var) % size;
        while (buckets[hash].var != NULL)
			hash = (hash + 1) % size;
        VarBucket *bucket = &table->buckets[hash];
        bucket->var = var;
        bucket->items = NULL;
    }
    
    return table;
}

VarList *varListNewPairs(Size capacity, void **symbols, Object *world)
{
    /* Given an array of altenrating symbols and values, create a new varList
     * with those symbols defined */
    Size size = capacity + (capacity >> 1); // hashtable size is 150% capacity
    VarList *table = malloc(sizeof(VarList) + sizeof(VarBucket) * size);
    table->capacity = capacity;
    VarBucket *buckets = table->buckets;
    table->size = size;
    
    Size hash;
    Size i;
    for (i = 0; i < capacity; i++)
    {
        void *var = symbols[i * 2];
        hash = valueHash(var) % size;
        while (buckets[hash].var != NULL)
			hash = (hash + 1) % size;
        VarBucket *bucket = &table->buckets[hash];
        bucket->var = var;
        bucket->items = malloc(sizeof(VarListItem));
        bucket->items->value = symbols[i * 2 + 1];
        bucket->items->world = world;
        bucket->items->next = NULL;
    }
    
    return table;
}

VarBucket *_varListGetBucket(VarList *table, Object *var)
{
    Size size = table->size;
    if (size == 0) return false; // avoid divide by zero; variable not found
    Size hash = valueHash(var) % size;
    VarBucket *buckets = table->buckets;
    bool found = false;
    Size i;
    for (i = 0; i < size; i++)
    {
		if (buckets[hash].var == var)
		{
			found = true; // found that the variable is defined in this varList
		    break;
		}
		hash = (hash + 1) % size;
	}
    if (found)
        return &buckets[hash];
    else
        return NULL;
}

/* Set the value of a variable in a given world. If the var isn't in this
 * list yet, it returns false. */
bool varListSet(VarList *table, Object *world, Object *var, Object *value)
{
    VarBucket *bucket = _varListGetBucket(table, var);
	if (bucket == NULL)
        return false; // variable not found
    VarListItem *item = bucket->items;
    
    /* Find our specific world. If it's not yet in this list, add it. */
    
    if (item == NULL)
    {
        bucket->items = malloc(sizeof(VarListItem));
        item = bucket->items;
    }
    else
    {
        while (true)
        {
            if (item->world == world)
            {
                item->value = value;
                return true;
            }
            else if (item->next == NULL)
                break;
            else
                item = item->next;
        }
        item->next = malloc(sizeof(VarListItem));
        item = item->next;
    }

    item->world = world;
    item->value = value;
    item->next = NULL;
    return true;
}

/* Get the value of a variable, but specifically in the outermost world which is
 * still an ancestor (or is identically) the given world.
 * 
 * Note that the asymptotic behavior of this function is O(1) if only one world
 * is in use, but worst case more like O(n^2) if n is the number of worlds.
 * 
 * */
Object *varListGet(VarList *table, Object *var, Object **world_ptr)
{
    VarBucket *bucket = _varListGetBucket(table, var);
	if (bucket == NULL)
        return NULL; // variable not found
    VarListItem *first_item = bucket->items;
    if (first_item == NULL)
        return NULL; // variable not defined
    VarListItem *item;
    Object *world = *world_ptr;
    
    // iterate through world lineage
    for (; world != NULL; world = world->world->parent)
    {
        // iterate through all world records for this variable
        for (item = first_item; item != NULL; item = item->next)
        {
            if (item->world == world)
            {
                *world_ptr = world;
                return item->value;
            }
        }
    }
    return NULL;
}

bool varListCheckConsistent(VarList *table, Object *world)
{
    return false;
}
