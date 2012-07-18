 /*  Copyright (C) 2012 Xander Vedejas <xvedejas@gmail.com>
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
#include <parser.h>
#include <threading.h>

/* TODO:
 * 
 * think about how worlds relates to both variables and hidden internal data
 * bytecode extend command
 * 
 */

// call as readValue(process->bytecode, &process->IP);
// This function reads in a 32-bit value from bytecode
Size readValue(u8 *bytecode, Size *IP)
{	u8 _val = *bytecode;
	Size result = -1;
	if ((_val & 0xF0) == 0xF0)
	{
		*IP++;
		switch (_val & 0x0F)
		{
			case 0:
			    result = bytecode[*IP];
			    *IP++;
			break;
			case 1:
			    result = bytecode[*IP] | (bytecode[*IP + 1] << 8);
			    *IP += 2;
			break;
			case 2:
				result = bytecode[*IP] | (bytecode[*IP + 1] << 8)
					| (bytecode[*IP + 2] << 16) | (bytecode[*IP + 3] << 24);
			    *IP += 4;
			break;
			default:
			    panic("Not implemented");
			break;
		}
	}
	return result;
}

ObjectSet *globalObjectSet;

StringMap *globalSymbolTable;

Object *globalScope;

// argc doesn't include self
extern Object *callInternal(void *function, Size argc, va_list args);
Object *object_bind(Object *self, Object *symbol);
Object *methodTable_get(Object *self, Object *symbol);

