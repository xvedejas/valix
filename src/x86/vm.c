#include <vm.h>
#include <threading.h>
#include <mm.h>
#include <data.h>
#include <string.h>
#include <types.h>
#include <parser.h>
#include <stdarg.h>
#include <list.h>

// only functions beginning with "v" should be used by the VM.

// #new:[varible list]methods:[method dictionary] creates a new object with
// given variables/methods out of another object.
// #clone creates a new object out of another without assigning new variables or methods

Map *globalSymbolTable;

Object *globalScope;

Object *objectProto, *symbolProto, *closureProto, *scopeProto, *processProto,
       *byteArrayProto, *stringProto, *arrayProto;

Object *trueObject, *falseObject, *console;

Object *isNilSymbol, *doesNotUnderstandSymbol;

#define bindLoop() {\
    closure = (object->methods == NULL)? NULL : symbolMapGet(object->methods, symbol);\
    if (closure != NULL)\
        return closure;\
    object = object->proto;\
    if (unlikely(object == NULL)) {\
        return NULL;\
    }}

inline Object *bind(Object *target, Object *symbol)
{
    register Object *object = target;
    if (object == NULL)
        return (symbol == isNilSymbol) ? trueObject : NULL;
    Object *closure;
    
    while (true)
    {
        /* Some loop unwinding here; this is a pretty important function so
         * optimization is necessary. */
        bindLoop(); bindLoop(); bindLoop(); bindLoop(); bindLoop(); bindLoop();
    }
}

Object *objectNew()
{
    Object *object = malloc(sizeof(Object));
    object->proto = objectProto;
    return object;
}

Object *processNew()
{
    Object *process = malloc(sizeof(Object) + sizeof(Process));
    process->proto = processProto;
    process->process->scope = NULL;
    process->process->valueStack = stackNew();
    return process;
}

Object *scopeNew(Object *block, Object *containing)
{
    Object *self = malloc(sizeof(Object) + sizeof(Scope));
    self->proto = scopeProto;
    Scope *scope = self->scope;
    scope->block = block;
    scope->containing = containing;
    return self;
}

Object *closureNew(Object *scope)
{
    Object *closure = malloc(sizeof(Object) + sizeof(Closure));
    closure->proto = closureProto;
    closure->closure->scope = scope; // scope where closure was defined
    return closure;
}

Object *stringNew(Size len, String s)
{
    Object *string = malloc(
        sizeof(Object) + sizeof(StringData) + sizeof(char) * len);
    memcpy(string->string->string, s, len);
    string->proto = stringProto;
    string->string->len = len;
    return string;
}

Object *symbolNew(String string)
{
    Object *symbol;
    if ((symbol = mapGet(globalSymbolTable, string, stringKey)) != NULL)
        return symbol;
    symbol = malloc(sizeof(Object) + sizeof(String));
    symbol->proto = symbolProto;
    symbol->data[0] = string;
    mapSet(globalSymbolTable, string, symbol, stringKey);
    symbol->methods = NULL;
    return symbol;
}

Object *internalMethodNew(InternalFunction *function, Size argc)
{
    Object *closure = closureNew(NULL);
    closure->closure->type = internalClosure;
    closure->closure->argc = argc;
    closure->closure->function = function;
    return closure;
}

Object *arrayNew(Size s)
{
    Object *array = malloc(sizeof(Object) + sizeof(Array) + sizeof(Object*) * s);
    array->proto = arrayProto;
    array->array->size = s;
    return array;
}

void vDoesNotUnderstand(Object *process)
{
    panic("Object at %x does not understand symbol '%s'\n", arg(1), arg(0)->data[0]);
    push(NULL);
}

void vApply(Object *process);

void vPrint(Object *process)
{
    Object *method = bind(arg(0), symbolNew("asString"));
    stackDebug(process->process->valueStack);
    printf("%x", arg(0));
    printf("Method %x\n", method);
    push(method);
    vApply(process);
    printf("%s", pop()->string->string);
    push(NULL);
}

Object *lookupVar(Object *scope, Object *symbol)
{
    Object *value;
    do
        value = symbolMapGet(scope->scope->variables, symbol);
    while (value == NULL && (scope = scope->scope->containing) != NULL);
    return value;
}

void setVar(Object *scope, Object *symbol, Object *value)
{
    while (scope != NULL)
    {
        if (symbolMapSet(scope->scope->variables, symbol, value))
            return;
        else
            scope = scope->scope->containing;
    }
    panic("Variable %s not found in scope", symbol->data[0]);
}

void processPushScope(Object *process, Object *scope)
{
    printf("Push scope\n");
    scope->scope->parent = currentScope;
    currentScope = scope;
}

Object *processPopScope(Object *process)
{
    printf("Pop scope\n");
    Object *prevScope = currentScope;
    currentScope = currentScope->scope->parent;
    return prevScope;
}

