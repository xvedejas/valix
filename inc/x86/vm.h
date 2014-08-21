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
 *      Xander Vedėjas <xvedejas@gmail.com>
 */
#ifndef __vm_h__
#define __vm_h__
#include <main.h>
#include <threading.h>
#include <data.h>
#include <setjmp.h>
#include <MethodTable.h>
#include <VarList.h>

typedef struct methodTable MethodTable;
typedef struct object Object;
typedef struct varList VarList;

typedef enum
{
    internalClosure,
    userDefinedClosure
} closureType;

typedef struct closure
{
    closureType type;
    Size argc;
    union
    {
        struct // internalClosure
        {
            void *function;
        };
        struct // userDefinedClosure
        {
            u8 *bytecode;
            /* The "parent" is the _scope_ in which this closure was defined */ 
            Object *parent;
            Object *world;
        };
    };
} Closure;

typedef struct
{
    Size len;
    Object *objects[0];
} ArrayData;

typedef struct stringData
{
    Size len;
    char string[0];
} StringData;

/* A trait is an array of closures that represent methods to be
 * added to method tables on their creation. */
typedef struct trait
{
    Size methodCount;
    Object **symbols;
    Object **closures;
} Trait;

typedef struct scope Scope;
typedef struct process Process;
typedef struct world World;

struct object
{
    struct object *parent;
    struct object *methodTable;
    union
    {
        void *data;
        // if the object is a MethodTable, it has this second methodTable:
        MethodTable *table;
        Closure *closure;
        StringData *string;
        ArrayData *array;
        Trait *trait;
        Scope *scope; // user-defined objects have a scope
        Process *process;
        World *world;
        char *symbol;
    };
};

/* A scope is, in a sense, a running "instance" of a block/"closure". 
 * It contains the values of local variables and knows about parent and calling
 * scopes. */
typedef struct scope
{
	VarList *variables; // Map<variable symbol, list of (world, value) pairs>
    /* Each scope has a _single_ world. That is, when a new world is created, it
     * requires a new scope. By default, scopes defined within a world also
     * exist in that world. The real effect is on the state of variables seen in
     * the parent scope, not in the current one. */
	Object *world;
    // parent scope (where this was declared) that we can look for variables in
	Object *containing;
	Object *caller; // scope that this scope was called from
	/* The following data is for storage when executing a child scope */
	Object *closure;
	u8 *bytecode; // beginning of bytecode
    Size IP; // index of bytecode
} Scope;

typedef struct process
{
	Object *parent; // parent process
    Stack values; // stack for saving values during statement execution
    Stack scopes; // the "current scope" is the top of the stack
    Object **symbols; // array of symbols (for de-interning)
    u8 *bytecode; // beginning of bytecode
    Size IP; // index of bytecode
} Process;

#define symbol(str) (symbol_new(symbolProto, str))

Object *objectProto, *objectMT, *symbolProto, *methodTableMT, *varTableProto,
    *closureProto, *scopeProto, *bindSymbol, *getSymbol, *trueObject,
    *falseObject, *newSymbol, *DNUSymbol, *worldProto, *console;

extern Object *symbol_new(Object *self, String string);
extern Object *newDisallowed(Object *self);
extern Object *object_new(Object *self);
extern Object *object_bind(Object *self, Object *symbol);
extern void vmInstall();
extern void methodTable_addClosure(Object *self, Object *symbol, Object *closure);
extern Object *closure_newInternal(Object *self, void *function, Size argc);
extern Object *returnTrue(Object *self);
extern Object *returnFalse(Object *self);
extern Object *closure_with(Object *self, ...);
extern Object *methodTable_new(Object *self, u32 size);
extern Object *currentProcess();
extern Object *interpret();
extern void interpretBytecode(u8 *bytecode);

#define object_send(self, message, ...)\
({\
    Object *method = NULL;\
	method = object_bind(self, message);\
	bool doesNotUnderstand = false;\
	if (method == NULL)\
	{\
		doesNotUnderstand = true;\
		method = object_bind(self, symbol("doesNotUnderstand:"));\
		assert(method != NULL, "does not understand does not understand...\n");\
		;\
	}\
	(doesNotUnderstand)?closure_with(method, self, message):\
	closure_with(method, self, ## __VA_ARGS__);\
})

#define send(obj, messagestr, ...)\
    object_send(obj, symbol(messagestr), ## __VA_ARGS__)

#define as(obj, T) ((T*)(obj)->data)

#endif
