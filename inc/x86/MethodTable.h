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
#ifndef __MethodTable_h__
#define __MethodTable_h__

#include <vm.h>
#include <main.h>

typedef Object *MethodTableBucket[2];

typedef struct
{
    Size size; // each bucket is (2 * sizeof(Object*)) big
    Size capacity, entries; // capacity is constant, entries cannot exceed it
    MethodTableBucket buckets[0];
} MethodTable;

extern MethodTable *methodTableDataNew(Size size);
extern void methodTableDataAdd(MethodTable *table, Object *symbol, Object *method);
extern Object *methodTableDataGet(MethodTable *table, Object *symbol);
extern void methodTableDataDebug(MethodTable *table);

#endif
