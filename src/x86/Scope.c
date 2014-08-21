#include <Scope.h>
#include <VarList.h>
#include <World.h>

void scopeInstall(void **global_symbols, Size symbols_array_len)
{
    Object *scopeMT = methodTable_new(methodTableMT, 2);
    scopeProto = object_new(objectProto);
    scopeProto->methodTable = scopeMT;
    
    methodTable_addClosure(scopeMT, symbol("world"),
		closure_newInternal(closureProto, scope_world, 1));
    methodTable_addClosure(scopeMT, symbol("spawn"),
		closure_newInternal(closureProto, scope_spawn, 1));
    
    globalScope = object_new(objectProto);
    Scope *globalScopeData = malloc(sizeof(Scope));
    globalScope->data = globalScopeData;
    globalScopeData->world = world_new(worldProto, NULL);
    
    globalScopeData->variables = varListNewPairs(symbols_array_len,
                                                 global_symbols,
                                                 globalScopeData->world);
    
    globalScopeData->containing = NULL;
    globalScopeData->caller = NULL;
}

/* Reads from the bytecode a list of symbols. Note: the scope's closure must
 * be set outside of this function. */
Object *scope_new(Object *self, Object *process, Object *containing,
                  Object *caller, Object *world, Size argCount, Size varCount,
                  Object **symbols)
{
    Process *processData = process->process;
    
    Object *scope = object_new(self);
    assert(self != NULL, "scope has no parent")
    Scope *scopeData = malloc(sizeof(Scope));
    scope->data = scopeData;
    scopeData->world = world;
    scopeData->containing = containing;
    scopeData->caller = caller;
    /* Now we make a mapping from the index that, in the bytecode, represents
     * the symbol to the actual symbol object. Because the symbols in the
     * bytecode have contiguous indices starting from 0, we can allocate an
     * array. */
    u8 *bytecode = processData->bytecode;
    Object **processSymbols = processData->symbols;
    
    Size symbolCount = argCount + varCount;
    scopeData->variables = varListNewPairs(symbolCount, (void**)symbols, world);
    
    return scope;
}

Object *scope_lookupVar(Object *self, Object *symbol)
{
	/// todo: throw errors instead of panicking
	if (symbol == symbol("this"))
		return self;
	Scope *scope = self->scope;
	assert(scope != NULL, "lookup error");
    Object *world = scope->world;
    Object *thisWorld = world;
    Object *value = NULL;
    while (true)
    {
        value = varListGet(scope->variables, symbol, &world);
        if (value != NULL)
            break;
        assert(scope->containing != NULL, "lookup error, variable not found");
        scope = scope->containing->scope;
    }
    if (world != thisWorld)
    {
        StringMap *expectedState = thisWorld->world->expectedParentState;
        void *expectedValue = stringMapGet(expectedState, symbol->symbol);
        if (expectedValue == NULL)
            stringMapSet(expectedState, symbol->symbol, value);
        else
            panic("inconsistent world: variable changed in parent");
    }
    return value;
}

void scope_setVar(Object *self, Object *symbol, Object *value)
{
	Scope *scope = self->scope;
	Object *world = scope->world;
	bool set;
	while (true)
	{
		Scope *containing = scope->containing->scope;
        set = varListSet(scope->variables, world, symbol, value);
        if (set)
			break;
		if (scope == globalScope->scope)
            panic("setVar Error: variable '%s' not found!", symbol->symbol);
        scope = scope->containing->scope;
    }
}

void scope_throw(Object *self, Object *error)
{ 
    panic("not implemented");
    /// error not caught
}

Object *scope_spawn(Object *self)
{
    return world_new(self->scope->world, self);
}

Object *scope_world(Object *self)
{
	Scope *scope = self->scope;
	return scope->world;
}
