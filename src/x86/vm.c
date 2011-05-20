#include <vm.h>
#include <threading.h>
#include <mm.h>
#include <data.h>
#include <string.h>
#include <types.h>
#include <stdarg.h>

/* The basic object model is based on the object model described in
 * the paper "Open, extensible object models" by Piumarta and Warth. */

/* Basic objects */
Object *symbolObject,
       *objectObject,
       *vtableObject;

/* Symbols */
Object *lookupSymbol,
       *addMethodSymbol,
       *allocateSymbol,
       *delegatedSymbol,
       *internSymbol;

/* Vtables, whose data field is a Map<symbol, method> */
Object *vtablevt,
       *symbolvt,
       *objectvt;

Map *globalSymbolTable; // <string, symbol>

/* Given a vtable, returns a new object with that vtable */
Object *vtableAllocate(Object *self)
{
    Object *object = malloc(sizeof(Object));
    object->vtable = self;
    return object;    
}

/* Given a vtable, returns a newly allocated child vtable */
Object *vtableDelegated(Object *self)
{
    Object *child;
    if (self != NULL)
        child = vtableAllocate(self->vtable);
    else
        child = vtableAllocate(NULL);
    child->parent = self;
    child->data = mapNew();
    return child;
}

Object *symbolNew(String string)
{
    Object *symbol = malloc(sizeof(Object));
    symbol->vtable = symbolvt;
    symbol->data = string;
    return symbol;
}

/* The receiver here is irrelevent, use to create/obtain a symbol from a string */
Object *symbolIntern(Object *symbol, String string)
{
    void *value;
    if ((value = mapGet(globalSymbolTable, string, stringKey)) != NULL)
        return value;
    Object *newSymbol = symbolNew(string);
    mapSet(globalSymbolTable, string, newSymbol, stringKey);
    return newSymbol;
}

void vtableAddMethod(Object *vtable, Object *symbol, Method *method)
{
    mapSetVal(vtable->data, symbol, method);
}

Method *vtableLookup(Object *vtable, Object *symbol)
{
    Method *method;
    do
    {
        if ((method = mapGetVal(vtable->data, symbol)) != NULL)
            return method;
    } while ((vtable = vtable->parent) != NULL);
    return NULL;
}

Method *bind(Object *self, Object *messageName);

/* Given an object and the symbol representing a message, send the
 * message with some number of arguments */
Object *send(Object *self, Object *messageName, ...)
{
    Method *method = bind(self, messageName);
    assert(method->argc == chrcount(messageName->data, ':'), "Internal VM Error");
    /* execute the given method, which is NOT an object */
    if (method->type == internalFunction)
    {
        va_list argptr;
        va_start(argptr, messageName);
        switch (method->argc)
        {
            case 0:
            {
                Object *(*func)(Object*) = method->funcptr;
                return func(self);
            } break;
            case 1:
            {
                Object *(*func)(Object*, Object*) = method->funcptr;
                return func(self, va_arg(argptr, Object*));
            } break;
            case 2:
            {   
                Object *(*func)(Object*, Object*, Object*) = method->funcptr;
                Object *arg = va_arg(argptr, Object*);
                return func(self, arg, va_arg(argptr, Object*));
            } break;
            default:
                panic("Not implemented");
            break;
        }
    }
    panic("Not implemented");
    return NULL;
}

/* Find a method on an object by sending the lookup message */
Method *bind(Object *self, Object *messageName)
{
    Method *method;
    if (messageName == lookupSymbol && self == vtablevt)
        method = vtableLookup(self->vtable, lookupSymbol);
    else
        method = (Method*)send(self->vtable, lookupSymbol, messageName);
    if (method == NULL)
        panic("Method not found");
        //send(self, doesNotUnderstandSymbol, messageName);
    return method;
}

/* Creates an internal function method */
Method *methodNew(void *funcptr, Size argc)
{
    Method *method = malloc(sizeof(Method));
    method->type = internalFunction;
    method->argc = argc;
    method->funcptr = funcptr;
    return method;
}

