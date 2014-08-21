 /*  Copyright (C) 2014 Xander Vedejas <xvedejas@gmail.com>
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
 *      Xander Vedejas <xvedejas@gmail.com>
 */
#ifndef __VarList_h__
#define __VarList_h__

#include <main.h>
#include <vm.h>
#include <World.h>

typedef struct object Object;

/* This is essentially a hash table mapping variable symbols to a list of
 * (world, value) tuples. */

typedef struct varListItem
{
    /* If the current world used the value of this variable from the parent,
     * then we record that value under the parent world. */
    Object *world;
    // If the variable is as of yet undefined, value == NULL
    Object *value;
    struct varListItem *next; // next item in linked list.
} VarListItem;

typedef struct varBucket
{
    Object *var;
    struct varListItem *items; // start of linked list of items
} VarBucket;

typedef struct varList
{
    Size size, capacity;
    /* size: number of buckets (typically 1.5x the number of symbols)
     * capacity: number of symbols max we want to store in buckets */
    VarBucket buckets[0];
} VarList;

/* The symbols we want to create variables for must be specified at creation. */
extern VarList *varListNew(Size capacity, void **symbols);
extern VarList *varListNewPairs(Size capacity, void **symbols_and_values, Object *world);
extern bool varListSet(VarList *table, Object *world, Object *var, Object *value);
extern Object *varListGet(VarList *table, Object *var, Object **world_ptr);
extern bool varListCheckConsistent(VarList *table, Object *world);
#endif
