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
#include <VarList.h>
#include <Number.h>
#include <String.h>

/* TODO:
 * 
 * think about how worlds relates to both variables and hidden internal data
 * bytecode extend command
 * 
 */

ObjectSet *globalObjectSet;

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
    table->methodTable = (self == NULL)?NULL:methodTableMT;
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
    Object *method = object_bind(self, message);
    if (method == NULL)
        return object_send(self, symbol("doesNotUnderstand:"), message);
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
                                symbol("asU32Int"));
                        }
                    } break;
                    case 's':
                    {
                        if (isObject(argument))
                        {
                            *((void**)argptr) = object_send(argument,
                                symbol("asS32Int"));
                        }
                    } break;
                    case 'o':
                    {
                        if (!isObject(argument))
                            panic("VM Error, expected an object, given some "
                                  "other value. Please construct and pass a "
                                  "valid object. Message sent was %s. Arg %x.",
                                  message->data, argument);
                    } break;
                    case 'S':
                    {
                        if (isObject(argument))
                        {
                            *((void**)argptr) = object_send(argument,
                                symbol("asCString"));
                        }
                    } break;
                    default:
                        panic("vm error");
                }
            }
            
            va_list arguments;
            va_start(arguments, message);
            void *result = callInternal(closureData->function, closureData->argc, self,
                arguments);
            switch (argString[0])
            {
                case 'u':
                case 's': return integer32_new(integer32Proto, (s32)result);
                case 'S': return string_new(stringProto, (String)result);
                case 'v': return NULL;
                case 'o':
                default: return result;
            }
        } break;
        case userDefinedClosure:
        {
            panic("not implemented\n");
        } break;
        default: panic("vm error, closure type %i not known", closureData->type);
    }
    return NULL;
}

struct methodCacheEntry
{
    Object *methodTable, *symbol, *method;
} MethodCache[8192];

Object *__attribute__ ((pure)) object_bind(Object *self, Object *symbol)
{
    if (!object_isSymbol(symbol))
        panic("sending something not a symbol");
    if (unlikely(self == NULL))
        panic("Binding symbol %s to null value not implemented", symbol->data);
    
    Object *methodTable = self->methodTable;
    
    /* Check the cache */
    Size methodCacheHash = (((Size)methodTable << 2) ^ ((Size)symbol >> 3)) &
        (sizeof(MethodCache) / sizeof(struct methodCacheEntry) - 1);
    struct methodCacheEntry *entry = MethodCache + methodCacheHash;
    if (entry->methodTable == methodTable && entry->symbol == symbol)
        return entry->method;
    
    Object *method = (symbol == lookupSymbol && self == methodTable)?
        methodTable_lookup(self, symbol):
        object_send(methodTable, lookupSymbol, symbol);
    entry->methodTable = methodTable;
    entry->symbol = symbol;
    entry->method = method;
    return method;
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

Object *returnTrue(Object *self)
{
    return trueObject;
}

Object *returnFalse(Object *self)
{
    return falseObject;
}

void object_doesNotUnderstand(Object *self, Object *symbol)
{
    printf("Object at %x does not understand symbol %s\n", self, symbol);
    panic("Error handling not currently implemented");
}

void vmInstall()
{
    globalSymbolTable = stringMapNew();
    globalObjectSet = objectSetNew();
    
    /* 1. create and initialize methodTables */
    
    objectProto = object_new(NULL);
    
    methodTableMT = methodTable_new(NULL, 4);
    methodTableMT->methodTable = methodTableMT;
    
    objectMT = methodTable_new(methodTableMT, 2);
    methodTableMT->parent = objectProto;
    objectProto->methodTable = objectMT;
    
    Object *symbolMT = methodTable_new(objectMT, 1);
    symbolProto = object_new(objectProto);
    symbolProto->methodTable = symbolMT;
    
    Object *closureMT = methodTable_new(objectMT, 0);
    closureProto = object_new(objectProto);
    closureProto->methodTable = closureMT;
    
    /* todo: trait object, its data is a MethodTable. */
    
    /* 2. install the following methods:
     * methodTable_lookup()
     * methodTable_add()
     * object_new()
     * methodTable_new()
     * symbol_new()
     * object_setMT() */
    
    lookupSymbol = symbol("lookup:");
    methodTable_addClosure(methodTableMT,
        lookupSymbol,
        closure_newInternal(closureProto, methodTable_lookup, "ooo"));
    methodTable_addClosure(objectMT,
        symbol("new"),
        closure_newInternal(closureProto, object_new, "oo"));
    methodTable_addClosure(objectMT,
        symbol("doesNotUnderstand:"),
        closure_newInternal(closureProto, object_doesNotUnderstand, "voo"));
    methodTable_addClosure(methodTableMT,
        symbol("new:"),
        closure_newInternal(closureProto, methodTable_new, "oou"));
    methodTable_addClosure(symbolMT,
        symbol("new:"),
        closure_newInternal(closureProto, symbol_new, "ooS"));
    
    /* 3. Do asserts to make sure things all connected correctly */
    
    assert(objectProto->parent == NULL, "vm error");
    assert(objectProto->methodTable == objectMT, "vm error");
    assert(objectMT->parent == methodTableMT, "vm error");
    assert(objectMT->methodTable == methodTableMT, "vm error");
    assert(methodTableMT->parent == objectProto, "vm error");
    assert(methodTableMT->methodTable == methodTableMT, "vm error");
    assert(symbolProto->parent == objectProto, "vm error");
    assert(symbolProto->methodTable == symbolMT, "vm error");
    assert(symbolMT->parent == objectMT, "vm error");
    assert(symbolMT->methodTable == methodTableMT, "vm error");
    
    /* 4. Test the system by creating a Console object from ObjectProto,
     *    adding a printTest method, and calling it using the proper mechanisms */
    
    Object *console = object_send(objectProto, symbol("new"));
    Object *consoleMT = object_send(objectMT, symbol("new:"), 1);
    console->methodTable = consoleMT;
    
    Object *methodClosure =
        closure_newInternal(closureProto, console_printTest, "vo");
    methodTable_addClosure(consoleMT,
        symbol("printTest"),
        methodClosure);
    
    object_send(console, symbol("printTest"));
    
    /* 5. Install other base components */
    
    numberInstall();
    stringInstall();
}
