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

/* This is essentially a hash table mapping variable symbols to a list of
 * (world, value) tuples. */

typedef struct object Object;

typedef struct accessPair
{
    Object *accessing_world;
    Object *expected_value;
    struct accessPair *next;
} AccessPair;

typedef struct varListItem
{
    Object *world, *value;
    struct varListItem *next;
} VarListItem;

typedef struct varBucket
{
    Object *var;
    struct varListItem *next;
    // list of (world that read/set this value), (value expected by that world)
    // pairs.
    // If the world changed the value itself before reading it, then no entry
    // must be kept.
    AccessPair *access_table;
} VarBucket;

typedef struct
{
    Size size, capacity;
    /* size: number of buckets (typically 1.5x the number of symbols)
     * capacity: number of symbols max we want to store in buckets */
    VarBucket buckets[0];
} VarList;

/* The symbols we want to create variables for must be specified at creation. */
extern VarList *varListDataNew(Size capacity, void **symbols);
extern VarList *varListDataNewPairs(Size capacity, void **symbols);
/* Set the value of a variable in a given world. If the world isn't in this
 * list yet, it returns false. */
extern bool varListDataSet(VarList *table, Object *var, Object *world, Object *value);
/* Get the value of a variable in a given world. If the world isn't found or
 * the variable isn't found, returns NULL */
extern Object *varListLookup(VarList *table, Object *var, Object *world);
extern bool varListCheckConsistent(VarList *table, Object *world);
extern void varListDebug(VarList *table);

#endif
