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