/* Always use this method when creating new objects */
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
                char expectedArgument = argString[i + 1];
                void *argument = va_arg(argptr, void*);
                
                switch (expectedArgument)
                {
                    case 'u': // Expecting unsigned integer, if object, try to convert
                    {
                        if (isObject(argument))
                        {
							printf("%s\n", argString);
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
                return count - 1;
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

Object *closure_new(Object *self, Object *process)
{
	u8 *bytecode = ((Process*)process->data)->bytecode;
	Size *IP = &((Process*)process->data)->IP;
	Size varCount = readValue(bytecode, IP); // does not include arg count
	Size argCount = readValue(bytecode, IP);
	printf("var count %i, arg count %i\n", varCount, argCount);
	panic("not implemented");
	return NULL;
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

Object *scope_new(Object *self, Object *parent)
{
	panic("Not implemented");
	return NULL;
}

/* This does not create a new thread for the process. This only tries to make a
 * new process object corresponding to the current thread. */
Object *process_new(Object *self, Object *block)
{
	if (currentThread->process != NULL)
		return currentThread->process;
    Object *process = object_new(self);
    Process *data = malloc(sizeof(Process));
    process->data = data;
    
    data->global = scope_new(scopeProto, block);
    data->scope = scope_new(scopeProto, block);
    data->parent = NULL;
    data->values = stackNew();
    data->scopes = stackNew();
    currentThread->process = process;
    return process;
}

Object *process_spawn(Object *self, Object *block)
{
	panic("not implemented!");
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
    
    Object *consoleMT = object_send(methodTableMT, symbol("new:"), 1);
    Object *console = object_send(objectProto, newSymbol);
    console->methodTable = consoleMT;
    
    methodTable_addClosure(consoleMT, symbol("printTest"),
		closure_newInternal(closureProto, console_printTest, "vo"));
    
    send(console, "printTest");
    
    /* 5. Install other base components */
    
    Object *traitMT = methodTable_new(methodTableMT, 1);
    Object *traitProto = object_new(objectProto);
    traitProto->methodTable = traitMT;
    
    Object *scopeMT = methodTable_new(methodTableMT, 1);
    Object *scopeProto = object_new(objectProto);
    scopeProto->methodTable = scopeMT;
    
    methodTable_addClosure(traitMT, symbol("new"),
        closure_newInternal(closureProto, trait_new, "oo"));
    
    stringInstall();
}

/* This is looking up a variable, beginning with the local scope. The chain of
 * lookup is to try the current world in the current scope, then the current
 * world in the parent scope, then the parent world in the parent scope, etc.
 * 
 * The worst case runtime is highly dependent on the number of scopes we have
 * to look through. That should be more reason to prefer more-local variables
 * to more-global ones. */
 /// Todo: stop the lookup if the scope reaches the global scope. Return NULL.
Object *scope_lookupVar(Object *self, Object *symbol)
{
	Scope *scope = self->data;
    Object *world = scope->thisWorld;
    Object *thisWorld = world;
    Object *value;
    do
    {
        value = varListLookup(scope->variables, symbol, world);
        if (world == scope->thisWorld)
        {
            if (scope == (Scope*)globalScope->data)
                panic("lookup Error: variable not found!");
            assert(scope->containing != NULL, "lookup error");
            scope = scope->containing->data;
            world = thisWorld;
        }
        else
        {
            world = ((World*)world->data)->parent;
            if (unlikely(world == NULL))
                return NULL;
        }
    } while (value == NULL);
    return value;
}

/* This should mimic the previous function in the way it finds a variable.
 * All variables must be declared at the top of their scope, so it does not
 * ever create a variable if it doesn't find one */
void scope_setVar(Object *self, Object *symbol, Object *value)
{
    panic("not implemented");
}

/* This function is called by interpret at the beginning of executing a
 * closure, to account for the header which tells info about the variables and
 * arguments available to the closure */
void closureSetup(Object *process)
{
	panic("not implemented");
}

/* Sets up intern table from the bytecode header */
void bytecodeSetup(Object *process)
{
	u8 *bytecode = ((Process*)process->data)->bytecode;
	/* The first byte(s) are the number of interned symbols present in the
	 * bytecode segment */
	Size symbolCount = readValue(bytecode, &((Process*)process->data)->IP);
}

/* We are entering a new scope. The new scope's caller is the last scope.
 * Typically operates on currentThread->process */
void process_pushScope(Object *self, Object *scopeObj)
{
    Scope *scope = scopeObj->data;
    Process *process = self->data;
	scope->caller = process->scope;
    stackPush(process->scopes, scope);
}

/* We are exiting the current scope, back to the caller scope.
 * Typically operates on currentThread->process */
Object *process_popScope(Object *self)
{
    Process *process = self->data;
    return stackPop(process->scopes);
    /* It is important to note that we do not free the scope we are leaving
     * here. In fact, some of its local variables could still be referenced
     * by closures defined in that scope and possibly returned by that scope.
     * This is part of the reason we rely on garbage collection to destroy
     * the scope later on */
}

/* Typically operates on currentThread->process */
void process_pushValue(Object *self)
{
	panic("not implemented");
}

/* Typically operates on currentThread->process */
Object *process_popValue(Object *self)
{
	panic("not implemented");
    return NULL;
}

#define currentScope process->scope
#define currentBlock process->scope->block
#define stack process->stack

void interpret(u8 *bytecode)
{
	if (currentThread->process == NULL)
		currentThread->process = process_new(objectProto, closureProto);
	Object *process = currentThread->process;
	
	/* first, the byte code has a section of interned variable names which we
	 * must turn into a table */
	bytecodeSetup(process);
	closureSetup(process);
	Object *closure = closure_new(objectProto, process);
	// If there is no current process, we create one.
	
	while (true)
	{
		printf("%x\n", *bytecode);
		switch (*bytecode)
		{
			case integerBC:
				panic("not implemented");
            break;
			case doubleBC:
				panic("not implemented");
            break;
			case stringBC:
				panic("not implemented");
            break;
			case charBC:
				panic("not implemented");
            break;
			case symbolBC:
				panic("not implemented");
            break;
			case arrayBC:
				panic("not implemented");
            break;
			case blockBC:
				/*Object *closure = */closure_new(closureProto, process);
				// increment bytecode until the end of this closure definition
				panic("not implemented");
            break;
			case variableBC:
				panic("not implemented");
            break;
			case messageBC:
				panic("not implemented");
            break;
			case stopBC:
				panic("not implemented");
            break;
			case setBC:
				panic("not implemented");
            break;
			case endBC:
				panic("not implemented");
            break;
			case objectBC:
				panic("not implemented");
            break;
			case traitBC:
				panic("not implemented");
            break;
			case extendedBC8:
				panic("not implemented");
            break;
			case extendedBC16:
				panic("not implemented");
            break;
			case extendedBC32:
				panic("not implemented");
            break;
			case extendedBC64:
				panic("not implemented");
            break;
			case extendedBC128:
				panic("not implemented");
            break;
			default:
				panic("not implemented");
			break;
		}
		bytecode++;
	}
	panic("not implemented");
}
