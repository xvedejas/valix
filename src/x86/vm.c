/*  Copyright (C) 2011 Xander Vedejas <xvedejas@gmail.com>
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
#include <threading.h>
#include <mm.h>
#include <data.h>
#include <string.h>
#include <types.h>
#include <parser.h>
#include <stdarg.h>
#include <list.h>
#include <number.h>

Map *globalSymbolTable;

Object *globalScope;

Object *trueObject, *falseObject, *console;

Object *isNilSymbol, *doesNotUnderstandSymbol, *thisWorldSymbol;

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
        bindLoop(); bindLoop(); bindLoop(); bindLoop(); bindLoop(); bindLoop();
    }
}

Object *objectNew()
{
    Object *object = malloc(sizeof(Object));
    object->proto = objectProto;
    object->object = NULL;
    return object;
}

Object *scopeNew(Object *block, Object *containing)
{
    Object *self = malloc(sizeof(Object) + sizeof(Scope));
    self->proto = scopeProto;
    Scope *scope = self->scope;
    scope->block = block;
    scope->thisWorld = NULL;
    if (containing == NULL)
        scope->containing = globalScope;
    else
        scope->containing = containing;
    
    return self;
}

Object *closureNew(Object *scope)
{
    Object *closure = malloc(sizeof(Object) + sizeof(Closure));
    closure->proto = closureProto;
    closure->closure->scope = scope; // scope where closure was defined
    closure->closure->isMethod = false;
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
    Object *method = closureNew(NULL);
    Closure *closure = method->closure;
    closure->type = internalClosure;
    closure->isMethod = true;
    closure->argc = argc;
    closure->function = function;
    return method;
}

Object *arrayNew(Size s)
{
    Object *array = malloc(sizeof(Object) + sizeof(Array) + sizeof(Object*) * s);
    array->proto = arrayProto;
    array->array->size = s;
    return array;
}

Object *integerNew(u32 value)
{
    Object *object = malloc(sizeof(Object) + sizeof(Number));
    object->proto = integerProto;
    object->object = NULL;
    object->number->type = integer;
    object->number->data[0] = value;
    return object;
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
    push(method);
    vApply(process);
    printf("%s", pop()->string->string);
}

/* This following two functions, lookupVar and setVar, first try to find the
 * variable in the current scope and world. If not found, they search the
 * current scope but with a parent world until all parent worlds are exhausted.
 * Then they look at the containing scope with the current world, and then
 * parent worlds, etc. */
Object *lookupVar(Object *scope, Object *symbol)
{
    if (symbol == thisWorldSymbol)
        return scope->scope->thisWorld;
    ///printf("looking up %s\n", symbol->data[0]);
    Object *value = NULL;
    Map *values;
    Object *startingWorld = scope->scope->thisWorld;
    Object *world = startingWorld;
    
    while (value == NULL)
    {
        while ((values = symbolMapGet(scope->scope->variables, symbol)) == NULL)
        {
            if ((scope = scope->scope->containing) == NULL)
            {
                ///printf("not found -- returning NULL\n");
                return NULL;
            }
        }
        while ((value = mapGetVal(values, world)) == NULL)
        {
            if ((world = world->world->parent) == NULL)
            {
                world = startingWorld;
                scope = scope->scope->containing;
                break;
            }
        }
    }
    
    /* Here we've found our value. If this was found in a world that
     * isn't the current world, then we must add the entry for the
     * current world because we read the value. */
    if (world != startingWorld)
        mapSetVal(values, startingWorld, value);
    return value;
}

void setVar(Object *scope, Object *symbol, Object *newValue)
{
    ///printf("for setting, looking up %s\n", symbol->data[0]);
    Map *values;
    Object *startingWorld = scope->scope->thisWorld;
    Object *world = startingWorld;
    
    while ((values = symbolMapGet(scope->scope->variables, symbol)) == NULL)
    {
        if ((scope = scope->scope->containing) == NULL)
        {
            panic("variable not found! cannot set.\n");
            return;
        }
    }
    while (!mapHasVal(values, world))
    {
        if ((world = world->world->parent) == NULL)
        {
            world = startingWorld;
            scope = scope->scope->containing;
            break;
        }
    }
    mapSetVal(values, startingWorld, newValue);
}

