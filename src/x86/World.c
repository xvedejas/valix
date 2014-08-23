 /*  Copyright (C) 2014 Xander Vedejas <xvedejas@gmail.com>
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
 *      Xander Vedejas <xvedejas@gmail.com>
 */

#include <World.h>
#include <mm.h>
#include <VarList.h>
#include <Scope.h>

void worldInstall()
{
    /* the current world is referenced by "this world", where "this" is a
     * special variable returning the current scope object */
    Object *worldMT = object_send(methodTableMT, symbol("new:"), 3);
    worldProto = object_send(objectProto, newSymbol);
    worldProto->methodTable = worldMT;
    
    methodTable_addClosure(worldMT, symbol("new:"),
		closure_newInternal(closureProto, world_new, 2));
    methodTable_addClosure(worldMT, symbol("commit"),
		closure_newInternal(closureProto, world_commit, 1));
    methodTable_addClosure(worldMT, symbol("do:"),
		closure_newInternal(closureProto, world_do, 2));
}


// "scope" is the scope in which this world is being defined.
Object *world_new(Object *self, Object *scope, Object **catches)
{
    Object *parentWorld;
    if (scope != NULL)
    {
        Object *parentScope = scope->scope->containing;
        parentWorld = parentScope->scope->world;
    }
    else
    {
        parentWorld = NULL;
    }
    
    Object *world = object_new(self);
    World *data = malloc(sizeof(World));
    world->world = data;
    data->scope = scope;
    data->parent = parentWorld;
    data->catches = catches;
    data->expectedParentState = stringMapNew();
    return world;
}


Object *world_do(Object *self, Object *block)
{
    block->closure->world = self;
    return send(block, "eval");
}

// commit "self" to its parent world
Object *world_commit(Object *self)
{
    /* First, check that the state of the parent world is consistent with what
     * the current world expects. */
    World *world = self->world;
    StringMap *expectedState = world->expectedParentState;
    Object *parentScope = world->parent->world->scope;
    
    /* Iterate through each key in expectedState and make sure things are
     * consistent */
    
    StringMapIter iter;
    stringMapIterNew(expectedState, &iter);
    void *varname;
    for (; (varname = stringMapIterKey(&iter)) != NULL; stringMapIterNext(&iter))
    {
        Object *expectedValue = stringMapIterValue(&iter);
        Object *value = scope_lookupVar(parentScope, symbol(varname));
        if (value != expectedValue)
            panic("inconsistent world state");
    }
    
    /* Now iterate through all variables set by this world in parent scopes and
     * apply them as the parent world. */
    Object *scope = world->scope;
    while (scope != NULL)
    {
        varListCommit(scope->scope->variables, self);
        scope = scope->scope->containing;
    }
    /* Now we can clear the expectedState */
    stringMapDel(expectedState);
    world->expectedParentState = stringMapNew();
    
    return NULL;
}
