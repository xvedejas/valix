#include <vm.h>
#include <threading.h>
#include <mm.h>
#include <data.h>
#include <string.h>
#include <types.h>
#include <parser.h>
#include <stdarg.h>

/* The basic object model is based on the object model described in
 * the paper "Open, extensible object models" by Piumarta and Warth. */

/* The following are some useful globals */

/* Basic objects */
Object *objectClass,
       *scopeClass,
       *symbolClass,
       *numberClass,
       *stringClass,
       *closureClass,
       *bytearrayClass,
       *consoleClass;

/* Symbols */
Object *newSymbol,
       *lengthSymbol,
       *newSymbolZArgs,
       *lookupSymbol,
       *addMethodSymbol,
       *allocateSymbol,
       *delegatedSymbol,
       *internSymbol,
       *doesNotUnderstandSymbol,
       *consoleSymbol,
       *printSymbol,
       *asStringSymbol;
       
Object *vtablevt;

Object *globalScope; // where all global classes should be as "local" variables

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
    Object *child = vtableAllocate(self);
    if (self != NULL)
        child->vtable = self->vtable;
    else
        child->vtable = NULL;
    child->parent = self;
    child->data = mapNew();
    return child;
}

/* The receiver here is irrelevent, use to create/obtain a symbol from a string */
Object *symbolIntern(Object *symbol, String string)
{
    void *value = mapGet(globalSymbolTable, string, stringKey);
    if (value != NULL)
        return value;
    Object *newSymbol = malloc(sizeof(Object));
    newSymbol->vtable = symbolClass->vtable;
    newSymbol->data = strdup(string);
    mapSet(globalSymbolTable, string, newSymbol, stringKey);
    return newSymbol;
}

Object *symbolNew(Object *self, Object *string)
{
    return symbolIntern(self, string->data);
}

void vtableAddMethod(Object *vtable, Object *symbol, Object *methodClosure)
{
    mapSetVal(vtable->data, symbol, methodClosure);
}

Object *vtableLookup(Object *vtable, Object *symbol)
{
    do
    {
        assert(vtable->data != NULL, "VM Error");
        Object *methodClosure = mapGetVal(vtable->data, symbol);
        if (methodClosure != NULL)
            return methodClosure;
    } while ((vtable = vtable->parent) != NULL);
    return NULL;
}

Object *bind(Object *self, Object *messageName);

