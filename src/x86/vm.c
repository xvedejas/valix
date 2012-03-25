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
extern Object *callInternal(void *function, Size argc, va_list args);
Object *object_bind(Object *self, Object *symbol);
Object *methodTable_get(Object *self, Object *symbol);

Object *object_new(Object *self)
{
    Object *new = malloc(sizeof(Object));
    new->parent = self;
    new->methodTable = (self == NULL)?NULL:self->methodTable;
    new->data = NULL;
    objectSetAdd(globalObjectSet, new);
    return new;
}

inline bool isObject(void *ptr)
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
    /* Symbol objects are unique, so given a string that already identifies an
     * existing symbol object, a new symbol is not created; instead the old one
     * is returned. */
    Object *symbol;
    if ((symbol = stringMapGet(globalSymbolTable, string)) != NULL)
        return symbol;
    symbol = object_new(self);
    symbol->data = string;
    stringMapSet(globalSymbolTable, string, symbol);
    assert(object_isSymbol(symbol), "VM error, symbol not a symbol");
    return symbol;
}


Object *closure_with(Object *self, ...)
/*
Executes a closure (self) with the given arguments
*/
{
	assert(self != NULL && self->data != NULL, "NULL error");
    Closure *closureData = self->data;
    va_list argptr;
    va_start(argptr, self);
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
                    case 'u': // Expecting unsigned integer, if object, convert
                    {
                        if (isObject(argument))
                        {
							printf("%x is an object?\n", argument);
                            *((void**)argptr) = send(argument, "asU32Int");
                        }
                    } break;
                    case 's': // Expecting signed integer, if object, convert
                    {
                        if (isObject(argument))
                        {
                            *((void**)argptr) = object_send(argument,
                                symbol("asS32Int"));
                        }
                    } break;
                    case 'o': // Expecting object
                    {
                        if (!isObject(argument))
                            panic("VM Error, expected an object, given some "
                                  "other value. Please construct and pass a "
                                  "valid object. Arg %x.", argument);
                    } break;
                    case 'S': // Expecting string, if object, convert
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
            va_start(arguments, self);
            /* We need to know the number of arguments to pass, so we must know
             * what a method expects. To pass variable arguments, we will need
             * to use a simple list object. */
            
            indention++;///
            void *result = callInternal(closureData->function,
                closureData->argc + 1, arguments);
            /* The given type of "result" is what we currently have, but we want
             * to return an object, so here we do the proper conversion. */
            indention--;///
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
        /* If the closure is of type userDefinedClosure, we must execute
         * bytecode instead of calling a function. Bytecode functions always
         * return objects and expect objects as arguments. */
        case userDefinedClosure:
        {
            panic("not implemented\n");
            char *bytecode = closureData->bytecode;
            
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
    /* This function gives us a method closure from a generic object and a
     * symbol object. The result is guaranteed to be the same given the same
     * input. Special care should be taken to remove any record of self's
     * method table in the method cache if self is garbage collected. */
    if (!object_isSymbol(symbol))
        panic("sending something not a symbol");
    if (unlikely(self == NULL))
        panic("Binding symbol %s to null value not implemented", symbol->data);
    
    Object *methodTable = self->methodTable;
    /*
    // Check the cache
    Size methodCacheHash = (((Size)methodTable << 2) ^ ((Size)symbol >> 3)) &
        (sizeof(MethodCache) / sizeof(struct methodCacheEntry) - 1);
    struct methodCacheEntry *entry = MethodCache + methodCacheHash;
    if (entry->methodTable == methodTable && entry->symbol == symbol)
        return entry->method;
    */
    
    Object *method;
    if ((symbol == getSymbol || symbol == bindSymbol) && self == self->methodTable)
    {
        method = methodTableDataGet(self->data, symbol);
    }
    else
    {
		Object *table = self->methodTable;
		method = send(table, "get:", symbol);
		if (method == NULL && self->parent != NULL)
			method = send(self->parent, "bind:", symbol);
    }
    /*
		entry->methodTable = methodTable;
		entry->symbol = symbol;
		entry->method = method;
	*/
    return method;
}

Size __attribute__ ((pure)) getArgc(String argString)
{
    /* Here we determine the number of arguments expected by a closure based on
     * the string that tells us the types of its arguments, starting with the
     * type the closure returns after invokation. We do not count the return
     * value or "self". */
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

Object *methodTable_get(Object *self, Object *symbol)
{
	return methodTableDataGet(self->data, symbol);
}

void methodTable_addClosure(Object *self, Object *symbol, Object *closure)
{
    MethodTable *table = self->data;
    methodTableDataAdd(table, symbol, closure);
}

void console_printTest(Object *self)
{
    printf("    VM CHECK: Success! console object at %x\n", self);
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
    /* Overrideable method called whenever an object does not understand some
     * symbol */
    printf("Object at %x does not understand symbol '%s'\n", self, symbol->data);
    panic("Error handling not currently implemented");
}

Object *object_methodTable(Object *self)
{
    return self->methodTable;
}

Object *trait_new(Object *self)
{
    panic("Not implemented");
    return NULL;
}

void vmInstall()
{
    globalSymbolTable = stringMapNew();
    globalObjectSet = objectSetNew();
    
    /* 1. create and initialize methodTables */
    
    objectProto = object_new(NULL);
    
    methodTableMT = methodTable_new(NULL, 4);
    methodTableMT->methodTable = methodTableMT;
    
    objectMT = methodTable_new(methodTableMT, 4);
    methodTableMT->parent = objectProto;
    objectProto->methodTable = objectMT;
    
    Object *symbolMT = methodTable_new(methodTableMT, 1);
    symbolProto = object_new(objectProto);
    symbolProto->methodTable = symbolMT;
    
    Object *closureMT = methodTable_new(methodTableMT, 1);
    closureProto = object_new(objectProto);
    closureProto->methodTable = closureMT;
    
    /* 2. install the following methods: */
    printf("Installing methods\n");
    
    bindSymbol = symbol("bind:");
    getSymbol = symbol("get:");
    newSymbol = symbol("new");
    DNUSymbol = symbol("doesNotUnderstand:");
    // methodTable.get(self, symbol)
    methodTable_addClosure(methodTableMT, getSymbol,
        closure_newInternal(closureProto, methodTable_get, "ooo"));
    // object.new(self)
    methodTable_addClosure(objectMT, newSymbol,
        closure_newInternal(closureProto, object_new, "oo"));
    // object.bind(self, symbol)
    methodTable_addClosure(objectMT, bindSymbol,
		closure_newInternal(closureProto, object_bind, "ooo"));
    // object.doesNotUnderstand(self, message)
    methodTable_addClosure(objectMT, symbol("doesNotUnderstand:"),
		closure_newInternal(closureProto, object_doesNotUnderstand, "voo"));
    // object.methodTable(self)
    methodTable_addClosure(objectMT, symbol("methodTable"),
        closure_newInternal(closureProto, object_methodTable, "oo"));
    // methodTable.new(self, size)
    methodTable_addClosure(methodTableMT, symbol("new:"),
        closure_newInternal(closureProto, methodTable_new, "oou"));
    // symbol.new(self, string)
    methodTable_addClosure(symbolMT, symbol("new:"),
        closure_newInternal(closureProto, symbol_new, "ooS"));
    // closure.send(self, list)
    methodTable_addClosure(closureMT, symbol("with:"),
        closure_newInternal(closureProto, closure_with, "ooo"));
    
    /* 3. Do asserts to make sure things all connected correctly */
    printf("Asserting\n");
    
    assert(objectProto->parent == NULL, "vm error");
    assert(objectProto->methodTable == objectMT, "vm error");
    assert(objectMT->parent == methodTableMT, "vm error");
    assert(objectMT->methodTable == methodTableMT, "vm error");
    assert(methodTableMT->parent == objectProto, "vm error");
    assert(methodTableMT->methodTable == methodTableMT, "vm error");
    assert(symbolProto->parent == objectProto, "vm error");
    assert(symbolProto->methodTable == symbolMT, "vm error");
    assert(symbolMT->parent == methodTableMT, "vm error");
    assert(symbolMT->methodTable == methodTableMT, "vm error");
    assert(closureProto->parent == objectProto, "vm error");
    assert(closureProto->methodTable == closureMT, "vm error");
    assert(closureMT->parent == methodTableMT, "vm error");
    assert(closureMT->methodTable == methodTableMT, "vm error");
    
    /* 4. Test the system by creating a Console object from ObjectProto,
     *    adding a printTest method, and calling it using the proper mechanisms */
    printf("Testing\n");
    
    numberInstall();
    
    printf("AAA\n");
    
    Object *consoleMT = object_send(methodTableMT, symbol("new:"), 1);
    
    printf("BBB\n");
    
    Object *console = object_send(objectProto, newSymbol);
    console->methodTable = consoleMT;
    
    methodTable_addClosure(consoleMT, symbol("printTest"),
		closure_newInternal(closureProto, console_printTest, "vo"));
    
    send(console, "printTest");
    
    /* 5. Install other base components */
    
    Object *traitMT = methodTable_new(methodTableMT, 1);
    Object *traitProto = object_new(objectProto);
    traitProto->methodTable = traitMT;
    
    methodTable_addClosure(traitMT, symbol("new"),
        closure_newInternal(closureProto, trait_new, "oo"));
    
    stringInstall();
}