/* Note that what world we are in is determined by which scope calls the current
 * scope, not which scope defines the current scope. It is the exact opposite
 * when dealing with variable delegation. */
void processPushScope(Object *process, Object *scope)
{
    scope->scope->parent = currentScope;
    if (scope->scope->thisWorld == NULL)
        scope->scope->thisWorld = process->process->world;
    currentScope = scope;
}

Object *processPopScope(Object *process)
{
    Object *prevScope = currentScope;
    currentScope = currentScope->scope->parent;
    process->process->world = currentScope->scope->thisWorld;
    return prevScope;
}

/* The return value is put on the process's value stack, supposedly. In the case
 * of a user-defined closure, we don't immediately get a return value, we change
 * the scope of execution by pushing a new scope to the process. */
void vApply(Object *process)
{
    process->process->depth++;
    Object *block = pop();
    assert(block != NULL, "Internal error; method does not exist");
    Closure *closure = block->closure;
    // if we are calling from an internal function, save this state
    bool fromInternal = currentClosure->closure->type == internalClosure;
    
    /* Here we see if we must correct for the scope of a method when the method
     * is defined for a prototype instead of the given object */
    
    Object *scope;
    if (closure->isMethod && closure->type == userDefinedClosure)
    {
        Object *methodScope = closure->scope;
        Object *self = arg(closure->argc - 1);
        Size i = 0;
        Object *current = self;
        for (; current->object[0] != methodScope; i++)
        {
            if ((current = current->proto) == NULL)
                panic("Method parent scope not found");
        }
        scope = scopeNew(block, self->object[i]);
    }
    else
    {
        // create a new scope
        scope = scopeNew(block, block->closure->scope);
    }
    processPushScope(process, scope);
    switch (closure->type)
    {
        case internalClosure:
        {
            closure->function(process);
            process->process->depth--;
            processPopScope(process);
        }
        return;
        case userDefinedClosure:
        {
            currentScope->scope->IP = closure->bytecode;
            if (fromInternal)
                processMainLoop(process);
        } break;
        default: panic("VM Error");
    }
}

// returns symbol
#define translate(internedValue)\
    ((Object*)({  Map *table = currentClosure->closure->translationTable;\
        assert(table != NULL, "VM Error");\
    mapGetVal(table, (void*)(Size)internedValue); }))

Object *processNew()
{
    Object *process = malloc(sizeof(Object) + sizeof(Process));
    process->proto = processProto;
    process->process->scope = NULL;
    process->process->valueStack = stackNew();
    
    process->process->depth = 0;
    Object *closure = closureNew(NULL);
    closure->closure->type = userDefinedClosure;
    Object *scope = scopeNew(closure, globalScope);
    process->process->world = globalWorld;
    
    currentScope = globalScope;
    processPushScope(process, scope);
    currentClosure = closure;
    
    return process;
}

/* The following function sets a bytecode for the process and generates the
 * corresponding transation table */
void processSetBytecode(Object *process, u8 *bytecode)
{
    currentClosure->closure->bytecode = bytecode;
    currentIP = bytecode;
    
    /* setup the translation table */
    u8 *IP = currentIP;
    Size symcount = *IP++;
    Map *table = mapNewSize(symcount);
    currentClosure->closure->translationTable = table;
    Size i = 0;
    for (; i < symcount; i++)
    {
        Object *symbol = symbolNew((String)IP);
        mapSetVal(table, i, symbol);
        IP += strlen((String)IP) + 1;
    }
    currentIP = IP;
    
    process->process->depth = 1;
}

/* This function is called by processMainLoop to create a block object from
 * bytecode generated whenever a block literal is encountered in the source. */
void blockLiteral(Object *process)
{
    u8 *IP = currentIP;
    Object *newClosure = closureNew(currentScope);
    newClosure->closure->type = userDefinedClosure;
    newClosure->closure->bytecode = IP;
    newClosure->closure->argc = IP[1];
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
}

/* This function is called by processMainLoop at the beginning of executing a
 * closure, to account for the header which tells info about the variables and
 * arguments available to the closure */
