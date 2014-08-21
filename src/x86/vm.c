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
 *      Xander Vedejas <xvedejas@gmail.com>
 */
 
#include <vm.h>
#include <mm.h>
#include <MethodTable.h>
#include <ObjectSet.h>
#include <StringMap.h>
#include <data.h>
#include <stdarg.h>
#include <Number.h>
#include <Array.h>
#include <Scope.h>
#include <Bool.h>
#include <World.h>
#include <cstring.h>
#include <String.h>
#include <parser.h>
#include <threading.h>
#include <types.h>

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
    printf("self %x parent %x gparent %x\n", self, self->parent, self->parent->parent);
    printf("arrayproto %x sequenceproto %x objectproto %x\n", arrayProto, sequenceProto, objectProto);
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
	return string_new(stringProto, strdup(self->character));
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
    return (stringMapGet(globalSymbolTable, self->character) == self);
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
	Closure *closure = self->closure;
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
              "(can't send message to null!)", symbol->character);
    
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
        method = methodTableDataGet(self->table, symbol);
    }
    else
    {
		Object *table = self->methodTable;
        assert(table != NULL, "Null method table to object at %x", self);
		method = send(table, "get:", symbol);
		if (method == NULL && self->parent != NULL)
			method = send(self->parent, "bind:", symbol);
    }
	entry->methodTable = methodTable;
	entry->symbol = symbol;
	entry->method = method;
    return method;
}

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
    Process *processData = process->process;
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
    closureData->world = scope->scope->world;
	return closure;
}

void closure_whileTrue(Object *self, Object *block)
{
	while (send(self, "eval") == trueObject)
		send(block, "eval");
}

Object *methodTable_get(Object *self, Object *symbol)
{
	return methodTableDataGet(self->table, symbol);
}

void methodTable_addClosure(Object *self, Object *symbol, Object *closure)
{
    MethodTable *table = self->table;
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
    printf("%S does not understand symbol '%s'\n", self, symbol->character);
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

Object *currentProcess()
{
	return getCurrentThread()->process;
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

/* This is the main method for catching errors. A new world is spawned in which
 * execution proceeds. */
Object *closure_on_catch(Object *self, Object *array_of_errors, Object *block)
{
    panic("Not Implemented");
    return NULL;
}

void traitInstall()
{
    Object *traitMT = methodTable_new(methodTableMT, 1);
    Object *traitProto = object_new(objectProto);
    traitProto->methodTable = traitMT;
    
    // trait new
    methodTable_addClosure(traitMT, symbol("new"),
    closure_newInternal(closureProto, trait_new, 1));
}

void consoleInstall()
{
    Object *consoleMT = object_send(methodTableMT, symbol("new:"), 5);
    console = object_send(objectProto, newSymbol);
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
    booleanInstall(); // defines trueObject, falseObject
    numberInstall();
    stringInstall();
    arrayInstall();
    worldInstall();
    consoleInstall(); // defines console
    traitInstall();
    
    /* 5. Make certain components accessible by defining global variables */
    
    Size symbols_array_len = 3;
    Object *symbols_array[] =
    {
		symbol("Console"), console,
		symbol("true"), trueObject,
		symbol("false"), falseObject,
	};
    void **global_symbols = (void**)(Object**)symbols_array;
    
    scopeInstall(global_symbols, symbols_array_len);
    
}

Object *interpret(Object *closure)
{
    Object *process = currentProcess();
    assert(process != NULL, "Create a new process first.");
    
    Process *processData = process->process;
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
		closureData = closure->closure;
        closureData->world = globalScope->scope->world;
    }
    else
    {
		closureData = closure->closure;
		processData->IP = closureData->bytecode - processData->bytecode;
	}
    
    assert(readValue(bytecode, IP) == blockBC,
			"Expected block: malformed bytecode (IP=%i)", *IP-1);
    
    // We've just begun executing a block, so create the scope for that block.
    Size argCount = readValue(bytecode, IP);
    Size varCount = readValue(bytecode, IP);
    Size symbolCount = argCount + varCount;
    
    Object **localSymbols = malloc(sizeof(Object*) * symbolCount * 2);
    /* Create a list of variables for the scope that is the proper size. */
    Size i;
    for (i = 0; i < symbolCount; i++)
    {
        Size value = readValue(bytecode, IP);
        localSymbols[2 * i] = symbols[value];
        localSymbols[2 * i + 1] = stackPop(valueStack);
    }
    Object *scope = scope_new(scopeProto, process, closureData->parent,
                              stackTop(scopeStack), closureData->world,
                              argCount, varCount, localSymbols);
    free(localSymbols);
    Scope *scopeData = scope->scope;
    scopeData->closure = closure;
    stackPush(scopeStack, scope);
    
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
            {
                Size elementCount = readValue(bytecode, IP);
                Object *objects[elementCount];
                
                Size i = elementCount;
                while (i --> 0)
                    objects[i] = stackPop(valueStack);
                
                Object *arrayNew = array_new(arrayProto,
                                             (Object**)objects,
                                             elementCount);
                
                stackPush(valueStack, arrayNew);
            }
            break;
			case blockBC:
			{
				Object *closureNew = closure_new(closureProto, process);
				// increment bytecode until the end of closure definition
				while (readValue(bytecode, IP) != endBC) {}
				stackPush(valueStack, closureNew);
				//Closure *data = closureNew->closure; // why is this line here?
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
			    //printf("Sending %s to %S, argc %i\n", symbol->character, recipient, argc);
			    Object *method = object_bind(recipient, symbol);
			    if (method == NULL)
			    {
					printf("Sent '%s' to %S, found no method\n",
							symbol->character, recipient);
					panic("null method");
				}
				
				Object *scope = stackTop(scopeStack);
				Scope *scopeData = scope->scope;
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
				Scope *scopeData = scope->scope;
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
	Process *processData = process->process;
	processData->bytecode = bytecode;
	processData->IP = 0;
	interpret(NULL);
}
