#include <vm.h>
#include <threading.h>
#include <mm.h>
#include <data.h>
#include <string.h>
#include <types.h>
#include <parser.h>
#include <stdarg.h>

/* Some notes:
 * 
 * For all internal methods that should be void, instead return NULL.
 * 
 */

/* The basic object model is based on the object model described in
 * the paper "Open, extensible object models" by Piumarta and Warth. */
       
Object *vtablevt;

Object *globalScope; // where all global classes should be as "local" variables

Map *globalSymbolTable; // <string, symbol>

#define runtimeError(message, args...) { printf("Runtime Error\n"); printf(message, ## args); endThread(); }

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
                runtimeError("Not implemented");
            break;
        }
    }
    else // method->type == userDefined
    {
        //Object *childScope = scopeNew(scope, closure, (Size)*IP++);
        /// recur over interpret()
    }
    runtimeError("Not implemented");
    return NULL;
}

/* Find a method on an object by sending the lookup message */
Object *bind(Object *self, Object *messageName)
{
    assert(messageName != NULL, "VM Error");
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
        runtimeError("VM error, you can't instantiate class Object");
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
    runtimeError("Object at %x does not understand message \"%s\" %x", self, symbol->data);
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
    Method *payload = calloc(1, sizeof(Method));
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
    executeSymbol           = symbolIntern(NULL, "execute:");
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

/* Not to be added as a message! The user should not be able to make
 * their own scope classes. This is just for convenience in C. */
Object *scopeNew(Object *parent, Object *closure, Size locals)
{
    Object *scope = new(scopeClass);
    scope->data = malloc(sizeof(Scope));
    ((Scope*)scope->data)->parent = parent;
    ((Scope*)scope->data)->closure = closure;
    ((Scope*)scope->data)->vars = mapNewSize(locals + 1);
    return scope;
}

Object *multiply(Object *self, Object *operand)
{
    return numberNew(numberClass, ((Size)self->data) * ((Size)operand->data));
}

Object *add(Object *self, Object *operand)
{
    return numberNew(numberClass, ((Size)self->data) + ((Size)operand->data));
}

Object *divide(Object *self, Object *operand)
{
    return numberNew(numberClass, ((Size)self->data) / ((Size)operand->data));
}

Object *subtract(Object *self, Object *operand)
{
    return numberNew(numberClass, ((Size)self->data) - ((Size)operand->data));
}

void print(Object *console, Object *argument)
{
    Object *string = send(argument, asStringSymbol);
    printf("%s\n", string->data);
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

Object *objectAsString(Object *self)
{
    return stringNew(stringClass, "");
}

#define translate(closure, internedValue)\
    mapGetVal(((Method*)closure->data)->translationTable, internedValue)

/* This returns a process object. Send it #execute: with a bytecode
 * argument to run the process. You can repeatedly send it #execute:
 * in fact, and it will run through each one sequentially. Processes
 * do not necessarily operate in their own thread. */
Object *processNew(Object *self)
{
    Object *process = new(processClass);
    /* 5 is an arbitrary guess at how many global variables there could be */
    process->data = scopeNew(globalScope, NULL, 5);
    return process;
}

Object *lookupVar(Object *scope, Object *symbol)
{
    Object *value;
    do value = mapGetVal(((Scope*)scope->data)->vars, symbol->data);
    while (value == NULL && (scope = ((Scope*)scope->data)->parent) != NULL);
    if (value == NULL)
        runtimeError("Undefined variable %s\n", symbol->data);
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

/* Given the scope, interpret the bytecode in its closure */
Object *interpret(Object *scope, ...)
{
    Object *closure = ((Scope*)scope->data)->closure;
    Object *lastValue; /* For returning values from this closure */
    Method *closurePayload = (Method*)closure->data;
    u8 *IP = closurePayload->bytearray->data;
    //Size argc = closurePayload->argc;
    /// don't do anything with args... yet
    
    /* Okay, the first bytecode should "Begin Procedure" 0x86,
     * double check that this is true */
    
    assert(*IP++ == 0x86, "Malformed bytecode error");
    
    /* The next byte tells us the number of local variables expected
     * in this procedure. Todo: pre-allocate this number in the
     * current scope's local variable map. */
    
    IP++; // just skip it for now
    
    /* Value stack */
    Stack *vStack = stackNew();
    
    while (*IP)
    {
        //printf("%x %c\n", *IP, *IP); // print the byte we're looking at
        switch (*IP++)
        {
            case importByteCode:
            {
                runtimeError("Not implemented!");
                /// this should create a new process over the imported
                /// file
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
                Object *symbol = translate(closure, index);
                stackPush(vStack, lookupVar(scope, symbol));
            } break;
            case callMethodByteCode:
            {
                Size index = (Size)*IP++;
                Size argc = (Size)*IP++;
                Object *methodSymbol = translate(closure, index);
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
                runtimeError("Not implemented!");
            } break;
            case beginProcedureByteCode:
            {
                runtimeError("Not implemented!");
                
            } break;
            case endListByteCode:
            {
                runtimeError("Not implemented!");
            } break;
            case endStatementByteCode:
            {
                // clear out the value stack
                lastValue = *vStack->bottom;
                while (stackPop(vStack));
            } break;
            case returnByteCode:
            {
                runtimeError("Not implemented!");
            } break;
            case setEqualByteCode:
            {
                Size index = (Size)*IP++;
                Object *symbol = translate(closure, index);
                setVar(scope, symbol, stackPop(vStack));
            } break;
            case newSymbolByteCode:
            {
                runtimeError("Not implemented!");
            } break;
            case setArgByteCode:
            {
                runtimeError("Not implemented!");
            } break;
            case 0xFF: /* EOS */
            {
                //printf("Execution done! Exiting thread.");
                //endThread();
                return lastValue;
            } break;
            default:
            {
                runtimeError("Execution error: Malformed bytecode.");
            } break;
        }
    }
    return NULL;
}

Object *processExecute(Object *self, u8 *headeredBytecode)
{
    u8 *IP = headeredBytecode;
    
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
    
    /* Setup the process scope */
    Object *processScope = self->data;
    Scope *scopePayload = processScope->data;
    Object *bytearray = send(bytearrayClass, newSymbol, IP);
    
    // Create a new closure
    ///if (scopePayload->closure != NULL)
    ///    closureDel(scopePayload->closure);
    scopePayload->closure = closureNew(closureClass, bytearray);
    Object *closure = scopePayload->closure;
    
    // Setup the closure's bytearray and translation table
    Method *closurePayload = closure->data;
    closurePayload->bytearray = bytearray;
    if (closurePayload->translationTable != NULL)
        mapDel(closurePayload->translationTable);
    closurePayload->translationTable = internTranslationTable;
    
    // Interpret
    Object *value = interpret(processScope);
    
    return value;
}

void initialize()
{
    scopeClass = subclass(objectClass);
    
    consoleClass = subclass(objectClass);
    
    
    processClass = subclass(objectClass);
    send(processClass->vtable, addMethodSymbol, newSymbol, methodNew(processNew, 0));
    send(processClass->vtable, addMethodSymbol, executeSymbol, methodNew(processExecute, 1));
    
    numberClass = subclass(objectClass);
    
    stringClass = subclass(objectClass);
    
    send(numberClass->vtable, addMethodSymbol, newSymbol, methodNew(numberNew, 1));
    send(numberClass->vtable, addMethodSymbol, asStringSymbol, methodNew(numberAsString, 0));
    send(stringClass->vtable, addMethodSymbol, newSymbol, methodNew(stringNew, 1));
    send(stringClass->vtable, addMethodSymbol, asStringSymbol, methodNew(stringAsString, 0));
    send(objectClass->vtable, addMethodSymbol, asStringSymbol, methodNew(objectAsString, 0));
    send(symbolClass->vtable, addMethodSymbol, newSymbol, methodNew(symbolNew, 1));
    
    send(consoleClass->vtable, addMethodSymbol, printSymbol, methodNew(print, 1));
    
    send(stringClass->vtable, addMethodSymbol, lengthSymbol, methodNew(stringLength, 0));
    send(symbolClass->vtable, addMethodSymbol, lengthSymbol, methodNew(stringLength, 0));
    
    Object *multiplySymbol = send(symbolClass, internSymbol, "*");
    send(numberClass->vtable, addMethodSymbol, multiplySymbol, methodNew(multiply, 1));
    
    Object *addSymbol = send(symbolClass, internSymbol, "+");
    send(numberClass->vtable, addMethodSymbol, addSymbol, methodNew(add, 1));
    
    Object *divideSymbol = send(symbolClass, internSymbol, "/");
    send(numberClass->vtable, addMethodSymbol, divideSymbol, methodNew(divide, 1));
    
    Object *subtractSymbol = send(symbolClass, internSymbol, "-");
    send(numberClass->vtable, addMethodSymbol, subtractSymbol, methodNew(subtract, 1));
    
    numberClass = send(numberClass->vtable, allocateSymbol);
    stringClass = send(stringClass->vtable, allocateSymbol);
    
    /* ByteArray */
    
    bytearrayClass = subclass(objectClass);
    send(bytearrayClass->vtable, addMethodSymbol, newSymbol, methodNew(bytearrayNew, 1));
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

}
