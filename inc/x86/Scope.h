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
#ifndef __scope_h__
#define __scope_h__
#include <mm.h>
#include <vm.h>

/* A scope is, in a sense, a running "instance" of a block/"closure". 
 * It contains the values of local variables and knows about parent and calling
 * scopes. */
typedef struct scope
{
	VarList *variables; // Map<variable symbol, list of (world, value) pairs>
    /* Each scope has a _single_ world. That is, when a new world is created, it
     * requires a new scope. By default, scopes defined within a world also
     * exist in that world. The real effect is on the state of variables seen in
     * the parent scope, not in the current one. */
	Object *world;
    // parent scope (where this was declared) that we can look for variables in
	Object *containing;
	Object *caller; // scope that this scope was called from
	/* The following data is for storage when executing a child scope */
	Object *closure;
	u8 *bytecode; // beginning of bytecode
    Size IP; // index of bytecode
} Scope;

Object *globalScope;

extern void scopeInstall(void **global_symbols, Size symbols_array_len);

extern Object *scope_new(Object *self, Object *process, Object *containing,
                         Object *caller, Object *world, Size argCount,
                         Size varCount, Object **symbols);

extern Object *scope_lookupVar(Object *self, Object *symbol);
extern void scope_setVar(Object *self, Object *symbol, Object *value);
extern void scope_throw(Object *self, Object *error);
extern Object *scope_spawn(Object *self);
extern Object *scope_world(Object *self);

#endif // __scope_h__
