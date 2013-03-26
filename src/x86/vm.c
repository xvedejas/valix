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
    new->methodTable = (self == NULL)?NULL:self->methodTable;
    new->data = NULL;
    objectSetAdd(globalObjectSet, new);
    return new;
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

Object *closure_with(Object *self, ...)
    /*
    ** Executes a closure (self) with the given arguments.
    ** Intended to work with both user-defined and internal (C-function) closures
    */
    /// todo: support for 8-bit and 16-bit wide values (?)
{
	assert(self != NULL && self->data != NULL, "NULL error");
    Closure *closureData = self->data;
    va_list argptr;
    va_start(argptr, self);
    Size arrayIndex = 0;
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
                    // Expecting unsigned integer, if object, try to convert
                    case 'u':
                    {
                        if (isObject(argument))
                        {
							printf("%s\n", argString);
                            ((void**)argptr)[-1] = send(argument, "asU32Int");
                        }
                    } break;
                    // Expecting signed integer, if object, convert
                    case 's':
                    {
                        if (isObject(argument))
                        {
                            ((void**)argptr)[-1] = object_send(argument,
                                symbol("asS32Int"));
                        }
                    } break;
                    // Expecting object
                    case 'o':
                    {
                        if (!isObject(argument))
                        {
							// try to see if it's an array of objects
							Object *tryArg = ((Object**)argument)[arrayIndex];
							if (isObject(tryArg))
							{
								((void**)argptr)[-1] = tryArg;
								arrayIndex++;
							}
							else
							{
								panic("VM Error, expected an object, given "
                                      "some other value. Please construct and "
                                      "pass a valid object. Arg %x.", argument);
							}
						}
                    } break;
                    // Expecting string, if object, convert
                    case 'S':
                    {
                        if (isObject(argument))
                        {
                            ((void**)argptr)[-1] = object_send(argument,
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
             * what a method expects. Passing variable numbers of arguments is
             * not a planned feature; lists or arrays should be used instead.
             * 
             * Using callInternal() can make the program flow here a bit more
             * ambiguous, since we're calling some arbitrary C function without
             * regards to its type. */
            
            //indention++;///
            void *result = callInternal(closureData->function,
                closureData->argc + 1, arguments);
            /* The given type of "result" is what we currently have, but we want
             * to return an object, so here we do the proper conversion. */
            //indention--;///
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
			/// todo: instead of executing this here when the process already
			/// is running an interpreter, put in some sort of execution queue
            interpret(self);
        } break;
        default: panic("vm error, closure type %i not known", closureData->type);
    }
    return NULL;
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
     * value.
     * 
     * For typical, user-defined closures, the arguments are all objects.
     * However, for interfacing with C code, we need to know what sort of
     * objects are allowed. */
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
            /* Here, we're done. Ignore the first count, which was for return
             * type and not any argument. */
                return count - 1;
            default:
                panic("Improperly formatted argstring, character %c",
                    argString[-1]);
        }
    }
    return 0;
}

/* The given argument string tells what sort of arguments are expected by the
 * given function.
 * 
 * Example: "SSous" means (String,Object*,u32,s32), return value is a String
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

/* An "external" or "user-defined" closure is user-defined and has an associated
 * scope created each time it is being executed. */
