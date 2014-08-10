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
#ifndef __StringMap_h__
#define __StringMap_h__

#include <main.h>

/* This is a hashtable implementation which uses c strings as keys */

typedef struct stringMapBucket
{
    String key;
    void *value;
    struct stringMapBucket *next;
} StringMapBucket;

typedef struct
{
    Size sizeA, sizeB, entriesA, entriesB;
    StringMapBucket *A;
    StringMapBucket *B;
} StringMap;

extern StringMap *stringMapNew();
extern bool stringMapSet(StringMap *map, String key, void *value);
extern void *stringMapGet(StringMap *map, String key);
extern void stringMapDebug(StringMap *map);

#endif
