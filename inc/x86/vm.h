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
    struct object *block; // the corresponding closure
    union
    {
        struct // if user-defined closure
        {
            struct object *parent; // parent scope is where the closure was defined
            SymbolMap *variables;
            u8 *IP;
        };
        struct // if internal function
        {
            jmp_buf env;
        };
    };
} Scope;

#define arg(n) ((Object*)stackGet(process->process->valueStack, n))
#define pop() ((Object*)stackPop(process->process->valueStack))
#define args(n) ((Object**)stackArgs(process->process->valueStack, n))
#define push(v) (stackPush(process->process->valueStack, v))
#define currentScope (process->process->scope)
#define currentIP (currentScope->scope->IP)
#define currentClosure (currentScope->scope->block)

typedef void (InternalFunction)(Object*);

typedef struct closure
{
    closureType type;
    Size argc;
    struct object *containingClosure;
    union
    {
        struct
        {
            PermissionLevel permissions;
            InternalFunction *function;
        };
        struct
        {
            u8 *bytecode;
            Map *translationTable; // int -> symbol
            Size variables;
        };
    };
} Closure;

typedef struct process
{
    u8 *bytecode;
    Object *scope;
    Stack *valueStack;
    PermissionLevel permissions;
    jmp_buf mainLoop;
} Process;

typedef struct stringData
{
    Size len;
    char string[0];
} StringData;

struct object
{
    struct object *proto;
    SymbolMap *methods;
    union
    {
        SymbolMap members[0];
        struct closure closure[0];
        struct scope   scope[0];
        struct process process[0];
        struct stringData string[0];
        u8 byte[0];
        void *data[0];
    };
};


extern Object *processNew();
extern void processSetBytecode(Object *process, u8 *bytecode);
extern void processMainLoop(Object *process);
extern void vmInstall();

#endif
