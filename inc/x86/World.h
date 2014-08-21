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
#ifndef __world_h__
#define __world_h__

#include <vm.h>

/* All variables are explicitly declared, associated with some scope. Those
 * variables can only be accessed from within the scope or inner scopes.
 * Note also that variables can be re-declared in an inner scope, in which case
 * refering to a variable name always means the inner-most such named variable.
 * 
 * Scopes thus have a tree-like structure (each scope points to a parent scope).
 * When looking up the value of any variable, we traverse this tree upwards
 * until such a variable is found or we reach the top-level "universal" scope,
 * at which point an error is raised.
 * 
 * This file handles scopes and variables, but it also handles "worlds", which
 * are essentially "multiple diverging program states". From any scope you can
 * "spawn" a new world (and at the same time a new scope), and any changes of
 * variables defined outside of this world but modified by this world are only
 * seen within the world.
 * 
 * The state of a world can later be "committed" to its parent world, but only
 * if the execution of code in the child world did not depend on any changes to
 * the parent world between spawn and commit. This is called "consistency".
 * 
 * The way variables, scopes, and worlds work together is a bit nuanced. I will
 * do my best to describe the code here, though the paper which inspired this
 * should also be helpful: http://www.vpri.org/pdf/tr2011001_final_worlds.pdf
 * 
 * There are basically four high-level operations that we care about:
 *   - scope_spawn:  Creates a new world.
 *   - world_do:     Given a block, executes in the world (with its own scope)
 *   - world_commit: Copies values from a child world to its parent world.
 *                   "Resets" the child world to not differ from its parent
 *                   any longer.
 *                   Before any of this, checks for consistency. If the state is
 *                   inconsistent, throws an error.
 *   - lookupVar:    We grab the value of this variable which is associated with
 *                   the innermost world which is an ancestor of this world (or
 *                   is this world)**.
 *                   We keep a record in this world that this is now the state
 *                   expected of the parent from that scope if the value wasn't
 *                   grabbed from a state associated with the current world.
 *                   We always find this value from the scope in which the
 *                   variable was declared.
 *   - setVar:       When we attempt to set the value of a variable which is
 *                   defined in a scope outside this world, we keep track of
 *                   its value in that same scope, but associated with the
 *                   current world. This does not affect the expected parent
 *                   state.
 * 
 * **this might be an O(n^2) operation worst case. I will keep thinking about
 *   whether there is a simple way to fix this.
 * 
 */

typedef struct object Object;

typedef struct world
{
    Object *parent; // world of the parent scope (parent world)
    Object *scope; // scope that this world was defined in
    Object **catches; // null-terminated list of errors which this world catches (todo)
    Object **expectedParentState; // a list of (scope, varname, value)
} World;

extern void worldInstall();
extern Object *world_new(Object *self, Object *scope);
extern Object *world_do(Object *self, Object *block);
extern Object *world_commit(Object *self);

#endif // __world_h__
