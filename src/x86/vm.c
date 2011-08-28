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
#include <math.h>
#include <Number.h>
#include <String.h>

Map *globalSymbolTable;

Object *globalScope;

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
    scope->valueStack = stackNew();
    scope->errorCatch[0] = NULL;
    scope->fromInternal = false;
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

Object *stringNewNT(String s) // null terminated
{
    return stringNew(strlen(s), s);
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
    object->number->type = integer;
    object->number->data[0] = value;
    return object;
}

Object *charNew(u8 value)
{
    Object *object = objectNew();
    object->proto = charProto;
    object->data[0] = (void*)(u32)value;
    return object;
}

void vDoesNotUnderstand(Object *process)
{
    call(process, doesNotUnderstandException, "raise");
    push(NULL);
}

void vApply(Object *process);

void vPrint(Object *process)
{
    Object *str = call(process, pop(), "asString");
    printf("%S", str);
}

void vPrintNl(Object *process)
{
    Object *str = call(process, pop(), "asString");
    printf("%S\n", str);
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
    Object *value = NULL;
    Map *values;
    Object *startingWorld = scope->scope->thisWorld;
    Object *world = startingWorld;
    
    while (value == NULL)
    {
        while ((values = symbolMapGet(scope->scope->variables, symbol)) == NULL)
        {
            if ((scope = scope->scope->containing) == NULL)
                return NULL;
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
     * current world because we read the value. Also we keep track of the value
     * that was read, and put it in the world's "reads" map. */
    if (world != startingWorld)
    {
        mapSetVal(values, startingWorld, value);
        if (startingWorld->world->reads != NULL)
            mapSetVal(startingWorld->world->reads, values, value);
    }
    return value;
}

void setVar(Object *scope, Object *symbol, Object *newValue)
{
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
    
    // note that this value has been modified in the current world
    if (startingWorld->world->modifies != NULL)
        listAdd(startingWorld->world->modifies, values);
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
    Object *block = pop();
    assert(block != NULL, "Internal error; method does not exist");
    Closure *closure = block->closure;
    // if we are calling from an internal function, save this state
    bool fromInternal = process->process->inInternal;
    
    if (closure->type == internalClosure)
    {
        process->process->inInternal = true;
        closure->function(process);
        process->process->inInternal = fromInternal;
    }
    else /* we create a new scope if the closure is a user-defined closure. */
    {
        process->process->depth++;
        process->process->inInternal = false;
        Object *scope;
        /* Here we see if we must correct for the scope of a method when the
         * method is defined for a prototype instead of the given object */
        if (closure->isMethod)
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
        
        Size i;
        for (i = 0; i < closure->argc; i++)
            pushTo(pop(), scope); // move arguments to new scope's stack
            
        processPushScope(process, scope);
        currentScope->scope->IP = closure->bytecode;
        if (fromInternal)
        {
            scope->scope->fromInternal = true;
            processMainLoop(process);
            process->process->inInternal = true;
        }
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
    
    process->process->depth = 1;
    Object *closure = closureNew(NULL);
    closure->closure->type = userDefinedClosure;
    Object *scope = scopeNew(closure, globalScope);
    process->process->world = globalWorld;
    process->process->inInternal = false;
    
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

/* with callArgsOnStack, the topmost item on the stack should be the argcount,
 * then the symbol of the message */
void callArgsOnStack(Object *process)
{
    Size argc = pop()->number->data[0];
    Object *message = pop();
    Object *receiver = stackGet(currentScope->scope->valueStack, argc);
    Object *method = bind(receiver, message);
    printf("sending %x %s %x\n", receiver, message->data[0], method);
    /* if the method is apply, the receiver becomes the method */
    if (unlikely(method->closure->function == &vApply))
    {
        method = receiver;
        // remove extra unused receiver argument
        Stack *stack = currentScope->scope->valueStack;
        Size entries = stack->entries;
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
        if (method == NULL)
            call(process, vmError, "raise:",
            symbolNew("Error: object does not implement doesNotUnderstand:."));
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
}

/* Syntactic argument count, does not include receiver, so might need to add one
 * if dealing with the real argument count of methods */
Size getArgc(String message)
{
    return (strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_",
        message[0]))? chrcount(message, ':') : 1;
}

Object *call(Object *process, Object *object, String message, ...)
{
    Size argc = getArgc(message);
    push(object);
    va_list argptr;
    va_start(argptr, message);
    Size i = argc;
    while (i --> 0)
        push(va_arg(argptr, Object*));
    va_end(argptr);
    push(symbolNew(message));
    push(integerNew(argc));
    callArgsOnStack(process);
    return pop();
}

void processMainLoop(Object *process)
{
    closureSetup(process);
    
    if (!setjmp(process->process->exit))
        while (true)
    {
        ///printf("Bytecode %s at %x\n", bytecodes[*currentIP - 0x80], currentIP);
        switch (*currentIP++)
        {
            case integerBC: // integer literal
            {
                Size value = strtoul((String)currentIP, NULL, 10);
                currentIP += strlen((String)currentIP) + 1;
                Object *integer = integerNew(value);
                push(integer);
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
            case messageBC: // message 0x89
            {
                Size argc = (Size)*currentIP++;
                push(integerNew(argc));
                callArgsOnStack(process);
            } break;
            case stopBC: // stop
            {
                pop();
            } break;
            case setBC: // set variable
            {
                setVar(currentScope, translate(*currentIP++), pop());
            } break;
            case returnBC: // return-from
            {
                panic("Not implemented");
            } break;
            case endBC: // end
            {
                Object *oldScope = currentScope;
                processPopScope(process);
                push(popFrom(oldScope)); // return value
                
                process->process->depth--;
                if (oldScope->scope->fromInternal)
                    return; // return to vApply()
                
                if (process->process->depth == 0)
                    panic("error");
            } break;
            case initBC: // object init
            {
                Object *self = arg(0);
                if (self == NULL)
                    call(process, vmError, "raise:",
                    symbolNew("Cannot instantiate null object"));
                if ((void**)self->object == (void**)0 || self->object[0] != NULL
                    || self->methods != NULL)
                    call(process, vmError, "raise:",
                        symbolNew("Object already initialized"));
                
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
            case 0x04: // EOF
            {
                longjmp(process->process->exit, true);
            } break;
            default:
            {
                panic("Unknown Bytecode %x", currentIP[-1]);
            }
        }
    }
}

void vObjectAsString(Object *process)
{
    Object *self = pop();
    Size size = floorlog10((u32)self) + 13;
    String s = malloc(sizeof(char) * size);
    strcpy(s, "<Object at: ");
    itoa(integer, s + 12, 10);
    s[size - 1] = '>';
    push(stringNew(size, s));
}

void vObjectEq(Object *process)
{
    /* For Object, == is the same as is: but in general Eq will be overridden to
     * return true in more situations (for instances, integers of equal value */
    Object *obj = pop();
    Object *self = pop();
    if (obj == self)
        push(trueObject);
    else
        push(falseObject);
}

void vObjectIs(Object *process)
{
    Object *obj = pop();
    Object *self = pop();
    if (obj == self)
        push(trueObject);
    else
        push(falseObject);
}

void symbolAsString(Object *process)
{
    String s = pop()->data[0];
    push(stringNew(strlen(s), s));
}

void yourself(Object *process)
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

void throw(Object *process)
{
    Object *symbol = pop();
    pop(); // self
    
    /* Search through scope to find one that's catching errors. */
    Object *scope = currentScope;
    
    while (scope->scope->errorCatch[0] == NULL)
    {
        process->process->depth--;
        if ((scope = scope->scope->parent) == NULL)
            panic("Did not catch throwable: %s", symbol->data[0]);
    }
    currentScope = scope;
    push(symbol);
    longjmp(scope->scope->errorCatch, true);
    panic("catch");
}

void throwArg(Object *process)
{
    Object *stringArg = pop();
    Object *self = pop();
    Object *new = call(process, self, "new:", stringArg);
    push(call(process, new, "raise"));
}

Object *worldNew(Object *parent)
{
    Object *world = malloc(sizeof(Object) + sizeof(World));
    world->proto = globalWorld;
    world->world->parent = parent;
    if (parent != NULL)
    {
        world->world->reads = mapNew();
        world->world->modifies = listNew();
    }
    else
    {
        world->world->reads = NULL;
        world->world->modifies = NULL;
    }
    return world;
}

void worldSpawn(Object *process)
{
    push(worldNew(pop()));
}

void worldEval(Object *process)
{
    Object *block = pop();
    Object *self = pop();
    process->process->world = self;
    push(block);
    vApply(process);
    push(NULL);
}

bool delegatesTo(Object *obj, Object *proto)
{
    while (obj != proto)
    {
        if ((obj = obj->proto) == NULL)
            return false;
    }
    return true;
}

void vObjectDelegatesTo(Object *process)
{
    Object *proto = pop();
    Object *self = pop();
    if (delegatesTo(self, proto))
        push(trueObject);
    else
        push(falseObject);
}

bool delegatesToAny(Object *obj, Object *arrayObject)
{
    Object **array = arrayObject->array->array;
    Size i = arrayObject->array->size;
    while (i --> 0)
    {
        if (delegatesTo(obj, array[i]))
            return true;
    }
    return false;
}

void vObjectIsLiteral(Object *process)
{
    pop();
    push(falseObject);
}

void arrayAt(Object *process)
{
    Object *self = pop();
    Object *index = pop();
    push(self->array->array[index->number->data[0]]);
}

void arrayAtPut(Object *process)
{
    Object *self = pop();
    Object *index = pop();
    Object *item = arg(0);
    self->array->array[index->number->data[0]] = item;
}

void arrayLen(Object *process)
{
    Object *self = pop();
    push(integerNew(self->array->size));
}

void charAsString(Object *process)
{
    Object *self = pop();
    s8 c = (s8)(u32)self->data[0];
    push(stringNew(1, &c));
}

void worldEvalOnErrorDo(Object *process)
{
    panic("not implemented\n");
}

void worldEvalOnDo(Object *process)
{
    Object *catchBlock = pop();
    Object *errors = pop();
    Object *self = arg(1);
    
    if (setjmp(currentScope->scope->errorCatch))
    {
        currentScope->scope->errorCatch[0] = NULL;
        Object *error = arg(0);
        if (!delegatesToAny(error, errors))
        {
            printf("nope\n");
            push(bind(error, symbolNew("raise")));
            vApply(process);
        }
        process->process->world = self;
        push(catchBlock);
        vApply(process);
    }
    else
    {
        worldEval(process);
        return;
    }
    
    push(NULL);
}

/* true returns if successful, false otherwise */
void worldCommit(Object *process)
{
    Object *world = pop();
    Object *parentWorld = world->world->parent;
    assert(parentWorld != NULL, "error");
    Map *reads = world->world->reads;
    MapIterator iter;
    mapIteratorInit(&iter);
    
    /* We iterate through the world's reads. If they are consistent with the
     * values found in the parent world, we can commit. The commit fails if the
     * state of the child world might be out of date due to changes made in the
     * parent world. */
    Association *assoc;
    while ((assoc = mapNext(reads, &iter)) != NULL)
    {
        Object *readValue = (Object*)assoc->value;
        Object *parentCurrentValue = mapGetVal((Map*)assoc->key, parentWorld);
        if (readValue != parentCurrentValue)
        {
            printf("commit failed\n");
            push(falseObject);
            return;
        }
    }
    
    /* Now we can commit */
    List *list = world->world->modifies;
    Size i;
    for (i = 0; i < list->entries; i++)
    {
        Map *values = list->array[i];
        mapSetVal(values, parentWorld, mapGetVal(values, world));
    }
    
    push(trueObject);
}

/* Similar to commit, revert works in the opposite direction. thisWorld is
 * restored to the state of its parent as it currently is. This operation is
 * pretty much equivalent to spawning a new world from the parent, except that
 * another object does not need to be instantiated. */
void worldRevert(Object *process)
{
    Object *world = pop();
    Object *parentWorld = world->world->parent;
    assert(parentWorld != NULL, "error");
    Map *reads = world->world->reads;
    
    mapEmpty(reads);
    
    List *list = world->world->modifies;
    Size i;
    for (i = 0; i < list->entries; i++)
    {
        Map *values = list->array[i];
        mapRemoveVal(values, world);
    }
    
    push(NULL);
}

void worldParent(Object *process)
{
    Object *world = pop();
    push(world->world->parent);
}

void vObjectProto(Object *process)
{
    Object *self = pop();
    push(self->proto);
}

/* Returns an array of symbols representing the messages that the object
 * responds to, including those defined by its prototype */
void vObjectRespondsToAll(Object *process)
{
    Object *self = pop();
    Size size = 0;
    Object *current = self;
    do
        size += current->methods->entries;
    while ((current = current->proto) != NULL);
    Object *arrayObject = arrayNew(size);
    Object **array = arrayObject->array->array;
    current = self;
    Size arrayIndex = 0;
    
    do
    {
        Size entries = current->methods->entries;
        Size i;
        Object *value;
        for (i = 0; entries > 0; i += 2)
        {
            if ((value = (Object*)current->methods->hashTable[i]) != NULL)
            {
                entries--;
                array[arrayIndex++] = value;
            }
        } 
    }
    while ((current = current->proto) != NULL);
    push(arrayObject);
}

/* Symbol argument to the following function */
void throwableNew(Object *process)
{
    Object *symbol = pop();
    Object *new = malloc(sizeof(Object) + sizeof(Throwable));
    new->proto = pop(); // self
    new->throwable->name = symbol->data[0];
    new->throwable->message = NULL;
    push(new);
}

Object *exceptionNew(String name)
{
    Object *new = objectNew();
    new->proto = exceptionProto;
    new->data[0] = name;
    return new;
}

Object *errorNew(String name)
{
    Object *new = objectNew();
    new->proto = errorProto;
    new->data[0] = name;
    return new;
}

void throwableAsString(Object *process)
{
    Object *self = pop();
    push(stringNew(strlen(self->data[0]), self->data[0]));
}

void setInternalMethods(Object *object, Size entries, void **entry)
{
    void *keys[entries];
    void *values[entries];
    Size i;
    for (i = 0; i / 2 < entries; i += 2)
    {
        String methodName = entry[i];
        keys[i / 2] = symbolNew(methodName);
        Size argc = getArgc(methodName);
        values[i / 2] = internalMethodNew(entry[i + 1], argc + 1);
    }
    object->methods = symbolMapNew(entries, (void**)keys, (void**)values);
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
    
    methodList objectEntries =
    {
        "doesNotUnderstand:", vDoesNotUnderstand,
        "isNil", vReturnFalse,
        "new", vObjectNew,
        "proto", vObjectProto,
        "respondsToAll", vObjectRespondsToAll,
        "asString", vObjectAsString,
        "==", vObjectEq,
        "is:", vObjectIs,
        "delegatesTo:", vObjectDelegatesTo,
        "isLiteral", vObjectIsLiteral,
    };

    setInternalMethods(objectProto, 10, objectEntries);
    
    /* Setting up SymbolProto */
    
    methodList symbolEntries =
    {
        "asString", symbolAsString,
    };
    
    setInternalMethods(symbolProto, 1, symbolEntries);
    
    thisWorldSymbol = symbolNew("thisWorld");
    
    /* Setting up ClosureProto */
    
    methodList closureEntries =
    {
        "apply", vApply,
        "apply:", vApply,
        "apply:and:", vApply,
    };
    
    setInternalMethods(closureProto, 3, closureEntries);
    
    /* ProcessProto */
    
    processProto = objectNew();
    
    /* Setting up ScopeProto */
    
    scopeProto = objectNew();
    
    /* Setting up stringProto */
    
    stringSetup();
    
    /* Setting up charProto */
    
    charProto = objectNew();
    
    methodList charEntries =
    {
        "asString", charAsString,
    };
    setInternalMethods(charProto, 1, charEntries);
    
    /* Setting up arrayProto */
    
    arrayProto = objectNew();
    
    methodList arrayEntries =
    {
        "asString", arrayAsString,
        "at:", arrayAt,
        "at:put:", arrayAtPut,
        "len", arrayLen,
    };
    setInternalMethods(arrayProto, 4, arrayEntries);
    
    /* Setting up console */
    
    console = objectNew();
    
    methodList consoleEntries =
    {
        "print:", vPrint,
        "printNl:", vPrintNl,
    };
    setInternalMethods(console, 2, consoleEntries);
    
    /* Setting up Throwable. Both Exception and Error inherit from Throwable;
     * the distinction is that Error is usually a bad, internal problem, and
     * the user usually shouldn't throw or catch one. Exceptions, on the other
     * hand, should usually be caught! */
    
    throwable = objectNew();
    
    methodList throwableEntries =
    {
        "raise", throw,
        "raise:", throwArg,
        "asString", throwableAsString,
        "new:", throwableNew,
    };
    setInternalMethods(throwable, 4, throwableEntries);
    
    exceptionProto = objectNew();
    exceptionProto->proto = throwable;
    
    errorProto = objectNew();
    errorProto->proto = throwable;
    vmError = errorNew("VM Error");
    
    divideByZeroException = exceptionNew("divideByZero");
    doesNotUnderstandException = exceptionNew("doesNotUnderstand"); 
    
    /* Setting up integer */
    
    integerSetup();
    
    /* Setting up global world */
    
    globalWorld = worldNew(NULL);
    
    methodList worldEntries =
    {
        "spawn", worldSpawn,
        "eval:", worldEval,
        "eval:on:do:", worldEvalOnDo,
        "eval:onErrorDo:", worldEvalOnErrorDo,
        "commit", worldCommit,
        "revert", worldRevert,
        "parent", worldParent,
    };
    setInternalMethods(globalWorld, 7, worldEntries);
    
    /* Setting up Global Scope */
    
    globalScope = scopeNew(NULL, NULL);
    globalScope->scope->containing = NULL;
    globalScope->scope->thisWorld = globalWorld;
    
    Size globalVarCount = 4;
    Object *globalKeys[] =
    {
        symbolNew("Object"),
        symbolNew("Console"),
        symbolNew("Exception"),
        symbolNew("DivideByZero"),
        symbolNew("Array"),
        symbolNew("String"),
    };
    
    Map *globalValues[] =
    {
        mapNewWith(globalWorld, objectProto),
        mapNewWith(globalWorld, console),
        mapNewWith(globalWorld, exceptionProto),
        mapNewWith(globalWorld, divideByZeroException),
        mapNewWith(globalWorld, arrayProto),
        mapNewWith(globalWorld, stringProto),
    };
    
    globalScope->scope->variables = symbolMapNew(globalVarCount,
        (void**)globalKeys, (void**)globalValues);
}
