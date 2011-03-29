#include <vm.h>
#include <threading.h>
#include <mm.h>
#include <data.h>
#include <string.h>
#include <types.h>

/* Bytecode is not null-terminated! it is terminated with EOS and contains
 * multiple null characters */
ThreadFunc execute(u8 *bytecode)
{
    u8 *IP = bytecode; /* Instruction Pointer */
    Stack *scopeStack = stackNew();
    Stack *valueStack = stackNew();
    
    void scopeNew(Size varCount)
    {
        Scope *scope = malloc(sizeof(Scope));
        scope->variableCount = varCount;
        scope->variables = calloc(varCount, sizeof(Object*));
        stackPush(scopeStack, scope);
    }    
    
    Object *getVariable(Size index)
    {
        Scope *scope = stackTop(scopeStack);
        Object *object = scope->variables[index];
        return object;
    }
    
    Size methodCount;
    Size numOfMethods = *IP++;
    String *methodNames = malloc(numOfMethods * sizeof(String));
    for (methodCount = 0; methodCount < numOfMethods; methodCount++)
    {
        String name = (String)IP;
        methodNames[methodCount] = name;
        IP += strlen(name) + 1;
    }
    
    while (true)
    {
        switch (*IP++)
        {
            case 0x80: /* Import */
            {
                String module = (String)IP;
                IP += strlen(module) + 1;
                printf("Importing %s\n", module);
            } break;
            case 0x81: /* String */
            {
                panic("Not implemented!");
            } break;
            case 0x82: /* Number */
            {
                Object *intObject = malloc(sizeof(Object));
                intObject->type = integerData;
                intObject->integer = strtoul((String)IP, NULL, 0);
				stackPush(valueStack, intObject);
                IP += strlen((String)IP) + 1;
            } break;
            case 0x83: /* Keyword */
            {
				stackPush(valueStack, getVariable(*IP++));
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
                scopeNew(*IP++);
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
                //runtimeRequire(false, "Execution error: Malformed bytecode.");
            } break;
        }
    }
}


void vmInstall()
{
    return;
}
