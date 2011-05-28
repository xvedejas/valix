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
    struct object *vtable; /* methods */
    struct object *parent;
    void *data; /* any associated data. In vtables, it's a Map<symbol,method> */
} Object;

typedef enum
{
    internalFunction,
    userDefinedClosure
} ClosureType;

typedef enum
{
    kernelPermissions,
    rootPermissions,
    userPermissions
} Permissions;

/* The method struct is the payload for a closure object */
typedef struct method
{
    ClosureType type;
    Size argc;
    union
    {
        /* For internal functions only */
        struct
        {
            void *funcptr;
            Permissions permissions;
        };
        /* For user-defined closures only */
        struct
        {
            Map *translationTable;
            Object *bytearray;
            Object *containingScope; // where this was declared, if anywhere
        };
    };
} Method;

/* This is the payload for a scope object, which should be useful in
 * error handling and stack trace stuff. */
typedef struct scope
{
    Object *parent;
    Object *closure;
    Map *vars; /* <symbol,object> */
} Scope;

/* The following are some useful globals */

/* Basic objects */
Object *objectClass,
       *scopeClass,
       *symbolClass,
       *boolClass,
       *numberClass,
       *stringClass,
       *closureClass,
       *bytearrayClass,
       *processClass,
       *consoleClass;

/* Symbols */
Object *newSymbol,
       *lengthSymbol,
       *newSymbolZArgs,
       *lookupSymbol,
       *addMethodSymbol,
       *allocateSymbol,
       *delegatedSymbol,
       *internSymbol,
       *doesNotUnderstandSymbol,
       *consoleSymbol,
       *printSymbol,
       *asStringSymbol,
       *callSymbol,
       *equivSymbol,
       *whileSymbol,
       *lessThanSymbol,
       *greaterThanSymbol,
       *selfSymbol,
       *executeSymbol;

/* Given an object and the symbol representing a message, send the
 * message with some number of arguments */
#define send(self, messageName, args...) \
    ({\
        Object *_self = self;\
        doMethod(_self, bind(_self, messageName), ## args);\
    })\

Object *doMethod(Object *self, Object *methodClosure, ...);
Object *bind(Object *self, Object *messageName);

void vmInstall();
Object *processNew(Object *self);
Object *processExecute(Object *self, u8 *headeredBytecode);

#endif
