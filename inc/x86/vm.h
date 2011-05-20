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
#ifndef __vm_h__
#define __vm_h__
#include <main.h>
#include <threading.h>
#include <data.h>

typedef struct object
{
    /* Instead of having a tag, each object's underlying type is
     * determined by its lookup function, which is ultimately
     * responsable for all access to its members. */
    struct object *(*lookup)(struct object *self, struct object *symbol);
    struct object *vtable; /* methods */
    struct object *parent;
    void *data;     /* any associated data */
} Object;

typedef enum
{
    internalFunction,
    userDefinedClosure
} ClosureType;

typedef struct method
{
    ClosureType type;
    Size argc;
    union
    {
        void *funcptr;
        u8 *bytecode;
    };
} Method;

void vmInstall();
ThreadFunc execute(u8 *bytecode);

#endif