Object *closure_new(Object *self, Object *process)
{
    Process *processData = process->data;
	u8 *bytecode = processData->bytecode;
	Size *IP = &processData->IP;	
    Object *closure = object_new(self);
    Closure *closureData = closure->data;
    Object *scope = stackTop(&processData->scopes);
    closureData->type = userDefinedClosure;
    closureData->parent = scope;
    closureData->bytecode = bytecode + *IP;
	return closure;
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

void scope_setVar(Object *self, Object *symbol, Object *value);

/* Reads from the bytecode a list of symbols. */
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
		Size argc = readValue(bytecode, &processData->IP);
		Size varc = readValue(bytecode, &processData->IP);
		Size symbolCount = argc + varc;
		
		if (symbolCount > 0)
		{
			Object **symbols = malloc(sizeof(Object*) * symbolCount);
			
			/* Create a list of variables for the scope that is the proper size. */
			Size i;
			for (i = 0; i < symbolCount; i++)
				symbols[i] = processSymbols[readValue(bytecode,
				                                      &processData->IP)];
			scopeData->variables = varListDataNew(symbolCount, (void**)symbols);
			
			/* Get the arguments off the stack */
			Stack *valueStack = &processData->values;
			for (i = 0; i < argc; i++)
				scope_setVar(scope, symbols[i], stackPop(valueStack));
			
			free(symbols);
		}
    }
    else
    {
		scopeData->variables = varListDataNew(0, NULL);
	}
    
    return scope;
}

Object *currentProcess()
{
	return getCurrentThread()->process;
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
 ///       than only at the header (will require parser modification)
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

Object *process_spawn(Object *self, Object *block)
{
	panic("not implemented!");
	return NULL;
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
    
    Object *consoleMT = object_send(methodTableMT, symbol("new:"), 2);
    Object *console = object_send(objectProto, newSymbol);
    console->methodTable = consoleMT;
    
    methodTable_addClosure(consoleMT, symbol("printTest"),
		closure_newInternal(closureProto, console_printTest, "vo"));
    methodTable_addClosure(consoleMT, symbol("print:"),
		closure_newInternal(closureProto, console_print, "voo"));
    
    send(console, "printTest"); // should print VM CHECK: Success!
    
    /* 5. Install other base components */
    
    Object *traitMT = methodTable_new(methodTableMT, 1);
    Object *traitProto = object_new(objectProto);
    traitProto->methodTable = traitMT;
    
    // trait new
    methodTable_addClosure(traitMT, symbol("new"),
        closure_newInternal(closureProto, trait_new, "oo"));
    
    Object *scopeMT = methodTable_new(methodTableMT, 1);
    Object *scopeProto = object_new(objectProto);
    scopeProto->methodTable = scopeMT;
    
    Object *worldMT = methodTable_new(methodTableMT, 1);
    Object *worldProto = object_new(objectProto);
    worldProto->methodTable = worldMT;
    
    stringInstall();
    
    /* 6. Make components accessible by defining global variables */
    
    Object *symbols_array[] = { symbol("Console"), console, };
    void **symbols = (void**)(Object**)symbols_array;
    Size symbols_array_len = 1;
    
    globalScope = object_new(objectProto);
    Scope *globalScopeData = malloc(sizeof(Scope));
    globalScope->data = globalScopeData;
    globalScopeData->thisWorld = object_new(worldProto);
    globalScopeData->variables =
		varListDataNewPairs(symbols_array_len, symbols);
}

/* This is looking up a variable, beginning with the local scope. The chain of
 * lookup is to try the current world in the current scope, then the current
 * world in the parent scope, then the parent world in the parent scope, etc.
 * 
 * The worst case runtime is highly dependent on the number of scopes we have
 * to look through. That should be more reason to prefer more-local variables
 * to more-global ones. */
Object *scope_lookupVar(Object *self, Object *symbol)
{
	/// todo: throw error instead of panicking
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
        indention++;
        scope = scope->containing->data;
        /// todo: worlds (?)
    }
}

/* We are entering a new scope. The new scope's caller is the last scope.
 * Typically operates on currentThread->process */
void process_pushScope(Object *self, Object *scopeObj)
{
    Scope *scope = scopeObj->data;
    Process *process = self->data;
	scope->caller = stackTop(&process->scopes);
    stackPush(&process->scopes, scope);
}

/* We are exiting the current scope, back to the caller scope.
 * Typically operates on currentThread->process */
Object *process_popScope(Object *self)
{
    Process *process = self->data;
    return stackPop(&process->scopes);
    /* It is important to note that we do not free the scope we are leaving
     * here. In fact, some of its local variables could still be referenced
     * by closures defined in that scope and possibly returned by that scope.
     * This is part of the reason we rely on garbage collection to destroy
     * the scope later on */
}

