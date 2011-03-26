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

void vmInstall();
ThreadFunc execute(String bytecode);

typedef struct object
{
    /* If an object (class) is abstract, it can be subclassed but cannot be
     * instantiated. Otherwise it is like a normal class. */
    bool isAbstract;
    /* An instant object is not a class object. Instances cannot define fields
     * (instead hold values) and cannot be instantiated */
    bool isInstance;
    union
    {
        Map *values; /* if isInstance. Values are mutable and protected. */
        Map *fields; /* if not isInstance. Fields are immutable and public. */
    };
} Object;

typedef struct scope
{
    Map *locals;
    Object **argStack;
    Size argStackTop;
} Scope;

#endif