void initialize()
{
    globalSymbolTable = mapNew();
    
    vtablevt = vtableDelegated(NULL);
    vtablevt->vtable = vtablevt;

    objectvt = vtableDelegated(NULL);
    objectvt->vtable = vtablevt;
    vtablevt->parent = objectvt;

    symbolvt = vtableDelegated(objectvt);

    lookupSymbol = symbolIntern(NULL, "lookup:");
    vtableAddMethod(vtablevt, lookupSymbol, methodNew(vtableLookup, 1));
    
    addMethodSymbol = symbolIntern(NULL, "addMethod:def:");
    vtableAddMethod(vtablevt, addMethodSymbol, methodNew(vtableAddMethod, 2));
    
    allocateSymbol = symbolIntern(NULL, "allocate");
    send(vtablevt, addMethodSymbol, allocateSymbol, methodNew(vtableAllocate, 0));
    
    symbolObject = send(symbolvt, allocateSymbol);
    
    internSymbol = symbolIntern(NULL, "intern:");
    send(symbolvt, addMethodSymbol, internSymbol, methodNew(symbolIntern, 1));

    delegatedSymbol = send(symbolObject, internSymbol, "delegated");
    printf("delegated: %s\n", delegatedSymbol->data);
    send(vtablevt, addMethodSymbol, delegatedSymbol, methodNew(vtableDelegated, 0));
}

void vmInstall()
{
    initialize();
    
    for (;;);
}

void runtimeRequire(bool value, String message)
{
	if (unlikely(value))
		printf(message);
}

/* Bytecode is not null-terminated! it is terminated with EOS and contains
 * multiple null characters */
ThreadFunc execute(u8 *bytecode)
{
	u8 *IP = bytecode;
	
	/* The first section of the code is the method interned header.
	 * We look at each entry, and create a dictionary (using the global
	 * method table) which has keys that equal the local interned value,
	 * and values that are the global interned value. */
	
	Size entries = (Size)*IP++;
	Map *internedMethodTable = mapNewSize(entries + 1);
	Size i;
	
	for (i = 0; i < entries; i++)
	{
		Size globalValue = (Size)mapGet(globalSymbolTable, (String)IP, stringKey);
		mapSet(internedMethodTable, (void*)i, (void*)globalValue, valueKey);
		while (*IP++);
	}
	
	//mapDebug(internedMethodTable);
	for (;;);
	
    while (true)
    {
		//printf("%x %c\n", *IP, *IP); // print the byte we're looking at
        switch (*IP++)
        {
            case 0x80: /* Import */
            {
                panic("Not implemented!");
            } break;
            case 0x81: /* String */
            {
                panic("Not implemented!");
            } break;
            case 0x82: /* Number */
            {
                panic("Not implemented!");
            } break;
            case 0x83: /* Keyword */
            {
				panic("Not implemented!");
            } break;
            case 0x84: /* Call Method */
            {
                panic("Not implemented!");
            } break;
            case 0x85: /* Begin List */
            {
                panic("Not implemented!");
            } break;
            case 0x86: /* Begin Procedure */
            {
                panic("Not implemented!");
            } break;
            case 0x87:
            {
                panic("Not implemented!");
            } break;
            case 0x88:
            {
                panic("Not implemented!");
            } break;
            case 0x89: /* End Statement */
            {
				// todo: clear value stack
				panic("Not implemented!");
            } break;
            case 0x8A:
            {
                panic("Not implemented!");
            } break;
            case 0x8B: /* Equality */
            {
				panic("Not implemented!");
            } break;
            case 0x8C: /* Binary method */
            {
				panic("Not implemented!");
            } break;
            case 0xFF: /* EOS */
            {
                printf("Execution done! Exiting thread.");
                endThread();
            } break;
            default:
            {
				runtimeRequire(false, "Execution error: Malformed bytecode.");
            } break;
        }
    }
}