void closureSetup(Object *process)
{
    /* setup variables */
    u8 *IP = currentIP;
    Size varcount = *IP++;
    IP += 1; // skip over arg count
    Object *keys[varcount];
    Map *values[varcount];
    Size i;
    for (i = 0; i < varcount; i++)
    {
        keys[i] = translate(*IP++);
        values[i] = mapNew();
        mapSetVal(values[i], currentScope->scope->thisWorld, NULL);
    }
    currentScope->scope->variables = symbolMapNew(varcount, (void**)keys,
        (void**)values);
        
    /* setup arguments */
    IP = currentIP + 1;
    Size argcount = *IP;
    IP += argcount;
    i = 0;
    for (; i < argcount; i++, IP--)
        setVar(currentScope, translate(*IP), pop());
    currentIP += *currentIP + 2;
}

void processMainLoop(Object *process)
{
    closureSetup(process);
    
    while (true)
    {
        ///printf("Bytecode %x at %x\n", *currentIP, currentIP);
        switch (*currentIP++)
        {
            case integerBC: // integer literal
            {
                Size value = strtoul((String)currentIP, NULL, 10);
                currentIP += strlen((String)currentIP) + 1;
                push(integerNew(value));
            } break;
            case doubleBC: // double literal
            {
                panic("Not implemented");
            } break;
            case stringBC: // string literal
            {
                Size len = strlen((String)currentIP);
                Object *string = stringNew(len, (String)currentIP);
                currentIP += len + 1;
                push(string);
            } break;
            case charBC: // character literal
            {
                panic("Not implemented");
            } break;
            case symbolBC: // symbol literal
            {
                Object *newSymbol = translate(*currentIP++);
                push(newSymbol);
            } break;
            case arrayBC: // array literal
            {
                Size size = (Size)*currentIP++;
                Object *newArray = arrayNew(size);
                while (size --> 0)
                    newArray->array->array[size] = pop();
                push(newArray);
            } break;
            case blockBC: // block literal
            {
                blockLiteral(process);
            } break;
            case variableBC: // variable
            {
                Size index = (Size)*currentIP++;
                Object *symbol = translate(index);
                Object *value = lookupVar(currentScope, symbol);
                push(value);
            } break;
            case messageBC: // message
            {
                Size argc = (Size)*currentIP++;
                Object *message, *receiver, *method;
                /* Get the message symbol from the stack */
                message = pop();
                receiver = stackGet(process->process->valueStack, argc);
                ///printf("sending %x message %s argc %x\n", receiver, message->data[0], argc);
                /* Find the corresponding method by using bind() */
                method = bind(receiver, message);
                /* if the method is apply, the receiver becomes the method */
                if (unlikely(method->closure->function == &vApply))
                {
                    method = receiver;
                    // remove extra unused receiver argument
                    Stack *stack = process->process->valueStack;
                    Size entries = process->process->valueStack->entries;
                    memcpyd(stack->bottom + entries - argc - 1,
                        stack->bottom + entries - argc, argc);
                    pop();
                }
                if (unlikely(method == NULL))
                {
                    /* Bind failed, send DoesNotUnderstand */
                    while (argc --> 0)
                        pop();
                    push(message);
                    method = bind(receiver, doesNotUnderstandSymbol);
                    assert(method != NULL,
                        "VM Error; object at %x, proto %x %x "
                        "does not delegate to Object %x",
                        receiver, receiver->proto,
                        receiver->proto->proto, objectProto);
                    push(method);
                    vApply(process);
                    return; // exit due to error.
                }
                push(method);
                if (method->closure->type == internalClosure)
                {
                    vApply(process);
                }
                else // user-defined method.
                {
                    /* Apply, using the arguments on the stack */
                    vApply(process);
                    closureSetup(process);
                }
            } break;
            case stopBC: // stop
            {
                pop();
            } break;
            case setBC: // set variable
            {
                setVar(currentScope, translate(*currentIP++),
                    pop());
            } break;
            case returnBC: // return-from
            {
                panic("Not implemented");
            } break;
            case endBC: // end
            {
                processPopScope(process);
                process->process->depth--;
                if (process->process->depth == 0) // process done
                    return;
                if (currentClosure->closure->type == internalClosure)
                    return; // return to vApply()
            } break;
            case initBC: // object init
            {
                Object *self = stackTop(process->process->valueStack);
                assert(self != NULL, "Cannot initialize null object");
                assert((void**)self->object != (void**)0, "Object already initialized");
                assert(self->object[0] == NULL, "Object already initialized");
                assert(self->methods == NULL, "Object already initialized");
                
                /* Setup variables for the level 0 scope */
                self->object[0] = scopeNew(NULL, currentScope);
                Object *scope = self->object[0];
                
                Size varc = *currentIP++;
                Object *varNames[varc];
                Map *varMaps[varc];
                Size i = varc;
                while (i --> 0)
                {
                    varNames[i] = translate(*currentIP++);
                    varMaps[i] = mapNew();
                    mapSetVal(varMaps[i], scope->scope->thisWorld, NULL);
                }
                scope->scope->variables = symbolMapNew(varc, (void**)varNames,
                    (void**)varMaps);
                
                Size methodCount = *currentIP++;
                Object *methodNames[methodCount];
                Object *methodClosures[methodCount];
                i = methodCount;
                
                /* Setup methods */
                while (i --> 0)
                {
                    methodNames[i] = translate(*currentIP++);
                    assert(*currentIP++ == blockBC, "Malformed Bytecode");
                    blockLiteral(process);
                    methodClosures[i] = pop();
                    methodClosures[i]->closure->isMethod = true;
                    methodClosures[i]->closure->scope =
                        self->object[0];
                }
                
                self->methods = symbolMapNew(methodCount,
                    (void**)methodNames, (void**)methodClosures);
            } break;
            default:
            {
                panic("Unknown Bytecode %x", currentIP[-1]);
            }
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

/* Does a copy of the Map<world, SymbolMap<variable, value>> structure used to
 * store variables. */
SymbolMap *variablesCopy(SymbolMap *vars)
{
    SymbolMap *variables = symbolMapCopy(vars);
    /* Iterate through the values of the symbolMap and replace with copies */
    int i = symbolMapBuckets(variables->entries);
    do
    {
        i--;
        Map *map = (Map*)variables->hashTable[i];
        if (map == NULL)
            continue;
        variables->hashTable[i] = (Size)mapCopy(map);
    } while (i-- > 0);
    return variables;
}

void vObjectNew(Object *process)
{
    Object *proto = pop();
    Object *object = malloc(sizeof(Object));
    object->proto = proto;
    object->methods = NULL;
    
    Size protoChainLen = 1;
    Object *current = proto;
    for (; current->object != 0 && current->proto != NULL; current = current->proto)
        protoChainLen++;
    
    object->object = calloc(sizeof(Object*), protoChainLen);
    
    /* Initialize all the super scopes */
    
    Size i;
    for (i = 1; i < protoChainLen; i++)
    {
        if ((void**)proto->object == (void**)0 || proto->object[i - 1] == NULL)
            continue;
        object->object[i] = scopeNew(NULL, currentScope->scope->parent);
        Scope *scope = object->object[i]->scope;
        scope->variables = variablesCopy(proto->object[i - 1]->scope->variables);
    }
    
    push(object);
}

void vReturnFalse(Object *process)
{
    push(falseObject);
}

void exceptionRaise(Object *process)
{
    Object *symbol = pop();
    pop(); // exception proto
    panic("Raising %s error Not implemented", symbol->data[0]);
}

Object *worldNew(Object *parent)
{
    Object *world = malloc(sizeof(Object) + sizeof(World));
    world->proto = globalWorld;
    world->world->parent = parent;
    return world;
}

void worldSpawn(Object *process)
{
    push(worldNew(pop()));
}

void worldDo(Object *process)
{
    Object *block = pop();
    Object *self = pop();
    process->process->world = self;
    push(block);
    vApply(process);
    push(NULL);
}

void worldCommit(Object *process)
{
    panic("Not implemented");
}

void vmInstall()
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
    
    doesNotUnderstandSymbol = symbolNew("doesNotUnderstand:");
    objectMethodSymbols[0] = doesNotUnderstandSymbol;
    objectMethods[0] = internalMethodNew(vDoesNotUnderstand, 2);
    
    objectMethodSymbols[1] = isNilSymbol;
    objectMethods[1] = internalMethodNew(vReturnFalse, 1);
    
    objectMethodSymbols[2] = symbolNew("new");
    objectMethods[2] = internalMethodNew(vObjectNew, 1);
    
    objectProto->methods = symbolMapNew(objectMethodCount,
        (void**)objectMethodSymbols, (void**)objectMethods);
    
    /* Setting up SymbolProto */
    
    Size symbolMethodCount = 1;
    Object *symbolMethodSymbols[symbolMethodCount];
    Object *symbolMethods[symbolMethodCount];
    
    symbolMethodSymbols[0] = symbolNew("asString");
    symbolMethods[0] = internalMethodNew(symbolAsString, 1);
    
    symbolProto->methods = symbolMapNew(symbolMethodCount,
        (void**)symbolMethodSymbols, (void**)symbolMethods);
    
    thisWorldSymbol = symbolNew("thisWorld");
    
    /* Setting up ClosureProto */
    
    Size closureMethodCount = 3;
    Object *closureMethodSymbols[closureMethodCount];
    Object *closureMethods[closureMethodCount];
    
    closureMethodSymbols[0] = symbolNew("apply");
    closureMethods[0] = internalMethodNew(vApply, 1);
    
    closureMethodSymbols[1] = symbolNew("apply:");
    closureMethods[1] = internalMethodNew(vApply, 2);
    
    closureMethodSymbols[2] = symbolNew("apply:and:");
    closureMethods[2] = internalMethodNew(vApply, 3);
    
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
    
    /* Setting up Exception */
    
    exceptionProto = objectNew();
    
    Object *exceptionKeys[] =
    {
        symbolNew("raise:"),
    };
    Object *exceptionValues[] =
    {
        internalMethodNew(exceptionRaise, 2),
    };
    
    exceptionProto->methods = symbolMapNew(1, (void**)exceptionKeys, (void**)exceptionValues);
    
    /* Setting up integer */
    
    integerProto = objectNew();
    
    Object *integerKeys[] =
    {
        symbolNew("+"),
        symbolNew("-"),
        symbolNew("*"),
        symbolNew("/"),
        symbolNew("%"),
        symbolNew("asString"),
        symbolNew("and:"),
        symbolNew("or:"),
        symbolNew("xor:"),
        symbolNew("^"),
        symbolNew("factorial"),
        symbolNew("to:do:"),
    };
    Object *integerValues[] =
    {
        internalMethodNew(integerAdd, 2),
        internalMethodNew(integerSub, 2),
        internalMethodNew(integerMul, 2),
        internalMethodNew(integerDiv, 2),
        internalMethodNew(integerMod, 2),
        internalMethodNew(integerAsString, 1),
        internalMethodNew(integerAnd, 2),
        internalMethodNew(integerOr, 2),
        internalMethodNew(integerXor, 2),
        internalMethodNew(integerExp, 2),
        internalMethodNew(integerFactorial, 1),
        internalMethodNew(integerToDo, 3),
    };
    
    integerProto->methods = symbolMapNew(12, (void**)integerKeys, (void**)integerValues);
    
    /* Setting up global world */
    
    globalWorld = worldNew(NULL);
    
    Object *worldKeys[] =
    {
        symbolNew("spawn"),
        symbolNew("do:"),
        symbolNew("commit"),
    };
    Object *worldValues[] =
    {
        internalMethodNew(worldSpawn, 1),
        internalMethodNew(worldDo, 1),
        internalMethodNew(worldCommit, 1),
    };
    
    globalWorld->methods = symbolMapNew(3, (void**)worldKeys, (void**)worldValues);
    
    /* Setting up Global Scope */
    
    globalScope = scopeNew(NULL, NULL);
    globalScope->scope->containing = NULL;
    globalScope->scope->thisWorld = globalWorld;
    printf("global scope at %x\n", globalScope);
    
    Size globalVarCount = 2;
    Object *globalKeys[] =
    {
        symbolNew("Object"),
        symbolNew("Console"),
    };
    
    Map *globalValues[] =
    {
        mapNewWith(globalWorld, objectProto),
        mapNewWith(globalWorld, console),
    };
    printf("Console at %x\n", console);
    
    globalScope->scope->variables = symbolMapNew(globalVarCount,
        (void**)globalKeys, (void**)globalValues);
}
