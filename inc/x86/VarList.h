/*  Copyright (C) 2011 Xander Vedejas <xvedejas@gmail.com>
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
#include <vm.h>

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

#endif
