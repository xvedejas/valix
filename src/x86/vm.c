#include <vm.h>
#include <threading.h>
#include <mm.h>
#include <data.h>
#include <string.h>
#include <types.h>
#include <parser.h>
#include <stdarg.h>
#include <list.h>

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

#define runtimeError(message, args...) { printf("Runtime Error, File %s Line %i\n", __FILE__, __LINE__); printf(message, ## args); endThread(); }

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

Object *symbolNew(Object *self, String string)
{
    void *value = mapGet(globalSymbolTable, string, stringKey);
    if (value != NULL)
        return value;
    Object *newSymbol = new(self);
    newSymbol->data = strdup(string);
    mapSet(globalSymbolTable, string, newSymbol, stringKey);
    return newSymbol;
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
    Object *retval = NULL;
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
                retval = func(self);
            }
            else
                retval = interpret(newScope);
        break;
        case 1:
            one = va_arg(argptr, Object*);
            if (method->type == internalFunction)
            {
                Object *(*func)(Object*,Object*) = method->funcptr;
                retval = func(self, one);
            }
            else
                retval = interpret(newScope, one);
        break;
        case 2:
            one = va_arg(argptr, Object*);
            two = va_arg(argptr, Object*);
            if (method->type == internalFunction)
            {
                Object *(*func)(Object*,Object*,Object*) = method->funcptr;
                retval = func(self, one, two);
            }
            else
                retval = interpret(newScope, one, two);
        break;
        case 3:
            one = va_arg(argptr, Object*);
            two = va_arg(argptr, Object*);
            three = va_arg(argptr, Object*);
            if (method->type == internalFunction)
            {
                Object *(*func)(Object*,Object*,Object*,Object*) = method->funcptr;
                retval = func(self, one, two, three);
            }
            else
                retval = interpret(newScope, one, two, three);
        break;
        case 4:
            one = va_arg(argptr, Object*);
            two = va_arg(argptr, Object*);
            three = va_arg(argptr, Object*);
            four = va_arg(argptr, Object*);
            if (method->type == internalFunction)
            {
                Object *(*func)(Object*,Object*,Object*,Object*,Object*) = method->funcptr;
                retval = func(self, one, two, three, four);
            }
            else
                retval = interpret(newScope, one, two, three, four);
        break;
    }
    va_end(argptr);
    return retval;
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
    runtimeError("%s at %x does not understand message \"%s\" %x",
        send(self, typeSymbol)->data, self, symbol->data, symbol);
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
    lengthSymbol            = symbolNew(symbolClass, "length");
    newSymbol               = symbolNew(symbolClass, "new:");
    lookupSymbol            = symbolNew(symbolClass, "lookup:");
    addMethodSymbol         = symbolNew(symbolClass, "addMethod:def:");
    allocateSymbol          = symbolNew(symbolClass, "allocate");
    delegatedSymbol         = symbolNew(symbolClass, "delegated");
    newZeroSymbol           = symbolNew(symbolClass, "new");
    doesNotUnderstandSymbol = symbolNew(symbolClass, "doesNotUnderstand:");
    printSymbol             = symbolNew(symbolClass, "print:");
    asStringSymbol          = symbolNew(symbolClass, "asString");
    executeSymbol           = symbolNew(symbolClass, "execute:");
    applySymbol             = symbolNew(symbolClass, "apply");
    applyOneSymbol          = symbolNew(symbolClass, "apply:");
    applyTwoSymbol          = symbolNew(symbolClass, "apply:and:");
    equivSymbol             = symbolNew(symbolClass, "==");
    lessThanSymbol          = symbolNew(symbolClass, "<");
    greaterThanSymbol       = symbolNew(symbolClass, ">");
    selfSymbol              = symbolNew(symbolClass, "self");
    typeSymbol              = symbolNew(symbolClass, "type");
    whileSymbol             = symbolNew(symbolClass, "whileTrue:");
    addSymbol               = symbolNew(symbolClass, "add:");
    atSymbol                = symbolNew(symbolClass, "at:");
    atPutSymbol             = symbolNew(symbolClass, "at:put:");
    popSymbol               = symbolNew(symbolClass, "pop:");
    atInsertSymbol          = symbolNew(symbolClass, "at:insert:");
    doSymbol                = symbolNew(symbolClass, "do:");
    toSymbol                = symbolNew(symbolClass, "to:");
    toDoSymbol              = symbolNew(symbolClass, "to:do:");
    trueSymbol              = symbolNew(symbolClass, "true");
    falseSymbol             = symbolNew(symbolClass, "false");
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

    send(symbolClass->vtable, addMethodSymbol, newSymbol, methodNew(symbolNew, 1));

    send(vtablevt, addMethodSymbol, delegatedSymbol, methodNew(vtableDelegated, 0));

    Object *subclassSymbol = send(symbolClass, newSymbol, "subclass");
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