/* arg(1) is the receiver of "block" if a method.
 * The return value is put on the process's value stack, supposedly. In the case
 * of a user-defined closure, we don't immediately get a return value, we change
 * the scope of execution by pushing a new scope to the process. */
void vApply(Object *process)
{
    Object *block = pop();
    assert(block != NULL, "Internal error; method does not exist");
    Closure *closure = block->closure;
    // if we are calling from an internal function, save this state
    bool fromInternal = currentClosure->closure->type == internalClosure;
    // create a new scope
    Object *scope = scopeNew(block, block->closure->scope);
    processPushScope(process, scope);
    switch (closure->type)
    {
        case internalClosure:
            closure->function(process);
            processPopScope(process);
        break;
        case userDefinedClosure:
        {
            currentScope->scope->IP = closure->bytecode;
            if (fromInternal)
            {
                printf("Saving state\n");
                if (setjmp(currentScope->scope->env))
                {
                    printf("Returning from saved state");
                    return;
                }
                else
                {
                    currentIP = currentClosure->closure->bytecode;
                    printf("re-entering main loop\n");
                    longjmp(process->process->mainLoop, true);
                    panic("catch");
                }
            }
        } break;
        default: panic("VM Error");
    }
}

#define translate(internedValue)\
    ({  Map *table = currentClosure->closure->translationTable;\
        assert(table != NULL, "VM Error");\
    mapGetVal(table, (void*)(Size)internedValue); })

void processSetBytecode(Object *process, u8 *bytecode)
{
    process->process->bytecode = bytecode;
}

void processMainLoop(Object *process)
{
    Size depth = 0;
    bool inScope = true;
    u8 *IP = NULL;
    Object *closure = closureNew(NULL);
    closure->closure->bytecode = process->process->bytecode;
    closure->closure->type = userDefinedClosure;
    Object *scope = scopeNew(closure, closure->closure->scope);
    scope->scope->containing = globalScope;
    
    scope->scope->IP = closure->closure->bytecode;
    currentScope = NULL;
    processPushScope(process, scope);
    currentClosure = closure;
    
    /* setup the translation table */
    IP = currentIP;
    Size symcount = *IP++;
    closure->closure->translationTable = mapNewSize(symcount);
    Map *table = closure->closure->translationTable;
    Size i = 0;
    for (; i < symcount; i++)
    {
        Object *symbol = symbolNew((String)IP);
        mapSetVal(table, i, symbol);
        IP += strlen((String)IP) + 1;
    }
    currentIP = IP;
    
    void setupVariables()
    {
        u8 *IP = currentIP;
        Size varcount = *IP++;
        Object *keys[varcount],
            *values[varcount];
        Size i;
        for (i = 0; i < varcount; i++)
        {
            keys[i] = translate(*IP++);
            values[i] = NULL;
        }
        symbolMapInit(currentScope->scope->variables,
            varcount, (void**)keys, (void**)values);
    }
    
    void setupArguments()
    {
        u8 *IP = currentIP + 1;
        Size argcount = *IP++;
        Size i = 0;
        for (; i < argcount; i++)
            setVar(currentScope, translate(*IP++), pop());
        currentIP += *currentIP + 2;
    }
    
    while (true)
    {
        setjmp(process->process->mainLoop);
        setupVariables();
        setupArguments();
        /* Begin looking at the block. First byte is the number of variables
         * used in the scope, followed by their interned values. The next
         * byte is the number of arguments. */
        inScope = true;
        while (inScope)
        {
            if (currentClosure->closure->type == internalClosure)
                longjmp(currentScope->scope->env, 1);
            printf("Bytecode %x at %x\n", *currentIP, currentIP);
            switch (*currentIP++)
            {
                case 0x81: // integer literal
                {
                    panic("Not implemented");
                    //Object *integer = integerClone(integerProto);
                    //integer->data = (Size)*IP++;
                    //stackPush(scope->scope->valueStack, integer);
                } break;
                case 0x82: // double literal
                {
                    panic("Not implemented");
                } break;
                case 0x83: // string literal
                {
                    Size len = strlen((String)currentIP);
                    Object *string = stringNew(len, (String)currentIP);
                    currentIP += len + 1;
                    push(string);
                } break;
                case 0x84: // character literal
                {
                    panic("Not implemented");
                } break;
                case 0x85: // symbol literal
                {
                    Object *newSymbol = translate(*currentIP++);
                    push(newSymbol);
                } break;
                case 0x86: // array literal
                {
                    Size size = (Size)*currentIP++;
                    Object *newArray = arrayNew(size);
                    while (size --> 0)
                        newArray->array->array[size] = pop();
                    push(newArray);
                } break;
                case 0x87: // block literal
                {
                    u8 *IP = currentIP;
                    Object *newClosure = closureNew(currentScope);
                    newClosure->closure->type = userDefinedClosure;
                    newClosure->closure->bytecode = IP;
                    newClosure->closure->translationTable =
                        currentClosure->closure->translationTable;
                    push(newClosure);
                    // increment IP until after this block
                    Size defdepth = 1;
                    while (defdepth > 0)
                    {
                        switch (*IP++)
                        {
                            case 0x87: // start block
                                defdepth++;
                            break;
                            case 0x8D: // end block
                                defdepth--;
                            break;
                        }
                    }
                    currentIP = IP;
                } break;
                case 0x88: // variable
                {
                    Size index = (Size)*currentIP++;
                    Object *symbol = translate(index);
                    Object *value = lookupVar(currentScope, symbol);
                    push(value);
                } break;
                case 0x89: // message
                {
                    Size argc = (Size)*currentIP++;
                    Object *message, *receiver, *method;
                    /* Get the message symbol from the stack */
                    message = pop();
                    printf("sending message %s argc %x\n", message->data[0], argc);
                    receiver = stackGet(process->process->valueStack, argc);
                    /* Find the corresponding method by using bind() */
                    method = bind(receiver, message);
                    /* if the method is apply, the receiver becomes the method */
                    if (unlikely(method->closure->function == &vApply))
                    {
                        method = receiver;
                    }
                    if (unlikely(method == NULL)) /* Bind failed, send DoesNotUnderstand */
                    {
                        while (argc --> 0)
                            stackPop(process->process->valueStack);
                        push(message);
                        method = bind(receiver, doesNotUnderstandSymbol);
                        assert(method != NULL, "VM Error; object at %x, proto %x %x does not delegate to Object %x",
                            receiver, receiver->proto, receiver->proto->proto, objectProto);
                        push(method);
                        vApply(process);
                        return; // exit due to error.
                    }
                    else if (method->closure->type == internalClosure)
                    {
                        push(method);
                        depth++;
                        vApply(process);
                        depth--;
                    }
                    else
                    {
                        /* Apply, using the arguments on the stack */
                        push(method);
                        vApply(process);
                        inScope = false;
                        depth++;
                    }
                } break;
                case 0x8A: // stop
                {
                    stackPop(process->process->valueStack);
                } break;
                case 0x8B: // set variable
                {
                    setVar(currentScope, translate(*currentIP++),
                        pop());
                } break;
                case 0x8C: // return-from
                {
                    panic("Not implemented");
                } break;
                case 0x8D: // end
                {
                    printf("? %x\n", currentClosure->closure->type == internalClosure);
                    printf("? %x\n", currentScope->scope->parent->scope->block->closure->type == internalClosure);
                    processPopScope(process);
                    if (depth == 0) // process done
                        return;
                    depth--;
                    printf("depth %x\n", depth);
                    printf("? %x\n", currentClosure->closure->type == internalClosure);
                } break;
                default:
                {
                    panic("Unknown Bytecode");
                }
            }
            if (!inScope) break;
        }
    }
}

