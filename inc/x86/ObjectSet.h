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
#ifndef __ObjectSet_h__
#define __ObjectSet_h__

#include <main.h>
#include <vm.h>

/* ObjectSet is an unordered set of vm objects. It is designed as a hash table
 * so that checking whether an item is in the set is a fast operation. */

typedef struct objectSetBucket
{
    Object *key;
    struct objectSetBucket *next;
} ObjectSetBucket;

typedef struct
{
    Size sizeA, sizeB, entriesA, entriesB;
    ObjectSetBucket *A;
    ObjectSetBucket *B;
} ObjectSet;

extern ObjectSet *objectSetNew();
extern bool objectSetAdd(ObjectSet *set, Object *obj);
extern bool objectSetRemove(ObjectSet *set, Object *obj);
extern bool objectSetHas(ObjectSet *set, Object *obj);
extern void objectSetDebug(ObjectSet *set);

#endif
