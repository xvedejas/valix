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
#ifndef __vm_h__
#define __vm_h__
#include <main.h>
#include <threading.h>
#include <data.h>
#include <setjmp.h>

typedef struct object Object;

typedef enum
{
    internalClosure,
    userDefinedClosure
} closureType;

typedef struct closure
{
    closureType type;
    union
    {
        struct // internalClosure
        {
            String argString;
            Size argc;
            void *function;
        };
        struct // userDefinedClosure
        {
            
        };
    };
} Closure;

typedef struct stringData
{
    Size len;
    char string[0];
} StringData;

typedef struct array
{
    Size size;
    Object *array[0];
} Array;

struct object
{
    struct object *parent;
    struct object *methodTable;
    void *data;
};

#define symbol(str) (symbol_new(symbolProto, str))

Object *objectProto, *objectMT, *symbolProto, *methodTableMT, *varTableProto,
    *closureProto, *lookupSymbol;

extern Object *symbol_new(Object *self, String string);
extern void *object_send(Object *self, Object *message, ...);
extern Object *object_bind(Object *self, Object *symbol);
extern void vmInstall();

#endif