/* Typically operates on currentThread->process */
void process_pushValue(Object *self, Object *value)
{
    Process *process = self->data;
    stackPush(&process->values, value);
}

/* Typically operates on currentThread->process */
Object *process_popValue(Object *self)
{
	Process *process = self->data;
    return stackPop(&process->values);
}

#define currentScope process->scope
#define currentBlock process->scope->block
#define stack process->stack

void interpret()
{
    Object *process = currentProcess();
    
    assert(process != NULL, "Create a new process first.");
    
    Process *processData = process->data;
    Object **symbols = processData->symbols;
    u8 *bytecode = processData->bytecode;
    Stack *valueStack = &processData->values;
    
    if (processData->IP == 0) // new file
    {   
        // Read the bytecode header defining interned symbols.
        Size symbolCount = readValue(bytecode, &processData->IP);
        if (symbolCount > 0)
        {
			// This is a process-wide symbol list unique to the given bytecode.
			symbols = malloc(sizeof(Object*) * symbolCount);
			Size i;
			for (i = 0; i < symbolCount; i++)
			{
				String s = readString(bytecode, &processData->IP);
				symbols[i] = symbol(s);
			}
			processData->symbols = symbols;
		}
    }
    
    assert(readValue(bytecode, &processData->IP) == blockBC,
			"Expected block: malformed bytecode");
    
    /* Now create the closure */
    Object *closure = closure_new(closureProto, process);
    Closure *closureData = closure->data;
    
    /* We've just begun executing a block, so create the scope for that block.
     * Note that scope_new reads bytecode to create its varlist. */
    Object *scope = scope_new(scopeProto, process, closureData->parent,
                              stackTop(&processData->scopes), true);
    stackPush(&processData->scopes, scope);
    
	while (true)
	{
        Size value = readValue(bytecode, &processData->IP);
		printf("executing %2i: %x, %s\n", processData->IP - 1,
		       value, bytecodes[value - 0x80]);
		switch (value)
		{
			case integerBC:
			{
                /* The value of the integer is given as a string. */
				String s = readString(bytecode, &processData->IP);
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
			    String s = readString(bytecode, &processData->IP);
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
				panic("not implemented");
				stackPush(valueStack, closureNew);
			}
            break;
			case variableBC:
			{
                /* We have found a variable, so we need to look up its value. */
                Object *symbol = symbols[readValue(bytecode, &processData->IP)];
                Object *value = scope_lookupVar(scope, symbol);
				stackPush(valueStack, value);
			}
            break;
			case messageBC:
			{
				/* argc is the number of arguments not including recipient */
				Object *symbol = symbols[readValue(bytecode, &processData->IP)];
				Size argc = readValue(bytecode, &processData->IP);
				Object **args = (Object**)stackPopMany(valueStack, argc);
				//printf("Sending message '%s', argc %i, args at %x\n", symbol->data, argc, args);
			    Object *recipient = stackPop(&processData->values);
			    Object *method = object_bind(recipient, symbol);
			    object_send(recipient, symbol, args);
			    /// todo: we sent the message, and as a result we might want to
			    /// evaluate a user-defined closure. Check the process struct
			    /// for a closure to enter.
			    free(args);
			}
            break;
			case stopBC: /* Clears the current stack. Possibly will cause GC? */
				/// (todo)
            break;
			case setBC:
			{
			    Object *symbol = symbols[readValue(bytecode, &processData->IP)];
			    Object *value = stackPop(valueStack);
			    scope_setVar(scope, symbol, value);
			}
            break;
			case endBC: /* Returns from the current block */
				/// todo: deal with scopes. Also, if there is something left on
				/// the stack, return that. There should be one or zero things
				/// left on the stack (depending on whether a stopBC was seen)
				return;
            break;
			case objectBC: /* Define an object */
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
            case EOFBC:
				return;
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
	interpret();
}
