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
#include <linkedlists.h>

/* Note on implementation:
 * 
 * This is a hash table with an open-addressing scheme. It is static at 150% of
 * the minimum size to hold all entries.
 * 
 * Each bucket contains a symbol and a linked list, and each item in the linked
 * list is a (world, value) pair. When the VarList is initialized, all buckets
 * are created with empty linked lists.
 * 
 * ATTN: Currently it lacks a way to add worlds, this needs to be fixed before
 * anything will work because worlds are required.
 *  */

VarList *varListDataNew(Size capacity, void **symbols)
{
	/* Given an array of symbols, create a new varList with those
	 * symbols undefined */
    Size size = capacity + (capacity >> 1);
    VarList *table = calloc(sizeof(VarList) + sizeof(VarBucket) * size, 1);
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
    }
    
    return table;
}

VarList *varListDataNewPairs(Size capacity, void **symbols)
{
    /* Given an array of objects where each even-indexed one is a symbol
     * and each odd-indexed one is its value, create a new varlist */
    Size size = capacity + (capacity >> 1);
    VarList *table = calloc(sizeof(VarList) + sizeof(VarBucket) * size, 1);
    table->capacity = capacity;
    VarBucket *buckets = table->buckets;
    table->size = size;
    Object *world = currentWorld();
    
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
        VarListItem *item = malloc(sizeof(VarListItem));
        bucket->next = item;
        item->world = world;
        item->value = symbols[i * 2 + 1];
        item->next = NULL;
    }
    
    return table;
}

/* Set the value of a variable in a given world. If the var/world isn't in this
 * list yet, it returns false. */
bool varListDataSet(VarList *table, Object *var, Object *world, Object *value)
{
    Size size = table->size;
    if (size == 0) return false;
    Size hash = valueHash(var) % size;
    VarBucket *buckets = table->buckets;
    bool found = false;
    Size i;
    for (i = 0; i < size; i++)
    {
		if (buckets[hash].var == var)
		{
			found = true;
		    break;
		}
		hash = (hash + 1) % size;
	}
	if (!found) return false;
    VarBucket *bucket = &buckets[hash];
    
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
    /* If we did not find an entry for this world, create one */
	item = malloc(sizeof(VarListItem));
	item->next = bucket->next;
	bucket->next = item;
	item->world = world;
	item->value = value;
    return true;
}

/* Get the value of a variable in a given world. If the world isn't found or
 * the variable isn't found, returns NULL */
 /// todo: raise errors instead of returning NULL
Object *varListLookup(VarList *table, Object *var, Object *world)
{
    Size size = table->size;
    if (size == 0) return NULL;
    Size hash = valueHash(var) % size;
    VarBucket *buckets = table->buckets;
    bool found = false;
    Size i;
    for (i = 0; i < size; i++)
    {
        if (buckets[hash].var == NULL)
            return NULL;
        else if (buckets[hash].var == var)
        {
			found = true;
			break;
		}
        else
            hash = (hash + 1) % size;
	}
	if (!found) return NULL;
    VarListItem *item = buckets[hash].next;
    for (; item != NULL; item = item->next)
    {
        if (item->world == world)
            return item->value;
    }
    
    // Try again, extending the search to the parent world (and then its parent,
    // etc)
    
    while ((world = as(world, World)->parent) != NULL)
    {
        item = buckets[hash].next;
        
        for (; item != NULL; item = item->next)
        {
            if (item->world == world)
            {
                // We need to mark this access so that on world commit, we can
                // check for consistency. Don't add pair iff there is already an
                // identical pair.
                AccessPair *pair = ll_find2(buckets[hash].access_table,
                                ->accessing_world == world,
                                ->expected_value == item->value);
                if (pair == NULL)
                    return item->value;
                    
                pair = ll_append_new(buckets[hash].access_table, AccessPair);
                pair->accessing_world = world;
                pair->expected_value = item->value;
                return item->value;
            }
        }
    }
    return NULL;
}

bool varListCheckConsistent(VarList *table, Object *world)
{
    Size i;
    Object *parentWorld = as(world, World)->parent;
	VarBucket *buckets = table->buckets;
	for (i = 0; i < table->size; i++)
	{
		Object *variable = buckets[i].var;
		VarListItem *itemList = buckets[i].next;
        AccessPair *access_table = buckets[i].access_table;
        
        if (itemList == NULL)
            continue;
        
        VarListItem *item = ll_find(itemList, ->world == parentWorld);
        if (item == NULL)
            continue;
        
        // Go through the access table and make sure that the value for the
        // parent world is the same as all the accessed values
        
        bool check_consistent(AccessPair *pair)
        {
            if (pair->accessing_world == world)
                return pair->expected_value == variable;
            return true;
        }
        
        if (!ll_all(access_table, check_consistent))
            return false;
	}
	return true;
}

void varListDebug(VarList *table)
{
	Size i;
	VarBucket *buckets = table->buckets;
	printf("====VarList debug====\n");
	for (i = 0; i < table->size; i++)
	{
		Object *variable = buckets[i].var;
		VarListItem *item = buckets[i].next;
		if (variable != NULL)
		{
			printf("Variable '%s'\n", variable->data);
			while (item != NULL)
			{
				printf("    world %x value %x\n", item->world, item->value);
				item = item->next;
			}
		}
	}
	printf("====End VarList debug====\n");
}
