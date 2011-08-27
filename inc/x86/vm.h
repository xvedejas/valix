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

typedef enum
{
    tobefilledout
} PermissionLevel;

typedef struct byteArray
{
    Size len;
    u8 array[0];
} ByteArray;

typedef struct scope
{
    struct object *parent; // for stack purposes only
    struct object *block; // the corresponding closure
    struct object *containing; // parent scope is where the closure was defined
    Stack *valueStack;
    bool fromInternal;
    u8 *IP;
    struct object *thisWorld;
    SymbolMap *variables; // SymbolMap<variable, Map<world, value>>
    jmp_buf errorCatch; // active if errorCatch[0] not null
} Scope;

#define arg(n) ((Object*)stackGet(currentScope->scope->valueStack, n))
#define pop() ({ ((Object*)stackPop(currentScope->scope->valueStack)); })
#define popFrom(_scope) ({ ((Object*)stackPop(_scope->scope->valueStack)); })
#define args(n) ((Object**)stackArgs(currentScope->scope->valueStack, n))
#define push(v) ({ (stackPush(currentScope->scope->valueStack, v)); })
#define pushTo(v, _scope) ({ (stackPush(_scope->scope->valueStack, v)); })
#define currentScope (process->process->scope)
#define currentIP (currentScope->scope->IP)
#define currentClosure (currentScope->scope->block)
#define currentWorld (currentScope->scope->thisWorld)

typedef void (InternalFunction)(Object*);

typedef struct closure
{
    closureType type;
    Size argc;
    struct object *scope; // the scope where this closure was defined
    bool isMethod;
    union
    {
        struct // Internal function
        {
            PermissionLevel permissions;
            InternalFunction *function;
        };
        struct // Defined function
        {
            u8 *bytecode;
            Map *translationTable; // int -> symbol
            Size variables;
        };
    };
} Closure;

typedef struct process
{
    Object *scope;
    Object *world;
    PermissionLevel permissions;
    Size depth;
    bool inInternal;
    Size line; // try to keep track of line number in source
    jmp_buf exit;
} Process;

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

typedef enum
{
    integer, /* 0 to 2^32-1  */
    bigint,  /* -inf to +inf */
    floating, /* float precision */
    fraction, /* perfect precision, bigint / bigint */
} NumberType;

typedef struct number
{
    NumberType type;
    u32 data[1];
} Number;

typedef struct world
{
    Object *parent;
    Map *reads; /* map<map<world, var>, initial_value> */
    List *modifies; /* list<map<world, var>> */
} World;

typedef struct throwable
{
    String name;
    Object *message;
} Throwable;

struct object
{
    struct object *proto;
    SymbolMap *methods;
    union
    {
        struct closure closure[0];
        struct scope   scope[0];
        struct process process[0];
        struct stringData string[0];
        struct array array[0];
        struct number number[0];
        struct world world[0];
        struct throwable throwable[0];
        u8 byte[0];
        Size value[0];
        void *data[0];
        struct object **object;
    };
};

Object *objectProto, *symbolProto, *closureProto, *scopeProto, *processProto,
       *byteArrayProto, *stringProto, *arrayProto, *integerProto,
       *globalWorld, *charProto;

Object *trueObject, *falseObject, *console;

Object *throwable, *exceptionProto, *errorProto, *divideByZeroException,
    *notImplementedException, *doesNotUnderstandException, *vmError;

extern Object *call(Object *process, Object *object, String message, ...);

extern Object *processNew();
extern void processSetBytecode(Object *process, u8 *bytecode);
extern void processMainLoop(Object *process);
extern void vApply(Object *process);
extern void vmInstall();
extern Object *integerNew(u32 value);
extern Object *stringNew(Size len, String s);
extern Object *stringNewNT(String s);
extern Object *symbolNew(String string);
extern void yourself(Object *process);
extern inline Object *bind(Object *target, Object *symbol);
extern Object *objectNew();
extern void setInternalMethods(Object *object, Size entries, void **entry);
extern Object *charNew(u8 value);

typedef void *methodList[];

#endif
