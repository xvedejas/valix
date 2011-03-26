#include <vm.h>
#include <threading.h>
#include <mm.h>
#include <data.h>
#include <string.h>

/* Some globals */
Object *stringObject, *integerObject, *doubleObject;

const Size vmScopeStackSize = 512;
const Size vmArgStackSize = 16;

#define currentScope() (scopeStack[scopeStackTop])
#define currentObject(scope) (scope->argStack[scope->argStackTop])

void runtimeRequire(bool value, String error)
{
    if (unlikely(!value))
    {
        printf(error);
        printf("\nExiting thread.\n");
        endThread();
    }
}

/* This is an internal routine to simplify object creation, not an actual
 * implementation of the method 'new' for objects. */
Object *objectNew()
{
    Object *object = malloc(sizeof(Object));
    object->values = mapNew();
    return object;
}

Object *subclass(Object *class, String name, bool isAbstract)
{
    runtimeRequire(class->isAbstract, "Attempted to subclass a concrete class");
    Object *newClass = objectNew();
    newClass->isAbstract = isAbstract;
    newClass->isInstance = false;
    mapAdd(newClass->fields, "__super__", class);
    mapAdd(newClass->fields, "__name__", name);
    return newClass;
}

Object *new(Object *class)
{
    runtimeRequire(!class->isAbstract, "Attempted to instantiate an abstract class");
    Object *newObject = objectNew();
    newObject->isAbstract = false;
    newObject->isInstance = true;
    mapAdd(newObject->values, "__super__", class);
    return newObject;
}

void vmInstall()
{
    /* Let's make some classes of basic data types */
    
    stringObject = objectNew();
    stringObject->isAbstract = false; /* We want instances */
    stringObject->isInstance = false; /* This is not an instance though */
    mapAdd(stringObject->fields, "__name__", "String");
    
    Object *number = objectNew();
    number->isAbstract = true; /* We want subclasses */
    number->isInstance = false; /* Obviously this is not an instance */
    mapAdd(number->fields, "__name__", "Number");
    
    integerObject = subclass(number, "Integer", false);
    
    doubleObject = subclass(number, "Double", false);
    
    /* Now we form a global scope from which these types can be found */
    
    return;
}

/* Bytecode format:
 * 
 * byte meaning         stack args                    end directive with \0?
 *  80  import          none                          yes
 *  81  push string     none (pushes)                 yes
 *  82  push number     "                             yes
 *  83  push keyword    "                             yes
 *  84  call method     as many args as method takes  yes
 *  85  begin list      none                          no
 *  86  begin block     none                          no
 *  87  end list        none                          no
 *  88  end block       none                          no
 *  89  end statement   all                           no
 *  8A  return          [pop return value]            no
 *  8B  set equal       [pop value] [pop symbol]      no
 *  8C  binary message  two                           yes
 *  FF  EOF
 */

Object *pop(Scope *scope);

void scopeDel(Scope *scope)
{
    free(scope->locals);
    while (scope->argStackTop) pop(scope);
    free(scope->argStack);
    free(scope);
}

void objectDel(Object *object)
{
    free(object->values);
    free(object);
}

void push(Scope *scope, Object *object)
{
    scope->argStack[scope->argStackTop++] = object;
    runtimeRequire(scope->argStackTop < vmArgStackSize, "Stack overflow error");
}

Object *pop(Scope *scope)
{
    runtimeRequire(scope->argStackTop > 0, "Stack underflow error");
    objectDel(currentObject(scope));
    return scope->argStack[scope->argStackTop--];
}

/* Bytecode is not null-terminated! it is terminated with EOS and contains multiple null characters */
ThreadFunc execute(String bytecode)
{
    /* We're starting a new execution thread, so setup our stacks and scopes */
    Scope **scopeStack = malloc(sizeof(Scope*) * vmScopeStackSize);
    Size scopeStackTop = 0;
    
    void pushScope(Scope *scope)
    {
        scopeStack[scopeStackTop++] = scope;
        runtimeRequire(scopeStackTop < vmScopeStackSize, "Stack overflow error");
    }

    Scope *popScope()
    {
        runtimeRequire(scopeStackTop > 0, "Stack underflow error");
        scopeDel(currentScope());
        scopeStackTop--;
        return currentScope();
    }
    
    Scope *topScope = malloc(sizeof(Scope));
    topScope->argStack = malloc(sizeof(Object*) * vmArgStackSize);
    topScope->argStackTop = 0;
    pushScope(topScope);
    
    void import(String module)
    {
        printf("Importing %s\n", module);
        /// todo
    }
    
    Object *scopeFindVar(String keyword)
    {
        int i;
        Object *object = NULL;
        for (i = 0; object == NULL && i <= scopeStackTop; i++)
            object = mapGet(scopeStack[scopeStackTop - i]->locals, keyword);
        return object;
    }
    
    Size i = 0;
    
    while (true)
    {
        printf("bytecode: %s\n", bytecode + i);
        switch ((u8)bytecode[i])
        {
            case 0x80: /* Import */
            {
                String module = bytecode + i + 1;
                import(module);
                i += strlen(module) + 1;
            } break;
            case 0x81: /* String */
            {
                String string = bytecode + i + 1;
                Object *object = new(stringObject);
                mapAdd(object->values, "__data__", string);
                push(currentScope(), object);
                i += strlen(string) + 1;
            } break;
            case 0x82: /* Number */
            {
                String number = bytecode + i + 1;
                Object *object;
                if (strchr(number, '.')) /* is a Double */
                    object = new(doubleObject);
                else /* is an Integer */
                    object = new(integerObject);
                mapAdd(object->values, "__data__", number);
                push(currentScope(), object);
                i += strlen(number) + 1;
            } break;
            case 0x83: /* Keyword */
            {
                String keyword = bytecode + i + 1;
                push(currentScope(), scopeFindVar(keyword));
                i += strlen(keyword) + 1;
            } break;
            case 0x84: /* Call Method */
            {
                /* This deals with keyword methods, not binary method */
                Size argc = chrcount(bytecode + i + 1, ':');
                printf("Method argc: %i\n", argc);
                panic("Not implemented!");
            } break;
            case 0x85: /* Begin List */
            {
                panic("Not implemented!");
            } break;
            case 0x86:
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
            case 0x89:
            {
                panic("Not implemented!");
            } break;
            case 0x8A:
            {
                panic("Not implemented!");
            } break;
            case 0x8B: /* Equality */
            {
                String variable = bytecode + i + 1;
                printf("Adding variable %s\n", variable);
                mapAdd(currentScope()->locals, variable, pop(currentScope()));
                i += strlen(variable) + 1;
            } break;
            case 0x8C:
            {
                panic("Not implemented!");
            } break;
            case 0xFF: /* EOS */
            {
                printf("Execution done!. Exiting thread.");
                endThread();
            } break;
            default:
            {
                runtimeRequire(false, "Execution error: Malformed bytecode.");
            } break;
        }
    }
}