void symbolAsString(Object *process)
{
    String s = pop()->data[0];
    push(stringNew(strlen(s), s));
}

void stringAsString(Object *process)
{
    /* Do nothing */
    return;
}

void arrayAsString(Object *process)
{
    Object *array = pop();
    StringBuilder *sb = stringBuilderNew("[");
    Size i;
    for (i = 0; i < array->array->size; i++)
    {
        Object *object = array->array->array[i];
        Object *method = bind(object, symbolNew("asString"));
        push(object);
        push(method);
        vApply(process);
        Object *string = pop();
        stringBuilderAppend(sb, string->string->string);
        if (i < array->array->size)
            stringBuilderAppend(sb, ", ");
    }
    stringBuilderAppend(sb, "]");
    Size size = sb->size;
    Object *string = stringNew(size, stringBuilderToString(sb));
    push(string);
}


/* This method creates a new user-defined object. The internal data of a
 * user-defined object is an array of scope objects, which each hold instance
 * variables either defined at the object's level, or at the level of one
 * of its super-objects (prototypes). */
void vObjectNewMethods(Object *process)
{
    /* Acquire arguments */
    Object *methods = pop();
    Object *methodNames = pop();
    Object *variables = pop();
    Object *variableNames = pop();
    Object *proto = pop();
    //Size newVarCount = variables->array->size;
    //Size newMethodCount = methods->array->size;
    
    /* Determine size of allocation */
    Size size = sizeof(Object);
    Object *current = proto;
    do
        size += sizeof(Object*);
    while ((current = current->proto) != NULL);
    size += sizeof(Object*);
    
    /* Allocate object */
    Object *self = malloc(size);
    self->object[0] = scopeNew(NULL, currentScope);
    Object *scope = self->object[0];
    
    /* Add new methods and variables */
    scope->scope->variables = symbolMapNew(variableNames->array->size,
        (void**)variableNames->array->array, (void**)variables->array->array);
    self->methods = symbolMapNew(methodNames->array->size,
        (void**)methodNames->array->array, (void**)methods->array->array);
    
    /* Allocate private super variables */
    Size superLevel;
    current = self->proto;
    for (superLevel = 1; current != NULL; current = current->proto, superLevel++)
    {
        self->object[superLevel] = scopeNew(NULL, scope);
        self->object[superLevel]->scope->variables =
            symbolMapCopy(current->object[0]->scope->variables);
    }
    
    push(self);
}

