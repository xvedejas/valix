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
 
#include <vm.h>
#include <mm.h>
#include <MethodTable.h>
#include <ObjectSet.h>
#include <StringMap.h>
#include <data.h>
#include <stdarg.h>

/* TODO:
 * 
 * add cache to bind()
 * think about how worlds relates to both variables and hidden internal data
 * 
 */

Object *objectProto, *symbolProto, *methodTableProto, *varTableProto,
    *closureProto;

ObjectSet *globalObjectSet;

Object *lookupSymbol;

StringMap *globalSymbolTable;

// argc doesn't include self
extern Object *callInternal(void *function, Size argc, Object *self, va_list args);
Object *object_bind(Object *self, Object *symbol);
Object *methodTable_lookup(Object *self, Object *symbol);

Object *object_new(Object *self)
{
    Object *new = malloc(sizeof(Object));
    new->parent = self;
    new->methodTable = (self == NULL)?NULL:self->methodTable;
    new->data = NULL;
    objectSetAdd(globalObjectSet, new);
    return new;
}

bool isObject(void *ptr)
{
    return objectSetHas(globalObjectSet, ptr);
}

Object *methodTable_new(Object *self, u32 size)
{
    Object *table = object_new(self);
    table->parent = self;
    table->methodTable = (self == NULL)?NULL:methodTableProto;
    table->data = methodTableDataNew(size);
    return table;
}

bool object_isSymbol(Object *self)
{
    return (stringMapGet(globalSymbolTable, self->data) == self);
}

Object *__attribute__ ((pure)) symbol_new(Object *self, String string)
{
    Object *symbol;
    if ((symbol = stringMapGet(globalSymbolTable, string)) != NULL)
        return symbol;
    symbol = object_new(self);
    symbol->data = string;
    stringMapSet(globalSymbolTable, string, symbol);
    assert(object_isSymbol(symbol), "VM error, symbol not a symbol");
    return symbol;
}

void *object_send(Object *self, Object *message, ...)
{
    if (!object_isSymbol(message))
        panic("sending something not a symbol");
    Object *method = object_bind(self, message);
    if (method == NULL)
        return object_send(self, symbol_new(symbolProto, "doesNotUnderstand:"),
            message);
    /* Call the method */
    Closure *closureData = method->data;
    va_list argptr;
    va_start(argptr, message);
    switch (closureData->type)
    {
        case internalClosure:
        {
            /* type checking and implicit conversion */
            String argString = closureData->argString;
            /* The first argument must be an object "self" */
            assert(isObject(self) && argString[1] == 'o', "vm error");
            Size i;
            for (i = 0; i < closureData->argc; i++)
            {
                char expectedArgument = argString[i + 2];
                void *argument = va_arg(argptr, void*);
                
                switch (expectedArgument)
                {
                    case 'u':
                    {
                        if (isObject(argument))
                        {
                            *((void**)argptr) = object_send(argument,
                                symbol_new(symbolProto, "asU32Int"));
                        }
                    } break;
                    case 's':
                    {
                        if (isObject(argument))
                        {
                            *((void**)argptr) = object_send(argument,
                                symbol_new(symbolProto, "asS32Int"));
                        }
                    } break;
                    case 'o':
                    {
                        if (!isObject(argument))
                            panic("VM Error, expected an object, given some "
                                  "other value. Please construct and pass a "
                                  "valid object. Message sent was %s. Arg %x",
                                  message->data, argument);
                    } break;
                    case 'S':
                    {
                        if (isObject(argument))
                        {
                            *((void**)argptr) = object_send(argument,
                                symbol_new(symbolProto, "asCString"));
                        }
                    } break;
                    default:
                        panic("vm error");
                }
            }
            
            va_list arguments;
            va_start(arguments, message);
            return callInternal(closureData->function, closureData->argc, self,
                arguments);
        } break;
        case userDefinedClosure:
        {
            panic("not implemented\n");
        } break;
        default: panic("vm error, closure type %i not known", closureData->type);
    }
    return NULL;
}

Object *__attribute__ ((pure)) object_bind(Object *self, Object *symbol)
{
    Object *methodTable = self->methodTable;
    return (symbol == lookupSymbol && self == self->methodTable)?
        methodTable_lookup(methodTable, symbol):
        object_send(methodTable, lookupSymbol, symbol);
}

