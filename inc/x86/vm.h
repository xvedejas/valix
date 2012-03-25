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
            char *bytecode;
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

/* A trait is a null-terminated array of closures that represent methods to be
 * added to method tables on their creation. */
typedef Object **Trait;

struct object
{
    struct object *parent;
    struct object *methodTable;
    void *data;
};

#define symbol(str) (symbol_new(symbolProto, str))

Object *objectProto, *objectMT, *symbolProto, *methodTableMT, *varTableProto,
    *closureProto, *bindSymbol, *getSymbol, *trueObject, *falseObject, *newSymbol, *DNUSymbol;

extern Object *symbol_new(Object *self, String string);
extern Object *object_bind(Object *self, Object *symbol);
extern void vmInstall();
extern void methodTable_addClosure(Object *self, Object *symbol, Object *closure);
extern Object *closure_newInternal(Object *self, void *function, String argString);
extern Object *returnTrue(Object *self);
extern Object *returnFalse(Object *self);
extern Object *closure_with(Object *self, ...);

#define object_send(self, message, ...)\
({\
    Object *method = NULL;\
	method = object_bind(self, message);\
	if (method == NULL)\
	{\
		method = object_bind(self, symbol("doesNotUnderstand:"));\
		assert(method != NULL, "does not understand does not understand...\n");\
	}\
    (method == NULL)?\
        closure_with(method, self, message):\
        closure_with(method, self, ## __VA_ARGS__);\
})

#define send(obj, messagestr, ...)\
    object_send(obj, symbol(messagestr), ## __VA_ARGS__)

#endif
