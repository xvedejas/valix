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
#ifndef __VarList_h__
#define __VarList_h__

#include <main.h>

/* This is essentially a hash table mapping variable symbols to a list. Each
 * item in the list is basically a (world, value) tuple. */

typedef struct object Object;

typedef struct varListItem
{
    Object *world, *value;
    struct varListItem *next;
} VarListItem;

typedef struct varBucket
{
    Object *var;
    struct varListItem *next;
} VarBucket;

typedef struct
{
    Size size, capacity, entries;
    VarBucket buckets[0];
} VarList;

VarList *varListDataNew(Size size);
/* Set the value of a variable in a given world. If the world isn't in this
 * list yet, it returns false. */
extern bool varListDataSet(VarList *table, Object *var, Object *world, Object *value);
/* Get the value of a variable in a given world. If the world isn't found or
 * the variable isn't found, returns NULL */
extern Object *varListLookup(VarList *table, Object *var, Object *world);

#endif
