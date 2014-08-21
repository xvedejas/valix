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
Object *world_new(Object *self, Object *scope)
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
    world->data = data;
    data->scope = scope;
    data->parent = parentWorld;
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
    // In the current scope (and its parents) find each instance of variables
    // modified in world "self" and set the values of world self->parent to
    // these values.
    Object *thisScope = self->world->scope;
    
    /// I'm not sure how hard this requirement should be. I need to re-read the paper.
    //assert(thisScope == stackTop(scopeStack), "Can only commit world in the "
    //       "scope it was defined in");
    
    Object *parentScope = thisScope->scope->containing;
    VarList *table = thisScope->scope->variables;
    
    // First we need to guarantee consistency. For each variable in the varList,
    // there is a list of accesses of the variable from any world that read from
    // it.
    
    // for now, return false. In the future an error might be thrown instead.
    if (!varListCheckConsistent(table, self))
        return falseObject;
    
    
	return trueObject;
}
