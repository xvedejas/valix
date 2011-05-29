 /*  Copyright (C) 2010 Xander Vedejas <xvedejas@gmail.com>
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
#ifndef __list_h__
#define __list_h__
#include <main.h>
#include <vm.h>

typedef struct list
{
    void **array;
    Size items, capacity;
} List;

extern Object *listNew(Object *self);
extern Object *listNewSize(Object *self, Size prealloc);
/* [1, 2, 3] add: 5 ==> [1, 2, 3, 5] */
extern Object *listAdd(Object *list, void *value);
/* [1, 2, 3] at: 1  --> 2
 * [1, 2, 3] at: -1 --> 3 */
extern Object *listAt(Object *list, Object *index);
/* [1, 2, 3] at: 1 put: 6 ==> [1, 6, 3] */
extern Object *listAtPut(Object *list, Object *index, void *value);
/* List#pop removes an index, for example:
 * [1, 2, 3, 4] pop: 2 ==> [1, 2, 4]    */
extern Object *listPop(Object *list, Object *index);
/* List#insert inserts before index, for example:
 * [1, 2, 3, 4] insert: 5 at: 2 ==> [1, 2, 5, 3, 4]    */
extern Object *listAtInsert(Object *self, Object *index, void *value);

extern Object *listAsString(Object *self);

#endif