Object *modulus(Object *self, Object *operand)
{
    return numberNew(numberClass, ((Size)self->data) % ((Size)operand->data));
}

Object *numberTo(Object *self, Object *end)
{
    Object *list = listNewSize(listClass, end->data - self->data);
    Size i;
    for (i = (Size)self->data; i < (Size)end->data; i++)
        listAdd(list, numberNew(numberClass, i));
    return list;
}

Object *numberToDo(Object *self, Object *end, Object *closure)
{
    Object *value = NULL;
    Size i;
    for (i = (Size)self->data; i < (Size)end->data; i++)
        value = send(closure, applyOneSymbol, numberNew(numberClass, i));
    return value;
}

Object *print(Object *console, Object *argument)
{
    if (argument == NULL) return NULL;
    Object *string = send(argument, asStringSymbol);
    printf("%s\n", string->data);
    return NULL;
}

Object *closureCallZero(Object *closure)
{
    return doMethod(NULL, closure);
}

Object *closureCallOne(Object *closure, Object *arg)
{
    return doMethod(NULL, closure, arg);
}

Object *closureCallTwo(Object *closure, Object *one, Object *two)
{
    return doMethod(NULL, closure, one, two);
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
        value = send(closure, applySymbol);
    return value;
}

Object *boolIfFalse(Object *self, Object *closure)
{
    Object *value = NULL;
    if (!self->data)
        value = send(closure, applySymbol);
    return value;
}

Object *boolIfTrueFalse(Object *self, Object *closureOne, Object *closureTwo)
{
    Object *value = NULL;
    if (self->data)
        value = send(closureOne, applySymbol);
    else
        value = send(closureTwo, applySymbol);
    return value;
}

Object *boolAsString(Object *self)
{
    if (self->data)
        return stringNew(stringClass, "true");
    return stringNew(stringClass, "false");
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
    while (send(self, applySymbol)->data)
        value = send(closure, applySymbol);
    return value;
}

Object *listDo(Object *self, Object *closure)
{
    Size i;
    Object *value;
    for (i = 0; i < self->list->items; i++)
    {
        value = send(closure, applyOneSymbol, self->list->array[i]);
    }
    return value;
}

Object *arrayNew(Object *self, Object **payload)
{
    Object *array = new(self);
    array->data = payload;
    return array;
}

Object *arrayAt(Object *self, Object *index)
{
    return ((Object**)self->data)[((Size)index->data)];
}

Object *arrayAtPut(Object *self, Object *index, Object *value)
{
    ((Object**)self->data)[((Size)index->data)] = value;
    return NULL;
}

Object *numberType(Object *self)
{
    return stringNew(stringClass, "Number");
}

Object *objectType(Object *self)
{
    return stringNew(stringClass, "Object");
}

Object *symbolType(Object *self)
{
    return stringNew(stringClass, "Symbol");
}

Object *closureType(Object *self)
{
    return stringNew(stringClass, "Closure");
}

Object *listType(Object *self)
{
    return stringNew(stringClass, "List");
}

Object *translate(Object *closure, Size internedValue)
{
    Map *table = closure->method->translationTable;
    Object *value = mapGetVal(table, internedValue);
    return value;
}