void bootstrap()
{
    globalSymbolTable = mapNew();
    
    /* Base object creation */
    
    objectProto = malloc(sizeof(Object));
    objectProto->proto = NULL;
    
    symbolProto = objectNew();
    
    closureProto = objectNew();
    
    /* Setting up ObjectProto */
    
    Size objectMethodCount = 3;
    Object *objectMethodSymbols[objectMethodCount];
    Object *objectMethods[objectMethodCount];
    
    objectMethodSymbols[0] = symbolNew("new:values:methods:def:");
    objectMethods[0] = internalMethodNew(vObjectNewMethods, 5);
    
    doesNotUnderstandSymbol = symbolNew("doesNotUnderstand:");
    objectMethodSymbols[1] = doesNotUnderstandSymbol;
    objectMethods[1] = internalMethodNew(vDoesNotUnderstand, 2);
    
    ///objectMethodSymbols[2] = isNilSymbol;
    ///objectMethods[2] = internalMethodNew(vReturnFalse, 1);
    
    objectProto->methods = symbolMapNew(objectMethodCount,
        (void**)objectMethodSymbols, (void**)objectMethods);
    
    /* Setting up SymbolProto */
    
    Size symbolMethodCount = 2;
    Object *symbolMethodSymbols[symbolMethodCount];
    Object *symbolMethods[symbolMethodCount];
    
    ///symbolMethodSymbols[0] = cloneSymbol;
    ///symbolMethods[0] = internalMethodNew(symbolClone, 1);
    
    symbolMethodSymbols[1] = symbolNew("asString");
    symbolMethods[1] = internalMethodNew(symbolAsString, 1);
    
    symbolProto->methods = symbolMapNew(symbolMethodCount,
        (void**)symbolMethodSymbols, (void**)symbolMethods);
    
    /* Setting up ClosureProto */
    
    Size closureMethodCount = 2;
    Object *closureMethodSymbols[closureMethodCount];
    Object *closureMethods[closureMethodCount];
    
    closureMethodSymbols[0] = symbolNew("apply");
    closureMethods[0] = internalMethodNew(vApply, 1);
    
    closureMethodSymbols[1] = symbolNew("apply:");
    closureMethods[1] = internalMethodNew(vApply, 2);
    
    closureProto->methods = symbolMapNew(closureMethodCount,
        (void**)closureMethodSymbols, (void**)closureMethods);
    
    /* ProcessProto */
    
    processProto = objectNew();
    
    /* Setting up ScopeProto */
    
    scopeProto = objectNew();
    
    /* Setting up stringProto */
    
    stringProto = objectNew();
    
    Object *stringMethodNames[] =
    {
        symbolNew("asString"),
    };
    Object *stringMethods[] =
    {
        internalMethodNew(stringAsString, 1),
    };
    
    stringProto->methods = symbolMapNew(1, (void**)stringMethodNames,
        (void**)stringMethods);
    
    /* Setting up arrayProto */
    
    arrayProto = objectNew();
    
    Object *arrayMethodNames[] =
    {
        symbolNew("asString"),
    };
    Object *arrayMethods[] =
    {
        internalMethodNew(arrayAsString, 1),
    };
    
    arrayProto->methods = symbolMapNew(1, (void**)arrayMethodNames,
        (void**)arrayMethods);
    
    /* Setting up console */
    
    console = objectNew();
    
    Object *consoleKeys[] =
    {
        symbolNew("print:"),
    };
    Object *consoleValues[] =
    {
        internalMethodNew(vPrint, 2),
    };
    
    console->methods = symbolMapNew(1, (void**)consoleKeys, (void**)consoleValues);
    
    /* Setting up Global Scope */
    
    globalScope = scopeNew(NULL, NULL);
    globalScope->scope->containing = NULL;
    printf("global scope at %x\n", globalScope);
    
    Size globalVarCount = 2;
    Object *globalKeys[] =
    {
        symbolNew("Object"),
        symbolNew("Console"),
    };
    
    Object *globalValues[] =
    {
        objectProto,
        console,
    };
    
    symbolMapInit(globalScope->scope->variables, globalVarCount,
        (void**)globalKeys, (void**)globalValues);
}

void vmInstall()
{
    bootstrap();
}