Object *doMethod(Object *self, Object *methodClosure, ...)
{
    Method *method = methodClosure->data;
    if (method->type == internalFunction)
    {
        va_list argptr;
        va_start(argptr, methodClosure);
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
    else // method->type == userDefined
    {
        /// recur over interpret()
    }
    panic("Not implemented");
    return NULL;
}

/* Given an object and the symbol representing a message, send the
 * message with some number of arguments */
#define send(self, messageName, args...) \
    ({\
        Object *_self = self;\
        doMethod(_self, bind(_self, messageName), ## args);\
    })\

/* Find a method on an object by sending the lookup message */
Object *bind(Object *self, Object *messageName)
{
    Object *methodClosure;
    if (messageName == lookupSymbol && self == vtablevt)
        methodClosure = vtableLookup(self->vtable, lookupSymbol);
    else
        methodClosure = send(self->vtable, lookupSymbol, messageName);
    if (methodClosure == NULL)
        send(self, doesNotUnderstandSymbol, messageName);
    return methodClosure;
}

/* A basic constructor/instantiator */
Object *new(Object *self)
{
    if (self == objectClass)
        panic("VM error, you can't instantiate class Object");
    Object *new = vtableAllocate(self->vtable);
    new->parent = self;
    return new;
}

Object *subclass(Object *self)
{
    Object *vtable = vtableDelegated(self->vtable);
    Object *subclass = vtableAllocate(vtable);
    return subclass;
}

void doesNotUnderstand(Object *self, Object *symbol)
{
    panic("Object at %x does not understand message \"%s\"", self, symbol->data);
}

/* Creates an internal function method closure */
Object *methodNew(void *funcptr, Size argc)
{
    Method *method = malloc(sizeof(Method));
    method->type = internalFunction;
    method->argc = argc;
    method->funcptr = funcptr;
    Object *closure = new(closureClass);
    closure->data = method;
    return closure;
}

Object *closureNew(Object *self, Object *bytearray)
{
    Object *closure = new(self);
    Method *payload = malloc(sizeof(Method));
    payload->bytearray = bytearray;
    closure->data = payload;
    return closure;
}

void symbolSetup()
{
    lengthSymbol            = symbolIntern(NULL, "length");
    newSymbol               = symbolIntern(NULL, "new:");
    lookupSymbol            = symbolIntern(NULL, "lookup:");
    addMethodSymbol         = symbolIntern(NULL, "addMethod:def:");
    allocateSymbol          = symbolIntern(NULL, "allocate");
    internSymbol            = symbolIntern(NULL, "intern:");
    delegatedSymbol         = symbolIntern(NULL, "delegated");
    newSymbolZArgs          = symbolIntern(NULL, "new");
    doesNotUnderstandSymbol = symbolIntern(NULL, "doesNotUnderstand:");
    printSymbol             = symbolIntern(NULL, "print:");
    asStringSymbol          = symbolIntern(NULL, "asString");
}

void bootstrap()
{
    /* Initialize vtablevt, objectvt, and objectClass */
    vtablevt = vtableDelegated(NULL);
    vtablevt->vtable = vtablevt;

    Object *objectvt = vtableDelegated(NULL);
    objectvt->vtable = vtablevt;
    vtablevt->parent = objectvt;
    objectClass = vtableAllocate(objectvt);

    /* Setup symbolClass, closureClass, and lookup/addmethod methods */
    symbolClass = subclass(objectClass);
    globalSymbolTable = mapNew();
    symbolSetup();
    
    closureClass = subclass(objectClass);
    vtableAddMethod(closureClass->vtable, newSymbol, methodNew(closureNew, 1));

    vtableAddMethod(vtablevt, lookupSymbol, methodNew(vtableLookup, 1));

    vtableAddMethod(vtablevt, addMethodSymbol, methodNew(vtableAddMethod, 2));

    send(vtablevt, addMethodSymbol, allocateSymbol, methodNew(vtableAllocate, 0));

    send(symbolClass->vtable, addMethodSymbol, internSymbol, methodNew(symbolIntern, 1));

    send(vtablevt, addMethodSymbol, delegatedSymbol, methodNew(vtableDelegated, 0));

    Object *subclassSymbol = send(symbolClass, internSymbol, "subclass");
    send(objectClass->vtable, addMethodSymbol, subclassSymbol, methodNew(subclass, 0));

    send(objectClass->vtable, addMethodSymbol, doesNotUnderstandSymbol, methodNew(doesNotUnderstand, 1));
}

Object *numberNew(Object *self, Size value)
{
    Object *number = new(self);
    number->data = (void*)value;
    return number;
}

Object *stringNew(Object *self, String value)
{
    Object *string = new(self);
    string->data = value;
    return string;
}

Object *stringLength(Object *self)
{
    return numberNew(numberClass, strlen(self->data));
}

Object *bytearrayNew(Object *self, u8 *bytecode)
{
    Object *bytearray = new(self);
    bytearray->data = bytecode;
    return bytearray;
}

/* Not to be added as a message */
Object *scopeNew(Object *parent, Object *closure, Size locals)
{
    Object *scope = new(scopeClass);
    scope->data = malloc(sizeof(Scope));
    ((Scope*)scope->data)->parent = parent;
    ((Scope*)scope->data)->closure = closure;
    ((Scope*)scope->data)->vars = mapNewSize(locals + 1);
    return scope;
}

Object *multiply(Object *self, Object *factor)
{
    self->data = (void*)(((Size)self->data) * ((Size)factor->data));
    return self;
}

void print(Object *console, Object *argument)
{
    Object *string = send(argument, asStringSymbol);
    printf(string->data);
}

Object *numberAsString(Object *self)
{
    /// this allocation is naive...
    String string = malloc(sizeof(char) * 9);
    return send(stringClass, newSymbol, itoa((Size)self->data, string, 10));
}

Object *stringAsString(Object *self)
{
    return self;
}

void initialize()
{
    scopeClass = subclass(objectClass);
    consoleClass = subclass(objectClass);
    
    numberClass = subclass(objectClass);
    stringClass = subclass(objectClass);
    
    send(numberClass->vtable, addMethodSymbol, newSymbol, methodNew(numberNew, 1));
    send(numberClass->vtable, addMethodSymbol, asStringSymbol, methodNew(numberAsString, 0));
    send(stringClass->vtable, addMethodSymbol, newSymbol, methodNew(stringNew, 1));
    send(stringClass->vtable, addMethodSymbol, asStringSymbol, methodNew(stringAsString, 0));
    send(symbolClass->vtable, addMethodSymbol, newSymbol, methodNew(symbolNew, 1));
    
    send(consoleClass->vtable, addMethodSymbol, printSymbol, methodNew(print, 1));
    
    send(stringClass->vtable, addMethodSymbol, lengthSymbol, methodNew(stringLength, 0));
    send(symbolClass->vtable, addMethodSymbol, lengthSymbol, methodNew(stringLength, 0));
    
    Object *multiplySymbol = send(symbolClass, internSymbol, "*");
    send(numberClass->vtable, addMethodSymbol, multiplySymbol, methodNew(multiply, 1));
    
    numberClass = send(numberClass->vtable, allocateSymbol);
    stringClass = send(stringClass->vtable, allocateSymbol);
    
    /* ByteArray */
    
    bytearrayClass = subclass(objectClass);
    send(bytearrayClass->vtable, addMethodSymbol, newSymbol, methodNew(bytearrayNew, 1));
}

Object *lookupVar(Object *scope, Object *symbol)
{
    Object *value;
    do value = mapGetVal(((Scope*)scope->data)->vars, symbol->data);
    while (value == NULL && (scope = ((Scope*)scope->data)->parent) != NULL);
    assert(value != NULL, "Undefined variable %s\n", symbol->data);
    return value;
}

void setVar(Object *currentScope, Object *symbol, Object *value)
{
    /* Find if it's already defined */
    Object *scope = currentScope;
    do
    {
        Object *currentValue =
            mapGetVal(((Scope*)scope->data)->vars, symbol->data);
        if (currentValue != NULL)
        {
            mapSetVal(((Scope*)scope->data)->vars, symbol->data, value);
            return;
        }
    } while ((scope = ((Scope*)scope->data)->parent) != NULL);
    mapSetVal(((Scope*)currentScope->data)->vars, symbol->data, value);
}

void vmInstall()
{
    bootstrap();
    /* After bootstrap, send() may now be used */
    initialize();
    globalScope = scopeNew(NULL, NULL, 20 /*globalVarCount*/);
    /// setup the global scope
    setVar(globalScope, symbolIntern(NULL, "Console"), consoleClass);
}

/* Bytecode is not null-terminated! it is terminated with EOS and contains
 * multiple null characters. The execute() function should be used to
 * run an entire file of bytecode. */
ThreadFunc execute(u8 *bytecode)
{
    u8 *IP = bytecode;
    
    /* The first section of the code is the method interned header.
     * We look at each entry, and create a dictionary (using the global
     * method table) which has keys that equal the local interned value,
     * and values that are the global interned value. */
    
    Size entries = (Size)*IP++;
    Map *internTranslationTable = mapNewSize(entries + 1);
    Size i;
    for (i = 0; i < entries; i++)
    {
        Object *globalSymbol = symbolIntern(NULL, (String)IP);
        mapSet(internTranslationTable, (void*)i, globalSymbol, valueKey);
        while (*IP++);
    }
    
    Object *translate(Size internedValue)
    {
        return mapGetVal(internTranslationTable, internedValue);
    }
    
    auto Object *interpret(Object *closure, Object *scope, ...);
    Object *bytearray = send(bytearrayClass, newSymbol, IP);
    Object *fileClosureObject = send(closureClass, newSymbol, bytearray);    
    interpret(fileClosureObject, globalScope);
    
    Object *interpret(Object *closure, Object *parentScope, ...)
    {
        Method *closurePayload = (Method*)closure->data;
        //Size argc = closurePayload->argc;
        u8 *IP = closurePayload->bytearray->data;
        /// don't do anything with args... yet
        
        /* Okay, the first bytecode should "Begin Procedure" 0x86,
         * double check that this is true */
        
        assert(*IP++ == 0x86, "Malformed bytecode error");
        
        /* The next byte tells us the number of local variables expected
         * in this procedure. Create a map that is one greater than that
         * number so that it doesn't resize */
        
        Object *scope = scopeNew(parentScope, closure, (Size)*IP++);
        
        /* Value stack */
        
        Stack *vStack = stackNew();
        
        while (*IP != 0xFF)
        {
            //printf("%x %c\n", *IP, *IP); // print the byte we're looking at
            switch (*IP++)
            {
                case importByteCode:
                {
                    panic("Not implemented!");
                    /// this should recur over execute()
                } break;
                case newStringByteCode:
                {
                    Size len = strlen((String)IP);
                    Object *string = send(stringClass, newSymbol, (String)IP);
                    IP += len + 1;
                    stackPush(vStack, string);
                } break;
                case newNumberByteCode:
                {
                    Size len = strlen((String)IP);
                    Size val = strtoul((String)IP, NULL, 10);
                    Object *number = send(numberClass, newSymbol, val);
                    IP += len + 1;
                    stackPush(vStack, number);
                } break;
                case variableByteCode:
                {
                    Size index = (Size)*IP++;
                    Object *symbol = translate(index);
                    stackPush(vStack, lookupVar(scope, symbol));
                } break;
                case callMethodByteCode:
                {
                    Size index = (Size)*IP++;
                    Size argc = (Size)*IP++;
                    Object *methodSymbol = translate(index);
                    /* The arguments are at the top of the stack, and
                     * message receiver directly after that. */
                    Object *args[8]; // argc limit is 8
                    Size i;
                    for (i = 0; i < argc; i++)
                        args[i] = stackPop(vStack);
                    // now we can get the receiver
                    Object *receiver = stackPop(vStack);
                    Object *methodClosure = bind(receiver, methodSymbol);
                    assert(((Method*)methodClosure->data)->argc == argc, "VM Error");
                    Object *ret = NULL;
                    switch (argc)
                    {
                        case 0: { ret = doMethod(receiver, methodClosure); } break;
                        case 1: { ret = doMethod(receiver, methodClosure, args[0]); } break;
                        case 2: { ret = doMethod(receiver, methodClosure, args[0], args[1]); } break;
                        case 3: { ret = doMethod(receiver, methodClosure, args[0], args[1], args[2]); } break;
                        case 4: { ret = doMethod(receiver, methodClosure, args[0], args[1], args[2], args[3]); } break;
                        case 5: { ret = doMethod(receiver, methodClosure, args[0], args[1], args[2], args[3], args[4]); } break;
                        case 6: { ret = doMethod(receiver, methodClosure, args[0], args[1], args[2], args[3], args[4], args[5]); } break;
                        case 7: { ret = doMethod(receiver, methodClosure, args[0], args[1], args[2], args[3], args[4], args[5], args[6]); } break;
                        case 8: { ret = doMethod(receiver, methodClosure, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]); } break;
                    }
                    if (ret != NULL)
                        stackPush(vStack, ret);
                } break;
                case beginListByteCode:
                {
                    panic("Not implemented!");
                } break;
                case beginProcedureByteCode:
                {
                    panic("Not implemented!");
                    
                } break;
                case endListByteCode:
                {
                    panic("Not implemented!");
                } break;
                case endStatementByteCode:
                {
                    // clear out the value stack
                    while (stackPop(vStack));
                } break;
                case returnByteCode:
                {
                    panic("Not implemented!");
                } break;
                case setEqualByteCode:
                {
                    Size index = (Size)*IP++;
                    Object *symbol = translate(index);
                    setVar(scope, symbol, stackPop(vStack));
                } break;
                case newSymbolByteCode:
                {
                    panic("Not implemented!");
                } break;
                case setArgByteCode:
                {
                    panic("Not implemented!");
                } break;
                case 0xFF: /* EOS */
                {
                    //printf("Execution done! Exiting thread.");
                    //endThread();
                } break;
                default:
                {
                    panic("Execution error: Malformed bytecode.");
                } break;
            }
        }
        return NULL;
    }
}
