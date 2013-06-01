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
#include <Array.h>
#include <cstring.h>
#include <String.h>
#include <parser.h>
#include <threading.h>
#include <types.h>

/* The code in this file is some of the most complex in this project. Any
 * changes to this code must be reviewed by Xander before the changes are
 * pushed. */

// call as readValue(process->bytecode, &process->IP);
// This function reads in a value from bytecode which may be encoded in
// 1, 2, or 4 bytes depending on format.
Size readValue(u8 *bytecode, Size *IP)
{
    u8 _val = bytecode[*IP];
	Size result = _val;
    *IP += 1;
	if ((_val & 0xF0) == 0xF0) // value >= 0xF0
	{
		switch (_val & 0x0F)
		{
			case 0: /* values from 0xF0 to 0xFF
                      (which normally indicate more bytes) */
			    result = bytecode[*IP];
			    *IP += 1;
			break;
			case 1: /* values from 0x100 to 0xFFFF */
			    result = bytecode[*IP] | (bytecode[*IP + 1] << 8);
			    *IP += 2;
			break;
			case 2: /* values from 0x10000 to 0xFFFFFFFF */
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

// call as readString(process->bytecode, &process->IP)
// This function reads a null-terminated byte string from bytecode. It does not
// create a copy, it only provides a pointer to the string in bytecode.
String readString(u8 *bytecode, Size *IP)
{
    String s = (String)(bytecode + *IP);
    *IP += strlen(s) + 1;
    return s;
}

ObjectSet *globalObjectSet;

StringMap *globalSymbolTable;

Object *globalScope;

// argc doesn't include self
extern Object *callInternal(void *function, Size argc, va_list args);
Object *object_bind(Object *self, Object *symbol);
Object *methodTable_get(Object *self, Object *symbol);

/* Always use this method when creating new objects. */
/// Todo: separate constructor and allocator
Object *object_new(Object *self)
{
    Object *new = malloc(sizeof(Object));
    new->parent = self;
    // By default, we point to the parent's method table
    new->methodTable = (self == NULL)?NULL:self->methodTable;
    new->data = NULL;
    objectSetAdd(globalObjectSet, new);
    return new;
}

Object *object_toString(Object *self)
{
	char strBuffer[30];
	sprintf(strBuffer, "<Object at %x>", self);
	return string_new(stringProto, strdup(strBuffer));
}

Object *methodTable_toString(Object *self)
{
	char strBuffer[30];
	sprintf(strBuffer, "<MethodTable at %x>", self);
	return string_new(stringProto, strdup(strBuffer));
}

Object *closure_toString(Object *self)
{
	char strBuffer[30];
	sprintf(strBuffer, "<Block at %x>", self);
	return string_new(stringProto, strdup(strBuffer));
}

Object *symbol_toString(Object *self)
{
	return string_new(stringProto, strdup(self->data));
}

Object *boolean_toString(Object *self)
{
	if (self == trueObject)
		return string_new(stringProto, strdup("true"));
	if (self == falseObject)
		return string_new(stringProto, strdup("false"));
	return string_new(stringProto, strdup("bool"));
	/// todo: delegate to super
	/*
	else
		return super(self, "toString");
		*/
}

/* There is a global set that tells us if a pointer points to an object in the
 * VM. This is mostly a debugging tool which prevents us from treating arbitrary
 * pointers as if they were objects. */
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

/* A symbol's data is a pointer to the string (not necessarily unique) that was
 * first used to identify the symbol. It may be necessary to copy the string as
 * to give the symbol object ownership of its data. */
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

Object *scope_new(Object *self, Object *process, Object *containing,
                  Object *caller, bool readBytecode);

Object *_closure_with(Object *self, va_list argptr)
{
	// If we are executing a block that expects some value that isn't an object,
	// it is not guaranteed to get that value; it may get an object and it is
	// the responsibility of that block to take care of the appropriate
	// conversion.
	
	// This function will not always return an object either, but expects that
	// if a function is called by the user which does not return an object,
	// an error will be raised elsewhere.
	
	/// todo: if the arguments given are fewer than the required, do currying
	Closure *closure = self->data;
	switch (closure->type)
	{
		case userDefinedClosure:
			return interpret(self);
		break;
		case internalClosure:
		{
			return callInternal(closure->function, closure->argc, argptr);
		}
	}
	return NULL;
}

Object *closure_with(Object *self, ...)
{
	va_list argptr;
	va_start(argptr, self);
	return _closure_with(self, argptr);
}

Object *closure_withArray(Object *self, Object **args)
{
	return _closure_with(self, (va_list)args);
}

// This cache is to speed up lookups in object_bind
struct methodCacheEntry
{
    Object *methodTable, *symbol, *method;
} MethodCache[8192];

Object *__attribute__ ((pure)) object_bind(Object *self, Object *symbol)
{
    /* This function gives us a method closure from a generic object and a
     * symbol object. The result is guaranteed to be the same given the same
     * input, since an object does not gain or lose methods after definition.
     * Special care should be taken to remove any record of self's
     * method table in the method cache if self is garbage collected. */
    if (!object_isSymbol(symbol))
        panic("sending something not a symbol");
    if (unlikely(self == NULL))
        panic("Binding symbol '%s' to null value not implemented "
              "(can't send message to null!)", symbol->data);
    
    Object *methodTable = self->methodTable;
    // Check the cache
    Size methodCacheHash = (((Size)methodTable << 2) ^ ((Size)symbol >> 3)) &
        (sizeof(MethodCache) / sizeof(struct methodCacheEntry) - 1);
    struct methodCacheEntry *entry = MethodCache + methodCacheHash;
    if (entry->methodTable == methodTable && entry->symbol == symbol)
        return entry->method;
    
    Object *method = NULL;
    if ((symbol == getSymbol || symbol == bindSymbol) &&
        self == self->methodTable)
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
	entry->methodTable = methodTable;
	entry->symbol = symbol;
	entry->method = method;
    return method;
}

/* The given argument string tells what sort of arguments are expected by the
 * given function.
 * 
 * Example: "SSous" means (String,Object*,u32,s32), return value is a String
 */
Object *closure_newInternal(Object *self, void *function, Size argc)
{
    Object *new = object_new(self);
    Closure *closure = malloc(sizeof(Closure));
    new->data = closure;
    closure->argc = argc;
    closure->function = function;
    closure->type = internalClosure;
    return new;
}

/* Raises an error */
Object *newDisallowed(Object *self)
{
	panic("Cannot use this object as a prototype");
	return NULL;
}

/* An "external" or "user-defined" closure is user-defined and has an associated
 * scope created each time it is being executed. */
Object *closure_new(Object *self, Object *process)
{
    Process *processData = process->data;
	u8 *bytecode = processData->bytecode - 1;
	Size IP = processData->IP;	
    Object *closure = object_new(self);
    Closure *closureData = malloc(sizeof(Closure));
    closure->data = closureData;
    Object *scope = stackTop(&processData->scopes);
    closureData->type = userDefinedClosure;
    closureData->parent = scope;
    closureData->bytecode = bytecode + IP;
    closureData->argc = bytecode[IP + 1];
	return closure;
}

void closure_whileTrue(Object *self, Object *block)
{
	while (send(self, "eval") == trueObject)
		send(block, "eval");
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

void console_print(Object *self, Object *str)
{
    printf("%S", str);
}

void console_printNl(Object *self, Object *str)
{
    printf("%S\n", str);
}

Object *console_toString(Object *self)
{
    char strBuffer[30];
	sprintf(strBuffer, "<Console at %x>", self);
	return string_new(stringProto, strdup(strBuffer));
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
     * symbol. */
    printf("%S does not understand symbol '%s'\n", self, symbol->data);
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

// forward declaration
void scope_setVar(Object *self, Object *symbol, Object *value);

/* Reads from the bytecode a list of symbols. Note: the scope's closure must
 * be set outside of this function. */
Object *scope_new(Object *self, Object *process, Object *containing,
                  Object *caller, bool readBytecode)
{
    Process *processData = process->data;
    
    Object *scope = object_new(self);
    Scope *scopeData = malloc(sizeof(Scope));
    scope->data = scopeData;
    scopeData->thisWorld = ((Scope*)caller->data)->thisWorld;
    scopeData->containing = containing;
    scopeData->caller = caller;
    /* Now we make a mapping from the index that, in the bytecode, represents
     * the symbol to the actual symbol object. Because the symbols in the
     * bytecode have contiguous indices starting from 0, we can allocate an
     * array. */
    if (readBytecode)
    {
		u8 *bytecode = processData->bytecode;
		Object **processSymbols = processData->symbols;
		Size argCount = readValue(bytecode, &processData->IP);
		Size varCount = readValue(bytecode, &processData->IP);
		Size symbolCount = argCount + varCount;
		
		if (symbolCount > 0)
		{
			Object **symbols = malloc(sizeof(Object*) * symbolCount);
			Stack *valueStack = &processData->values;
			/* Create a list of variables for the scope that is the proper size. */
			Size i;
			for (i = 0; i < symbolCount; i++)
			{
				Size value = readValue(bytecode, &processData->IP);
				symbols[i] = processSymbols[value];
			}
			scopeData->variables = varListDataNew(symbolCount, (void**)symbols);
			for (i = 0; i < argCount; i++)
				scope_setVar(scope, symbols[i], stackPop(valueStack));
			free(symbols);
		}
		else
		{
			scopeData->variables = varListDataNew(0, NULL);
		}
    }
    else
    {
		panic("not in use, not sure if ever useful");
	}
    
    return scope;
}

Object *currentProcess()
{
	return getCurrentThread()->process;
}

Object *scope_world(Object *self)
{
	Scope *scope = self->data;
	return scope->thisWorld;
}

Object *currentWorld()
{
	Process *process = getCurrentThread()->process->data;
	if (process == NULL)
		return ((Scope*)globalScope->data)->thisWorld;
	Scope *scope = ((Object*)stackTop(&process->scopes))->data;
	return scope->thisWorld;
}

/* This does not create a new thread for the process. This only tries to make a
 * new process object corresponding to the current thread.
 * 
 * Still, there should be at most one thread per process and at most one
 * process per thread. Processes are considered interpreter processes when they
 * are processing bytecode by means of the interpret() function. When the
 * bytecode runs out, if the process is not doing some computation in a C
 * subroutine, the process will exit.
 * 
 * Special care must be taken that a process has only one reference to its
 * bytecode. The bytecode pointer should be swappable at any time (so that
 * we may append more bytecode to the end, for example). */
 /// todo: check that the bytecode pointer is swappable like this everywhere
 /// todo: allow adding of new symbol indices in the middle of bytecode rather
 ///       than only at the header
Object *process_new(Object *self)
{
	if (currentThread->process != NULL)
		return currentThread->process;
    Object *process = object_new(self);
    Process *data = malloc(sizeof(Process));
    process->data = data;
    
    data->parent = NULL;
    stackNew(&data->values);
    stackNew(&data->scopes);
    // create process scope
    
    stackPush(&data->scopes, globalScope);
    // associate the process object with the system thread
    currentThread->process = process;
    return process;
}

Object *boolean_new(Object *self)
{
	panic("Cannot use boolean as a prototype");
	return NULL;
}

Object *boolean_ifTrue(Object *self, Object *block)
{
	if (self == trueObject)
		return send(block, "eval");
	else
		return NULL;
}

Object *boolean_ifFalse(Object *self, Object *block)
{
	if (self == falseObject)
		return send(block, "eval");
	else
		return NULL;
}

Object *boolean_ifTrueifFalse(Object *self, Object *trueBlock,
							                Object *falseBlock)
{
	printf("trueBlock %S, falseBlock %S\n", trueBlock, falseBlock);
	if (self == trueObject)
		return send(trueBlock, "eval");
	else
		return send(falseBlock, "eval");
}

Object *boolean_and(Object *self, Object *other)
{
	if (self == trueObject)
		return other;
	else
		return self;
}

Object *boolean_or(Object *self, Object *other)
{
	if (self == trueObject)
		return self;
	else
		return other;
}

Object *boolean_xor(Object *self, Object *other)
{
	if (self == trueObject)
		return send(other, "not");
	else
		return other;
}

Object *boolean_eq(Object *self, Object *other)
{
	if (self == trueObject)
		return other;
	else
		return send(other, "not");
}

Object *boolean_not(Object *self, Object *other)
{
	if (self == trueObject)
		return falseObject;
	else
		return trueObject;
}

Object *world_spawn(Object *self)
{
	
}

Object *world_eval(Object *self, Object *block)
{
	
}

Object *world_commit(Object *self)
{
	
}

/* This function must be called before any VM actions may be done. After this
 * function is called, any VM actions should be done in a thread with a
 * Process defined for it. Helper functions may be created for this later, but
 * the intention is not to use the VM much from C; instead, the intention is to
 * allow the interpreter to call C functions via the VM. */
void vmInstall()
{
    /* 0. Setup global tables */
    
    globalSymbolTable = stringMapNew(); /* string -> symbol */
    globalObjectSet = objectSetNew(16/*1024*/);
    
    /* 1. create and initialize methodTables */
    
    objectProto = object_new(NULL);
    
    methodTableMT = methodTable_new(NULL, 3);
    methodTableMT->methodTable = methodTableMT;
    
    objectMT = methodTable_new(methodTableMT, 5);
    methodTableMT->parent = objectProto;
    objectProto->methodTable = objectMT;
    
    Object *symbolMT = methodTable_new(methodTableMT, 2);
    symbolProto = object_new(objectProto);
    symbolProto->methodTable = symbolMT;
    
    Object *closureMT = methodTable_new(methodTableMT, 6);
    closureProto = object_new(objectProto);
    closureProto->methodTable = closureMT;
    
    /* 2. install the following methods: */
    bindSymbol = symbol("bind:");
    getSymbol = symbol("get:");
    newSymbol = symbol("new");
    DNUSymbol = symbol("doesNotUnderstand:");
    // methodTable.get(self, symbol)
    methodTable_addClosure(methodTableMT, getSymbol,
        closure_newInternal(closureProto, methodTable_get, 2));
    // object.new(self)
    methodTable_addClosure(objectMT, newSymbol,
        closure_newInternal(closureProto, object_new, 1));
    // object.bind(self, symbol)
    methodTable_addClosure(objectMT, bindSymbol,
		closure_newInternal(closureProto, object_bind, 2));
    // object.doesNotUnderstand(self, message)
    methodTable_addClosure(objectMT, symbol("doesNotUnderstand:"),
		closure_newInternal(closureProto, object_doesNotUnderstand, 2));
    // object.methodTable(self)
    methodTable_addClosure(objectMT, symbol("methodTable"),
        closure_newInternal(closureProto, object_methodTable, 1));
    // object.toString(self), will not work until after stringInstall()
    methodTable_addClosure(objectMT, symbol("toString"),
        closure_newInternal(closureProto, object_toString, 1));
    // methodTable.new(self, size)
    methodTable_addClosure(methodTableMT, symbol("new:"),
        closure_newInternal(closureProto, methodTable_new, 2));
    // methodTable.toString(self)
    methodTable_addClosure(methodTableMT, symbol("toString"),
        closure_newInternal(closureProto, methodTable_toString, 1));
    // symbol.new(self, string)
    methodTable_addClosure(symbolMT, symbol("new:"),
        closure_newInternal(closureProto, symbol_new, 2));
    // symbol.new(self, string)
    methodTable_addClosure(symbolMT, symbol("toString"),
        closure_newInternal(closureProto, symbol_toString, 1));
    // closure.with(self, args)
    methodTable_addClosure(closureMT, symbol("eval"),
        closure_newInternal(closureProto, closure_with, 1));
    // closure.with(self, args)
    methodTable_addClosure(closureMT, symbol(":"),
        closure_newInternal(closureProto, closure_with, 2));
    // closure.with(self, args)
    methodTable_addClosure(closureMT, symbol("::"),
        closure_newInternal(closureProto, closure_with, 3));
    // closure.with(self, args)
    methodTable_addClosure(closureMT, symbol(":::"),
        closure_newInternal(closureProto, closure_with, 4));
    // closure.toString(self)
    methodTable_addClosure(closureMT, symbol("toString"),
        closure_newInternal(closureProto, closure_toString, 1));
    // closure.whileTrue(self, block)
    methodTable_addClosure(closureMT, symbol("whileTrue:"),
        closure_newInternal(closureProto, closure_whileTrue, 2));
    
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
    
    
    /* 4. Install other base components */
    
    Object *booleanMT = object_send(methodTableMT, symbol("new:"), 10);
    Object *boolean = object_send(objectProto, newSymbol);
    boolean->methodTable = booleanMT;
    
    trueObject = object_send(boolean, symbol("new"));
    falseObject = object_send(boolean, symbol("new"));
    
    methodTable_addClosure(booleanMT, symbol("new"),
		closure_newInternal(closureProto, boolean_new, 1));
    methodTable_addClosure(booleanMT, symbol("ifTrue:"),
		closure_newInternal(closureProto, boolean_ifTrue, 2));
    methodTable_addClosure(booleanMT, symbol("ifFalse:"),
		closure_newInternal(closureProto, boolean_ifFalse, 2));
    methodTable_addClosure(booleanMT, symbol("ifTrue:ifFalse:"),
		closure_newInternal(closureProto, boolean_ifTrueifFalse, 3));
    methodTable_addClosure(booleanMT, symbol("and:"),
		closure_newInternal(closureProto, boolean_and, 2));
    methodTable_addClosure(booleanMT, symbol("or:"),
		closure_newInternal(closureProto, boolean_or, 2));
    methodTable_addClosure(booleanMT, symbol("xor:"),
		closure_newInternal(closureProto, boolean_xor, 2));
    methodTable_addClosure(booleanMT, symbol("=="),
		closure_newInternal(closureProto, boolean_eq, 2));
    methodTable_addClosure(booleanMT, symbol("not"),
		closure_newInternal(closureProto, boolean_not, 1));
    methodTable_addClosure(booleanMT, symbol("toString"),
		closure_newInternal(closureProto, boolean_toString, 1));
    
    numberInstall();
    stringInstall();
    arrayInstall();
    
    /* the current world is referenced by "this world", where "this" is a
     * special variable returning the current scope object */
    Object *worldMT = object_send(methodTableMT, symbol("new:"), 3);
    Object *worldProto = object_send(objectProto, newSymbol);
    worldProto->methodTable = worldMT;
    
    methodTable_addClosure(worldMT, symbol("spawn"),
		closure_newInternal(closureProto, world_spawn, 1));
    methodTable_addClosure(worldMT, symbol("commit"),
		closure_newInternal(closureProto, world_commit, 1));
    methodTable_addClosure(worldMT, symbol("eval:"),
		closure_newInternal(closureProto, world_eval, 2));
    
    Object *consoleMT = object_send(methodTableMT, symbol("new:"), 5);
    Object *console = object_send(objectProto, newSymbol);
    console->methodTable = consoleMT;
    printf("Console at %x\n", console);
    
    methodTable_addClosure(consoleMT, symbol("printTest"),
		closure_newInternal(closureProto, console_printTest, 1));
    methodTable_addClosure(consoleMT, symbol("print:"),
		closure_newInternal(closureProto, console_print, 2));
    methodTable_addClosure(consoleMT, symbol("printNl:"),
		closure_newInternal(closureProto, console_printNl, 2));
    methodTable_addClosure(consoleMT, symbol("new"),
		closure_newInternal(closureProto, newDisallowed, 1));
    methodTable_addClosure(consoleMT, symbol("toString"),
		closure_newInternal(closureProto, console_toString, 1));
    
    send(console, "print:", console);
    send(console, "printTest"); // should print VM CHECK: Success!
    
    Object *traitMT = methodTable_new(methodTableMT, 1);
    Object *traitProto = object_new(objectProto);
    traitProto->methodTable = traitMT;
    
    // trait new
    methodTable_addClosure(traitMT, symbol("new"),
        closure_newInternal(closureProto, trait_new, 1));
    
    Object *scopeMT = methodTable_new(methodTableMT, 1);
    Object *scopeProto = object_new(objectProto);
    scopeProto->methodTable = scopeMT;
    
    methodTable_addClosure(scopeMT, symbol("world"),
		closure_newInternal(closureProto, scope_world, 1));
    //methodTable_addClosure(scopeMT, symbol("return"),
	//	closure_newInternal(closureProto, scope_return, 1));
    
    /* 6. Make components accessible by defining global variables */
    
    Object *symbols_array[] =
    {
		symbol("Console"), console,
		symbol("true"), trueObject,
		symbol("false"), falseObject,
	};
    void **symbols = (void**)(Object**)symbols_array;
    Size symbols_array_len = 3;
    
    globalScope = object_new(objectProto);
    Scope *globalScopeData = malloc(sizeof(Scope));
    globalScope->data = globalScopeData;
    globalScopeData->thisWorld = object_new(worldProto);
    globalScopeData->variables =
		varListDataNewPairs(symbols_array_len, symbols);
}

/* This is looking up a variable, beginning with the local scope. The chain of
 * lookup is to try the current world in the current scope, then the parent
 * world in the current scope, then the current world in the parent scope, etc.
 * 
 * The worst case runtime is highly dependent on the number of scopes we have
 * to look through. That should be more reason to prefer more-local variables
 * to more-global ones. */
Object *scope_lookupVar(Object *self, Object *symbol)
{
	/// todo: throw errors instead of panicking
	if (symbol == symbol("this"))
		return self;
	Scope *scope = self->data;
	assert(scope != NULL, "lookup error");
    Object *world = scope->thisWorld;
    Object *thisWorld = world;
    Object *value;
    while (true)
    {
        value = varListLookup(scope->variables, symbol, world);
        if (value != NULL)
            return value;
        if (world == scope->thisWorld)
        {
            if (scope == (Scope*)globalScope->data)
                panic("lookup Error: variable '%s' not found!", symbol);
            assert(scope->containing != NULL, "lookup error");
            Scope *containing = scope->containing->data;
            if (scope == containing) // root
                panic("lookup Error: scope does not lead back to global scope");
            scope = containing; // scope = scope->containing
            world = thisWorld;
        }
        else
        {
			panic("not implemented");
            world = ((World*)world->data)->parent;
            if (unlikely(world == NULL))
                panic("lookup Error");
        }
    }
    return NULL;
}

/* This should mimic the previous function in the way it finds a variable.
 * All variables must be declared at the top of their scope, so it does not
 * ever create a variable if it doesn't find one (this should be taken care
 * of by the varList routine). */
void scope_setVar(Object *self, Object *symbol, Object *value)
{
	Scope *scope = self->data;
	Object *world = scope->thisWorld;
	bool set;
	while (true)
	{
		Scope *containing = scope->containing->data;
        set = varListDataSet(scope->variables, symbol, world, value);
        if (set)
			break;
		if (scope == (Scope*)globalScope->data)
            panic("setVar Error: variable '%s' not found!", symbol->data);
        scope = scope->containing->data;
        /// todo: worlds (?)
    }
}

Object *interpret(Object *closure)
{
    Object *process = currentProcess();
    assert(process != NULL, "Create a new process first.");
    
    Process *processData = process->data;
    Object **symbols = processData->symbols;
    Stack *valueStack = &processData->values;
    Stack *scopeStack = &processData->scopes;
    
    u8 *bytecode = processData->bytecode;
    Size *IP = &processData->IP;
    Closure *closureData;
    
    if (closure == NULL) // new file
    {
        // Read the bytecode header defining interned symbols.
        Size symbolCount = readValue(bytecode, IP);
        if (symbolCount > 0)
        {
			// This is a process-wide symbol list unique to the given bytecode.
			symbols = malloc(sizeof(Object*) * symbolCount);
			Size i;
			for (i = 0; i < symbolCount; i++)
			{
				String s = readString(bytecode, IP);
				symbols[i] = symbol(s);
			}
			processData->symbols = symbols;
		}
		closure = closure_new(closureProto, process);
		closureData = closure->data;
    }
    else
    {
		closureData = closure->data;
		processData->IP = closureData->bytecode - processData->bytecode;
	}
    
    assert(readValue(bytecode, IP) == blockBC,
			"Expected block: malformed bytecode (IP=%i)", *IP-1);
    
    /* We've just begun executing a block, so create the scope for that block.
     * Note that scope_new reads bytecode to create its varlist. */
    {
		Object *scope = scope_new(scopeProto, process, closureData->parent,
								  stackTop(scopeStack), true);
		Scope *scopeData = scope->data;
		scopeData->closure = closure;
		stackPush(scopeStack, scope);
	}
    
	while (true)
	{
        Size value = readValue(bytecode, IP);
		printf("executing %2i: %x, %s\n", *IP - 1,
		       value, bytecodes[value - 0x80]);
		switch (value)
		{
			case integerBC:
			{
                /* The value of the integer is given as a string. */
				String s = readString(bytecode, IP);
				Object *integer = integer32_new(integer32Proto,
				                                strtoul(s, NULL, 10));
				stackPush(valueStack, integer);
			}
            break;
			case doubleBC:
                /* The value of the double is given as a string. */
				panic("not implemented");
            break;
			case stringBC:
			{
			    String s = readString(bytecode, IP);
			    Object *str = string_new(stringProto, s);
			    stackPush(valueStack, str);
			}
            break;
			case charBC:
				panic("not implemented");
            break;
			case symbolBC:
                /* use the symbol array to find the symbol object which has
                 * already been created at the start of the file */
				panic("not implemented");
            break;
			case arrayBC:
				panic("not implemented");
            break;
			case blockBC:
			{
				Object *closureNew = closure_new(closureProto, process);
				// increment bytecode until the end of closure definition
				while (readValue(bytecode, IP) != endBC) {}
				stackPush(valueStack, closureNew);
				Closure *data = closureNew->data;
			}
            break;
			case variableBC:
			{
                /* We have found a variable, so we need to look up its value. */
                Object *symbol = symbols[readValue(bytecode, IP)];
                Object *value = scope_lookupVar(stackTop(scopeStack), symbol);
				stackPush(valueStack, value);
			}
            break;
            case cascadeBC:
            {
				panic("not implemented");
			} break;
			case messageBC:
			{
				/* argc is the number of arguments not including recipient */
				Object *symbol = symbols[readValue(bytecode, IP)];
				Size argc = readValue(bytecode, IP);
				Object **args = (Object**)stackAt(valueStack, argc);
				Object *recipient = args[0];
			    //printf("Sending %s to %S, argc %i\n", symbol->data, recipient, argc);
			    Object *method = object_bind(recipient, symbol);
			    if (method == NULL)
			    {
					printf("Sent '%s' to %S, found no method\n",
							symbol->data, recipient);
					panic("null method");
				}
				
				Object *scope = stackTop(scopeStack);
				Scope *scopeData = scope->data;
				scopeData->IP = processData->IP;
				scopeData->bytecode = processData->bytecode;
				
				/// spawn thread?
				
				Object *result = closure_withArray(method, args);
				
				stackPopMany(valueStack, argc); // now args no longer needed on stack
				stackPush(valueStack, result);
			}
            break;
			case stopBC: /* Clears the current stack. Possibly will cause GC? */
				//stackPop(valueStack);
            break;
			case setBC:
			{
			    Object *symbol = symbols[readValue(bytecode, IP)];
			    Object *value = stackPop(valueStack);
			    scope_setVar(stackTop(scopeStack), symbol, value);
			}
            break;
			case endBC: /* Returns from the current block */
			{
				stackPop(scopeStack);
				Object *scope = stackTop(scopeStack);
				Scope *scopeData = scope->data;
				processData->IP = scopeData->IP;
				processData->bytecode = scopeData->bytecode;
				return stackPop(valueStack);
			}
            break;
			case objectBC: /* Define an object */
				panic("not implemented");
            break;
            case EOFBC:
				return stackPop(valueStack);
            break;
			default:
				panic("invalid bytecode");
			break;
		}
	}
	panic("not implemented");
}

void interpretBytecode(u8 *bytecode)
{
	/// todo: check that this process isn't already executing bytecode
	/// (this function should not be used to recurse!)
	Object *process = currentProcess();
	if (process == NULL) // create new process
		process = process_new(objectProto);
	Process *processData = process->data;
	processData->bytecode = bytecode;
	processData->IP = 0;
	interpret(NULL);
}
