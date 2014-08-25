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
#ifndef __Array_h__
#define __Array_h__
#include <main.h>
#include <vm.h>

typedef struct arrayData
{
    Size len;
    Object *objects[0];
} ArrayData;

typedef struct
{
    Size pos;
    Object *array;
} ArrayIterData;

Object *arrayProto, *sequenceProto;

extern void arrayInstall();
extern Object *array_new(Object *self, Object **objects, Size length);

#endif
