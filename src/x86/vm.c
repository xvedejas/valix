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
       *byteArrayProto, *stringProto;

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

Object *scopeNew()
{
    Object *scope = malloc(sizeof(Object) + sizeof(Scope));
    scope->proto = scopeProto;
    return scope;
}

Object *closureNew()
{
    Object *closure = malloc(sizeof(Object) + sizeof(Closure));
    closure->proto = closureProto;
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
    Object *closure = closureNew();
    closure->closure->type = internalClosure;
    closure->closure->argc = argc;
    closure->closure->function = function;
    return closure;
}

void vDoesNotUnderstand(Object *process)
{
    panic("Object at %x does not understand symbol '%s'\n", arg(1), arg(0)->data[0]);
    push(NULL);
}

void vApply(Object *process);

void vPrint(Object *process)
{
    push(bind(arg(0), symbolNew("asString")));
    printf("A\n");
    vApply(process); // in vApply, we should jump back into the main loop
    printf("B\n");
    printf("%s", pop()->string->string);
    push(NULL);
}

Object *lookupVar(Object *scope, Object *symbol)
{
    Object *value;
    do
        value = symbolMapGet(scope->scope->variables, symbol);
    while (value == NULL && (scope = scope->scope->parent) != NULL);
    return value;
}

void setVar(Object *scope, Object *symbol, Object *value)
{
    while (scope != NULL)
    {
        if (symbolMapSet(scope->scope->variables, symbol, value))
            return;
        else
            scope = scope->scope->parent;
    }
    panic("Variable not found in scope");
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
    Closure *closure = block->closure;
    // if we are calling from an internal function, save this state
    bool fromInternal = currentClosure->closure->type == internalClosure;
    // create a new scope
    Object *scope = scopeNew();
    scope->scope->block = block;
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
                    printf("re-entering main loop\n");
                    longjmp(process->process->mainLoop, true);
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
    Object *closure = closureNew();
    closure->closure->bytecode = process->process->bytecode;
    Object *scope = scopeNew();
    
    scope->scope->IP = closure->closure->bytecode;
    currentScope = globalScope;
    processPushScope(process, scope);
    currentClosure = closure;
    
    /* setup the translation table */
    u8 *IP = currentIP;
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
        currentScope->scope->variables =
            symbolMapNew(varcount, (void**)keys, (void**)values);
        currentIP = IP;
    }
    
    void setupArguments()
    {
        u8 *IP = currentIP;
        Size argcount = *IP++;
        Size i = 0;
        for (; i < argcount; i++)
        {
            /// todo currentClosure
            IP++;
        }
        currentIP = IP;
    }
    
    while (true)
    {
        if (currentClosure->closure->type == internalClosure)
            longjmp(currentScope->scope->env, 1);
        setupVariables();
        setupArguments();
        /* Begin looking at the block. First byte is the number of variables
         * used in the scope, followed by their interned values. The next
         * byte is the number of arguments. */
        inScope = true;
        while (inScope)
        {
            setjmp(process->process->mainLoop);
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
                    panic("Not implemented");
                } break;
                case 0x87: // block literal
                {
                    u8 *IP = currentIP;
                    Object *newClosure = closureNew();
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
                    receiver = stackGet(process->process->valueStack, argc);
                    /* Find the corresponding method by using bind() */
                    method = bind(receiver, message);
                    /* if the method is apply, the receiver becomes the method */
                    if (method->closure->function == vApply)
                    {
                        method = receiver;
                    }
                    if (method == NULL) /* Bind failed, send DoesNotUnderstand */
                    {
                        while (argc --> 0)
                            stackPop(process->process->valueStack);
                        push(message);
                        method = bind(receiver, doesNotUnderstandSymbol);
                        assert(method != NULL, "VM Error");
                        push(method);
                        vApply(process);
                        return; // exit due to error.
                    }
                    else if (method->closure->type == internalClosure)
                    {
                        push(method);
                        vApply(process);
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
                    processPopScope(process);
                    if (depth == 0) // process done
                        return;
                    depth--;
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

void vObjectNewMethods(Object *process)
{
    
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
    
    ///objectMethodSymbols[0] = newMethodsSymbol;
    ///objectMethods[0] = internalMethodNew(vObjectNewMethods, 1);
    
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
    
    ///closureMethodSymbols[0] = cloneSymbol;
    ///closureMethods[0] = internalMethodNew(closureClone, 1);
    
    closureMethodSymbols[1] = symbolNew("apply");
    closureMethods[1] = internalMethodNew(vApply, 1);
    
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
    
    globalScope = scopeNew();
    
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
    
    globalScope->scope->variables = symbolMapNew(globalVarCount,
        (void**)globalKeys, (void**)globalValues);
}

void vmInstall()
{
    bootstrap();
}