/* This returns a process object. Send it #execute: with a bytecode
 * argument to run the process. You can repeatedly send it #execute:
 * in fact, and it will run through each one sequentially. Processes
 * do not necessarily operate in their own thread. */
Object *processNew(Object *self)
{
    Object *process = new(processClass);
    /* 5 is an arbitrary guess at how many process-global variables there could be */
    process->data = scopeNew(globalScope, NULL, 5);
    return process;
}

Object *processExit(Object *self)
{
   /// todo
   return NULL; 
}

Object *lookupVar(Object *scope, Object *symbol)
{
    Object *value;
    do
        value = mapGetVal(scope->scope->vars, symbol->data);
    while (value == NULL && (scope = scope->scope->parent) != NULL);
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
            mapGetVal(scope->scope->vars, symbol->data);
        if (currentValue != NULL)
        {
            mapSetVal(scope->scope->vars, symbol->data, value);
            return;
        }
    } while ((scope = scope->scope->parent) != NULL);
    mapSetVal(currentScope->scope->vars, symbol->data, value);
}

/* Given the scope, interpret the bytecode in its closure */
Object *interpret(Object *scope, ...)
{
    va_list argptr;
    va_start(argptr, scope);
    Object *closure = scope->scope->closure;
    Method *closurePayload = closure->method;
    u8 *IP = closurePayload->bytearray->data;
    
    /* Value stack */
    Stack *vStack = stackNew();
    
    // Load arguments
    
    Size argc = closurePayload->argc;
    int i;
    for (i = 0; i < argc; i++)
        stackPush(vStack, va_arg(argptr, Object*));
    va_end(argptr);
    
    while (true)
    {
        //printf("%s %x %c\n", bytecodes[*IP], *IP, *IP); // print the byte we're looking at
        switch (*IP++)
        {
            case stringBC:
            {
                Size len = strlen((String)IP);
                Object *string = send(stringClass, newSymbol, (String)IP);
                IP += len + 1;
                stackPush(vStack, string);
            } break;
            case integerBC:
            {
                Size len = strlen((String)IP);
                Size val = strtoul((String)IP, NULL, 10);
                Object *number = send(numberClass, newSymbol, val);
                IP += len + 1;
                stackPush(vStack, number);
            } break;
            case doubleBC:
            {
                Size len = strlen((String)IP);
                Size val = strtod((String)IP, NULL);
                Object *number = send(numberClass, newSymbol, val);
                IP += len + 1;
                stackPush(vStack, number);
            } break;
            case charBC:
            {
                panic("Not implemented");
            } break;
            case symbolBC:
            {
                stackPush(vStack, translate(closure, (Size)*IP));
                IP++;
            } break;
            case arrayBC:
            {
                Size length = *IP++;
                Object **arrayPayload = malloc(sizeof(Object*) * length);
                Size i;
                for (i = 0; i < length; i++)
                    arrayPayload[i] = stackPop(vStack);
                Object *array = arrayNew(arrayClass, arrayPayload);
                stackPush(vStack, array);
            } break;
            case blockBC:
            {
                argc = *IP++;
                Object *newClosure = closureNew(closureClass, bytearrayNew(bytearrayClass, IP), scope);
                newClosure->method->argc = argc;
                newClosure->method->type = userDefinedClosure;
                newClosure->method->translationTable = scope->scope->closure->method->translationTable;
                /* Now we increment IP until we are passed the block, making
                 * sure that we don't count the ends of any inner blocks as
                 * the end of the main block. */
                Size blocks = 1; // The number of beginProcedures we encounter
                while (blocks > 0)
                {
                    if (*IP == blockBC)
                        blocks++;
                    if (*IP == endBC)
                        blocks--;
                    IP++;
                }
                stackPush(vStack, newClosure);
            } break;
            case variableBC:
            {
                Size index = (Size)*IP++;
                Object *symbol = translate(closure, index);
                stackPush(vStack, lookupVar(scope, symbol));
            } break;
            case messageBC:
            {
                Size argc = (Size)*IP++;
                Object *methodSymbol = stackPop(vStack);
                /* The arguments are at the top of the stack, and
                 * message receiver directly after that. */
                Object *args[8]; // argc limit is 8
                Size i;
                for (i = 0; i < argc; i++)
                {
                    args[i] = stackPop(vStack);
                    if (args[i] == NULL)
                        runtimeError("Null argument");
                }
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
                stackPush(vStack, ret);
            } break;
            case stopBC:
            {
                // ?
            } break;
            /* "return" is like lisp's "return-from", not like expected
             * from most other languages. */
            case returnBC:
            {
                runtimeError("Not implemented!");
            } break;
            case setBC:
            {
                Size index = (Size)*IP++;
                Object *symbol = translate(closure, index);
                setVar(scope, symbol, stackPop(vStack));
            } break;
            case endBC:
            {
                if (vStack->entries)
                    return stackPop(vStack);
                else
                    return NULL;
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
Object *processExecute(Object *self, u8 *bytecode)
{
    /* The first section of the code is the method interned header.
     * We look at each entry, and create a dictionary (using the global
     * method table) which has keys that equal the local interned value,
     * and values that are the global interned value. */
    
    Size entries = (Size)*bytecode++;
    Map *internTranslationTable = mapNewSize(entries + 1);
    Size i;
    for (i = 0; i < entries; i++)
    {
        Object *globalSymbol = symbolNew(symbolClass, (String)bytecode);
        mapSet(internTranslationTable, (void*)i, globalSymbol, valueKey);
        while (*bytecode++);
    }

    /* Setup the process scope */
    Object *processScope = self->data;
    Scope *scopePayload = processScope->data;
    Object *bytearray = send(bytearrayClass, newSymbol, bytecode);
    
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
    Object *ifTrueSymbol = send(symbolClass, newSymbol, "ifTrue:");
    Object *ifFalseSymbol = send(symbolClass, newSymbol, "ifFalse:");
    Object *ifTrueFalseSymbol = send(symbolClass, newSymbol, "ifTrue:ifFalse:");
    send(boolClass->vtable, addMethodSymbol, ifTrueSymbol, methodNew(boolIfTrue, 1));
    send(boolClass->vtable, addMethodSymbol, ifFalseSymbol, methodNew(boolIfFalse, 1));
    send(boolClass->vtable, addMethodSymbol, ifTrueFalseSymbol, methodNew(boolIfTrueFalse, 2));
    send(boolClass->vtable, addMethodSymbol, asStringSymbol, methodNew(boolAsString, 0));
    trueSingleton = boolNew(boolClass, 1);
    falseSingleton = boolNew(boolClass, 0);
    setVar(globalScope, trueSymbol, trueSingleton);
    setVar(globalScope, falseSymbol, falseSingleton);
    
    // Number //
    numberClass = subclass(objectClass);
    send(numberClass->vtable, addMethodSymbol, newSymbol, methodNew(numberNew, 1));
    send(numberClass->vtable, addMethodSymbol, asStringSymbol, methodNew(numberAsString, 0));
    Object *multiplySymbol = send(symbolClass, newSymbol, "*");
    Object *additionSymbol = send(symbolClass, newSymbol, "+");
    Object *divideSymbol = send(symbolClass, newSymbol, "/");
    Object *subtractSymbol = send(symbolClass, newSymbol, "-");
    Object *modulusSymbol = send(symbolClass, newSymbol, "%");
    send(numberClass->vtable, addMethodSymbol, multiplySymbol, methodNew(multiply, 1));
    send(numberClass->vtable, addMethodSymbol, additionSymbol, methodNew(add, 1));
    send(numberClass->vtable, addMethodSymbol, divideSymbol, methodNew(divide, 1));
    send(numberClass->vtable, addMethodSymbol, subtractSymbol, methodNew(subtract, 1));
    send(numberClass->vtable, addMethodSymbol, modulusSymbol, methodNew(modulus, 1));
    send(numberClass->vtable, addMethodSymbol, equivSymbol, methodNew(numberEquiv, 1));
    send(numberClass->vtable, addMethodSymbol, greaterThanSymbol, methodNew(numberGreaterThan, 1));
    send(numberClass->vtable, addMethodSymbol, lessThanSymbol, methodNew(numberLessThan, 1));
    send(numberClass->vtable, addMethodSymbol, typeSymbol, methodNew(numberType, 0));
    send(numberClass->vtable, addMethodSymbol, toSymbol, methodNew(numberTo, 1));
    send(numberClass->vtable, addMethodSymbol, toDoSymbol, methodNew(numberToDo, 2));
    
    // String //
    stringClass = subclass(objectClass);
    send(stringClass->vtable, addMethodSymbol, newSymbol, methodNew(stringNew, 1));
    send(stringClass->vtable, addMethodSymbol, asStringSymbol, methodNew(stringAsString, 0));
    send(stringClass->vtable, addMethodSymbol, lengthSymbol, methodNew(stringLength, 0));
    
    // Object //
    send(objectClass->vtable, addMethodSymbol, asStringSymbol, methodNew(objectAsString, 0));
    send(objectClass->vtable, addMethodSymbol, typeSymbol, methodNew(objectType, 0));
    
    // Symbol //
    send(symbolClass->vtable, addMethodSymbol, newSymbol, methodNew(symbolNew, 1));
    send(symbolClass->vtable, addMethodSymbol, lengthSymbol, methodNew(stringLength, 0));
    send(symbolClass->vtable, addMethodSymbol, typeSymbol, methodNew(symbolType, 0));
    
    // Closure //
    send(closureClass->vtable, addMethodSymbol, applySymbol, methodNew(closureCallZero, 0));
    send(closureClass->vtable, addMethodSymbol, applyOneSymbol, methodNew(closureCallOne, 1));
    send(closureClass->vtable, addMethodSymbol, applyTwoSymbol, methodNew(closureCallTwo, 2));
    send(closureClass->vtable, addMethodSymbol, whileSymbol, methodNew(closureWhile, 1));
    send(closureClass->vtable, addMethodSymbol, typeSymbol, methodNew(closureType, 0));
    
    // ByteArray //
    bytearrayClass = subclass(objectClass);
    send(bytearrayClass->vtable, addMethodSymbol, newSymbol, methodNew(bytearrayNew, 1));
    
    // Array //
    arrayClass = subclass(objectClass);
    send(arrayClass->vtable, addMethodSymbol, newSymbol, methodNew(arrayNew, 1));
    send(arrayClass->vtable, addMethodSymbol, atSymbol, methodNew(arrayAt, 1));
    send(arrayClass->vtable, addMethodSymbol, atPutSymbol, methodNew(arrayAtPut, 2));
    
    // List //
    listClass = subclass(objectClass);
    send(listClass->vtable, addMethodSymbol, newZeroSymbol, methodNew(listNew, 0));
    send(listClass->vtable, addMethodSymbol, asStringSymbol, methodNew(listAsString, 0));
    send(listClass->vtable, addMethodSymbol, typeSymbol, methodNew(listType, 0));
    send(listClass->vtable, addMethodSymbol, addSymbol, methodNew(listAdd, 1));
    send(listClass->vtable, addMethodSymbol, atSymbol, methodNew(listAt, 1));
    send(listClass->vtable, addMethodSymbol, atPutSymbol, methodNew(listAtPut, 2));
    send(listClass->vtable, addMethodSymbol, popSymbol, methodNew(listPop, 1));
    send(listClass->vtable, addMethodSymbol, atInsertSymbol, methodNew(listAtInsert, 2));
    send(listClass->vtable, addMethodSymbol, doSymbol, methodNew(listDo, 1));
}

void vmInstall()
{
    bootstrap();
    /* After bootstrap, send() may now be used */
    globalScope = scopeNew(NULL, NULL, 20 /*globalVarCount*/);
    initialize();
    /// setup the global scope
    setVar(globalScope, symbolNew(symbolClass, "Console"), consoleClass);
    setVar(globalScope, symbolNew(symbolClass, "Array"), arrayClass);
    setVar(globalScope, symbolNew(symbolClass, "List"), listClass);
}
