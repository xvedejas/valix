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

void setVar(Object *currentScope, Object *symbol, Object *value);

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

/* A basic constructor/instantiator */
Object *new(Object *self)
{
    if (self == objectClass)
        runtimeError("VM error, you can't instantiate class Object");
    Object *new = vtableAllocate(self->vtable);
    new->parent = self;
    return new;
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

Object *interpret(Object *scope, ...);
Object *bind(Object *self, Object *messageName);

Object *doMethod(Object *self, Object *closure, ...)
{
    Method *method = closure->data;
    va_list argptr;
    va_start(argptr, closure);
    Object *newScope = NULL;
    if (method->type == userDefinedClosure)
    {
        newScope = scopeNew(method->containingScope, closure, 5);
        setVar(newScope, selfSymbol, self);
    }
    Object *one, *two, *three, *four;
    switch (method->argc)
    {
        case 0:
            if (method->type == internalFunction)
            {
                Object *(*func)(Object*) = method->funcptr;
                return func(self);
            }
            else
                return interpret(newScope);
        case 1:
            one = va_arg(argptr, Object*);
            if (method->type == internalFunction)
            {
                Object *(*func)(Object*,Object*) = method->funcptr;
                return func(self, one);
            }
            else
                return interpret(newScope, one);
        case 2:
            one = va_arg(argptr, Object*);
            two = va_arg(argptr, Object*);
            if (method->type == internalFunction)
            {
                Object *(*func)(Object*,Object*,Object*) = method->funcptr;
                return func(self, one, two);
            }
            else
                return interpret(newScope, one, two);
        case 3:
            one = va_arg(argptr, Object*);
            two = va_arg(argptr, Object*);
            three = va_arg(argptr, Object*);
            if (method->type == internalFunction)
            {
                Object *(*func)(Object*,Object*,Object*,Object*) = method->funcptr;
                return func(self, one, two, three);
            }
            else
                return interpret(newScope, one, two, three);
        case 4:
            one = va_arg(argptr, Object*);
            two = va_arg(argptr, Object*);
            three = va_arg(argptr, Object*);
            four = va_arg(argptr, Object*);
            if (method->type == internalFunction)
            {
                Object *(*func)(Object*,Object*,Object*,Object*,Object*) = method->funcptr;
                return func(self, one, two, three, four);
            }
            else
                return interpret(newScope, one, two, three, four);
    }
    panic("Not supported");
    return NULL;
}

Object *doInternalMethod(Object *self, Object *methodClosure, ...)
{
    Method *method = methodClosure->data;
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

Object *closureNew(Object *self, Object *bytearray, Object *scope)
{
    Object *closure = new(self);
    Method *payload = calloc(1, sizeof(Method));
    payload->bytearray = bytearray;
    payload->containingScope = scope;
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
    callSymbol              = symbolIntern(NULL, "call");
    equivSymbol             = symbolIntern(NULL, "==");
    lessThanSymbol          = symbolIntern(NULL, "<");
    greaterThanSymbol       = symbolIntern(NULL, ">");
    selfSymbol       = symbolIntern(NULL, "self");
    whileSymbol             = symbolIntern(NULL, "whileTrue:");
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

Object *print(Object *console, Object *argument)
{
    Object *string = send(argument, asStringSymbol);
    printf("%s\n", string->data);
    return NULL;
}

Object *closureCallOne(Object *closure)
{
    return doMethod(NULL, closure);
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

Object *boolNew(Object *self, bool value)
{
    Object *boolean = new(self);
    boolean->data = (void*)value;
    return boolean;
}

Object *boolIfTrue(Object *self, Object *closure)
{
    Object *value = NULL;
    if (self->data)
        value = send(closure, callSymbol);
    return value;
}

Object *boolIfFalse(Object *self, Object *closure)
{
    Object *value = NULL;
    if (!self->data)
        value = send(closure, callSymbol);
    return value;
}

Object *boolIfTrueFalse(Object *self, Object *closureOne, Object *closureTwo)
{
    Object *value = NULL;
    if (self->data)
        value = send(closureOne, callSymbol);
    else
        value = send(closureTwo, callSymbol);
    return value;
}

Object *numberEquiv(Object *self, Object *number)
{
    if (self->data == number->data)
        return boolNew(boolClass, 1);
    return boolNew(boolClass, 0);
}

Object *numberGreaterThan(Object *self, Object *number)
{
    if ((Size)self->data > (Size)number->data)
        return boolNew(boolClass, 1);
    return boolNew(boolClass, 0);
}

Object *numberLessThan(Object *self, Object *number)
{
    if ((Size)self->data < (Size)number->data)
        return boolNew(boolClass, 1);
    return boolNew(boolClass, 0);
}

Object *closureWhile(Object *self, Object *closure)
{
    void *value = NULL;
    while (send(self, callSymbol)->data)
        value = send(closure, callSymbol);
    return value;
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
    do
        value = mapGetVal(((Scope*)scope->data)->vars, symbol->data);
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
                    case 2: { ret = doMethod(receiver, methodClosure, args[1], args[0]); } break;
                    case 3: { ret = doMethod(receiver, methodClosure, args[2], args[1], args[0]); } break;
                    case 4: { ret = doMethod(receiver, methodClosure, args[3], args[2], args[1], args[0]); } break;
                    case 5: { ret = doMethod(receiver, methodClosure, args[4], args[3], args[2], args[1], args[0]); } break;
                    case 6: { ret = doMethod(receiver, methodClosure, args[5], args[4], args[3], args[2], args[1], args[0]); } break;
                    case 7: { ret = doMethod(receiver, methodClosure, args[6], args[5], args[4], args[3], args[2], args[1], args[0]); } break;
                    case 8: { ret = doMethod(receiver, methodClosure, args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]); } break;
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
                /* Note; there are TWO begin procedure bytecodes for each
                 * block statement. Between them are the argument definitions */
                while (*IP != beginProcedureByteCode) IP++;
                Object *newClosure = closureNew(closureClass, bytearrayNew(bytearrayClass, IP), scope);
                ((Method*)newClosure->data)->type = userDefinedClosure;
                ((Method*)newClosure->data)->translationTable = ((Method*)closure->data)->translationTable;
                while (*IP++ != endProcedureByteCode);
                stackPush(vStack, newClosure);
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
            case endProcedureByteCode:
            {
                return lastValue;
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

/* Bytecode is not null-terminated! it is terminated with EOS and contains
 * multiple null characters. The processExecute() function should be used to
 * run an entire file of bytecode. The interpret() function is for running
 * blocks. */
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
    scopePayload->closure = closureNew(closureClass, bytearray, globalScope);
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
    // Scope //
    scopeClass = subclass(objectClass);
    
    // Console //
    consoleClass = subclass(objectClass);
    send(consoleClass->vtable, addMethodSymbol, printSymbol, methodNew(print, 1));
    
    // Process //
    processClass = subclass(objectClass);
    send(processClass->vtable, addMethodSymbol, newSymbol, methodNew(processNew, 0));
    send(processClass->vtable, addMethodSymbol, executeSymbol, methodNew(processExecute, 1));
    
    // Boolean //
    boolClass = subclass(objectClass);
    send(boolClass->vtable, addMethodSymbol, newSymbol, methodNew(boolNew, 1));
    Object *ifTrueSymbol = send(symbolClass, internSymbol, "ifTrue:");
    Object *ifFalseSymbol = send(symbolClass, internSymbol, "ifFalse:");
    Object *ifTrueFalseSymbol = send(symbolClass, internSymbol, "ifTrue:ifFalse:");
    send(boolClass->vtable, addMethodSymbol, ifTrueSymbol, methodNew(boolIfTrue, 1));
    send(boolClass->vtable, addMethodSymbol, ifFalseSymbol, methodNew(boolIfFalse, 1));
    send(boolClass->vtable, addMethodSymbol, ifTrueFalseSymbol, methodNew(boolIfTrueFalse, 2));
    
    // Number //
    numberClass = subclass(objectClass);
    send(numberClass->vtable, addMethodSymbol, newSymbol, methodNew(numberNew, 1));
    send(numberClass->vtable, addMethodSymbol, asStringSymbol, methodNew(numberAsString, 0));
    Object *multiplySymbol = send(symbolClass, internSymbol, "*");
    Object *addSymbol = send(symbolClass, internSymbol, "+");
    Object *divideSymbol = send(symbolClass, internSymbol, "/");
    Object *subtractSymbol = send(symbolClass, internSymbol, "-");
    send(numberClass->vtable, addMethodSymbol, multiplySymbol, methodNew(multiply, 1));
    send(numberClass->vtable, addMethodSymbol, addSymbol, methodNew(add, 1));
    send(numberClass->vtable, addMethodSymbol, divideSymbol, methodNew(divide, 1));
    send(numberClass->vtable, addMethodSymbol, subtractSymbol, methodNew(subtract, 1));
    send(numberClass->vtable, addMethodSymbol, equivSymbol, methodNew(numberEquiv, 1));
    send(numberClass->vtable, addMethodSymbol, greaterThanSymbol, methodNew(numberGreaterThan, 1));
    send(numberClass->vtable, addMethodSymbol, lessThanSymbol, methodNew(numberLessThan, 1));
    
    // String //
    stringClass = subclass(objectClass);
    send(stringClass->vtable, addMethodSymbol, newSymbol, methodNew(stringNew, 1));
    send(stringClass->vtable, addMethodSymbol, asStringSymbol, methodNew(stringAsString, 0));
    send(stringClass->vtable, addMethodSymbol, lengthSymbol, methodNew(stringLength, 0));
    
    // Object //
    send(objectClass->vtable, addMethodSymbol, asStringSymbol, methodNew(objectAsString, 0));
    
    // Symbol //
    send(symbolClass->vtable, addMethodSymbol, newSymbol, methodNew(symbolNew, 1));
    send(symbolClass->vtable, addMethodSymbol, lengthSymbol, methodNew(stringLength, 0));
    
    // Closure //
    send(closureClass->vtable, addMethodSymbol, callSymbol, methodNew(closureCallOne, 0));
    send(closureClass->vtable, addMethodSymbol, whileSymbol, methodNew(closureWhile, 1));
    
    // ByteArray //
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