Size getArgc(String argString)
{
    Size count = 0;
    while (true)
    {
        switch (*argString++)
        {
            case 's': case 'u': // signedint, unsigned (word size)
            case 'o': case 'S': case 'v': // object*, String, void
                count++;
            break;
            case '\0':
            /* ignore first argument, which is return value, and the second
             * argument (self) isn't counted */
                return count - 2;
            default:
                panic("Improperly formatted argstring, character %c",
                    argString[-1]);
        }
    }
    return count - 2;
}

/* The given argument string tells what sort of arguments are expected by the
 * given function.
 * 
 * Example: "SSou32s8" means (String,Object*,u32,s8), return value is a String
 */
Object *closure_newInternal(Object *self, void *function, String argString)
{
    Size argc = getArgc(argString);
    Object *new = object_new(self);
    Closure *closure = malloc(sizeof(Closure));
    new->data = closure;
    closure->argString = argString;
    closure->argc = argc;
    closure->function = function;
    closure->type = internalClosure;
    return new;
}

Object *methodTable_lookup(Object *self, Object *symbol)
{
    MethodTable *table = self->data;
    return methodTableDataGet(table, symbol);
}

void methodTable_addClosure(Object *self, Object *symbol, Object *closure)
{
    MethodTable *table = self->data;
    methodTableDataAdd(table, symbol, closure);
}

void console_printTest(Object *self)
{
    printf("Success! object at %x\n", self);
}

void vmInstall()
{
    globalSymbolTable = stringMapNew();
    globalObjectSet = objectSetNew();
    
    /* 1. create and initialize methodTables */
    
    objectProto = object_new(NULL);
    
    methodTableProto = methodTable_new(NULL, 4);
    methodTableProto->methodTable = methodTableProto;
    
    Object *objectMethodTable = methodTable_new(methodTableProto, 1);
    methodTableProto->parent = objectProto;
    objectProto->methodTable = objectMethodTable;
    
    Object *symbolMethodTable = methodTable_new(objectMethodTable, 1);
    symbolProto = object_new(objectProto);
    symbolProto->methodTable = symbolMethodTable;
    
    Object *closureMethodTable = methodTable_new(objectMethodTable, 0);
    closureProto = object_new(objectProto);
    closureProto->methodTable = closureMethodTable;
    
    /* 2. install the following methods:
     * methodTable_lookup()
     * methodTable_add()
     * object_new()
     * methodTable_new()
     * symbol_new()
     * object_setMethodTable() */
    
    lookupSymbol = symbol_new(symbolProto, "lookup:");
    methodTable_addClosure(methodTableProto,
        lookupSymbol,
        closure_newInternal(closureProto, methodTable_lookup, "ooo"));
    methodTable_addClosure(objectMethodTable,
        symbol_new(symbolProto, "new"),
        closure_newInternal(closureProto, object_new, "oo"));
    methodTable_addClosure(methodTableProto,
        symbol_new(symbolProto, "new:"),
        closure_newInternal(closureProto, methodTable_new, "oou"));
    methodTable_addClosure(symbolMethodTable,
        symbol_new(symbolProto, "new:"),
        closure_newInternal(closureProto, symbol_new, "ooS"));
    
    /* 3. Do asserts to make sure things all connected correctly */
    
    assert(objectProto->parent == NULL, "vm error");
    assert(objectProto->methodTable == objectMethodTable, "vm error");
    assert(objectMethodTable->parent == methodTableProto, "vm error");
    assert(objectMethodTable->methodTable == methodTableProto, "vm error");
    assert(methodTableProto->parent == objectProto, "vm error");
    assert(methodTableProto->methodTable == methodTableProto, "vm error");
    assert(symbolProto->parent == objectProto, "vm error");
    assert(symbolProto->methodTable == symbolMethodTable, "vm error");
    assert(symbolMethodTable->parent == objectMethodTable, "vm error");
    assert(symbolMethodTable->methodTable == methodTableProto, "vm error");
    
    /* 4. Test the system by creating a Console object from ObjectProto,
     *    adding a printTest method, and calling it using the proper mechanisms */
    
    Object *console = object_send(objectProto, symbol_new(symbolProto, "new"));
    Object *consoleMethodTable = object_send(objectMethodTable, symbol_new(symbolProto, "new:"), 1);
    console->methodTable = consoleMethodTable;
    
    Object *methodClosure = closure_newInternal(closureProto, console_printTest, "vo");
    methodTable_addClosure(consoleMethodTable,
        symbol_new(symbolProto, "printTest"),
        methodClosure);
    
    object_send(console, symbol_new(symbolProto, "printTest"));
}
