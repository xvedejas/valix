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
                            *((void**)argptr) = send(argument, "asU32Int");
                        }
                    } break;
                    // Expecting signed integer, if object, convert
                    case 's':
                    {
                        if (isObject(argument))
                        {
                            *((void**)argptr) = object_send(argument,
                                symbol("asS32Int"));
                        }
                    } break;
                    // Expecting object
                    case 'o':
                    {
                        if (!isObject(argument))
                            panic("VM Error, expected an object, given some "
                                  "other value. Please construct and pass a "
                                  "valid object. Arg %x.", argument);
                    } break;
                    // Expecting string, if object, convert
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
            panic("not implemented\n");
            u8 *bytecode = closureData->bytecode;
            /// This code will likely call interpret(), but some refactoring
            /// will probably need to be done besides that
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
 * scope created each time it is being executed. Assume the scope has already
 * been created and is the top scope known by the process. */
Object *closure_new(Object *self, Object *process)
{
    Process *processData = process->data;
	u8 *bytecode = processData->bytecode;
	Size *IP = &processData->IP;
	Size argCount = readValue(bytecode, IP);
	Size varCount = readValue(bytecode, IP); // does not include arg count
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

/* Reads from the bytecode a list of symbols. */
Object *scope_new(Object *self, Object *process, Object *containing,
                  Object *caller, Size symbolCount)
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
    if (symbolCount > 0)
    {
        Object **symbols = malloc(sizeof(Object*) * symbolCount);
        
        /* Create a list of variables for the scope that is the proper size. */
        Size i;
        for (i = 0; i < symbolCount; i++)
            symbols[i] = processData->symbols[readValue(processData->bytecode,
                                                        &processData->IP)];
        scopeData->variables = varListDataNew(symbolCount, (void**)symbols);
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
    
    stackPush(&data->scopes,
              scope_new(scopeProto, process, globalScope, globalScope, 0));
    assert(((Object*)stackTop(&data->scopes))->data != NULL, "");
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
    
    Object *scopeMT = methodTable_new(methodTableMT, 1);
    Object *scopeProto = object_new(objectProto);
    scopeProto->methodTable = scopeMT;
    
    methodTable_addClosure(traitMT, symbol("new"),
        closure_newInternal(closureProto, trait_new, "oo"));
    
    stringInstall();
    
    /* 6. Make components accessible by defining global variables */
    
    Object *symbols_array[] = { symbol("Console"), console, };
    void **symbols = (void**)(Object**)symbols_array;
    Size symbols_array_len = 1;
    
    globalScope = object_new(objectProto);
    Scope *globalScopeData = malloc(sizeof(Scope));
    globalScope->data = globalScopeData;
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
            {
				varListDebug(scope->variables);
                panic("lookup Error: variable not found!");
			}
            assert(scope->containing != NULL, "lookup error");
            Scope *containing = scope->containing->data;
            if (scope == containing) // root
                panic("lookup Error: scope does not lead back to global scope");
            scope = containing; // scope = scope->containing
            world = thisWorld;
        }
        else
        {
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
    panic("not implemented");
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

void interpret(u8 *bytecode)
{
    Object **symbols;
    
    Object *process, *scope, *closure;
    // Check and see if there's no process in this thread. This tells us whether
    // we are starting with a new file.
    bool newFile = false;
    if (currentThread->process == NULL)
    {
        // Creating a new process also creates a scope, but the closure must
        // be created separately.
        process = process_new(objectProto);
        newFile = true;
    }
    else
    {
        process = currentThread->process;
    }
    
    Process *processData = process->data;
    scope = stackTop(&processData->scopes);
	
    if (newFile)
    {
        // 1. create process corresponding to this thread.
        processData->bytecode = bytecode;
        processData->IP = 0;
        
        // 2. Read the bytecode header defining interned symbols.
        Size symbolCount = readValue(bytecode, &processData->IP);
        
        // This is a process-wide symbol list unique to the given bytecode.
        symbols = malloc(sizeof(Object*) * symbolCount);
        Size i;
        for (i = 0; i < symbolCount; i++)
        {
            String s = readString(bytecode, &processData->IP);
            symbols[i] = symbol(s);
        }
        
        processData->symbols = symbols;
        
        // 3. Read the closure definition to determine file-global variables.
        /* Format is roughly: [argc] [varc(not including argc)] args.. vars.. */
        closure = closure_new(objectProto, process);
    }
    else
    {
        // If there was already a process, we assume that we're not starting at the
        // beginning of a file, so we continue.
        symbols = processData->symbols;
    }
	
	while (true)
	{
        Size value = readValue(bytecode, &processData->IP);
		printf("%x, %s\n", value, bytecodes[value - 0x80]);
		switch (value)
		{
			case integerBC:
			{
                /* The value of the integer is given as a string. */
				String s = readString(bytecode, &processData->IP);
				Object *integer = integer32_new(integer32Proto,
				                                strtoul(s, NULL, 10));
				stackPush(&processData->values, integer);
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
			    stackPush(&processData->values, str);
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
				/*Object *closure = */closure_new(closureProto, process);
				// increment bytecode until the end of this closure definition
				panic("not implemented");
            break;
			case variableBC:
			{
                /* We have found a variable, so we need to look up its value. */
                Object *symbol = symbols[readValue(bytecode, &processData->IP)];
                Object *value = scope_lookupVar(scope, symbol);
				stackPush(&processData->values, value);
			}
            break;
			case messageBC:
			{
				/* argc is the number of arguments not including recipient */
				Object *symbol = symbols[readValue(bytecode, &processData->IP)];
			    Object *arg = stackPop(&processData->values);
			    Object *recipient = stackPop(&processData->values);
			    Object *method = object_bind(recipient, symbol);
			    object_send(recipient, symbol, arg);
			}
            break;
			case stopBC:
				/// todo: cleanup
				return;
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
				panic("invalid bytecode");
			break;
		}
	}
	panic("not implemented");
}
