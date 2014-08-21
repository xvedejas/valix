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
#ifndef __bool_h__
#define __bool_h__

#include <vm.h>

extern void booleanInstall();
extern Object *boolean_new(Object *self);
extern Object *boolean_toString(Object *self);
extern Object *boolean_ifTrue(Object *self, Object *block);
extern Object *boolean_ifFalse(Object *self, Object *block);
extern Object *boolean_ifTrueifFalse(Object *self, Object *trueBlock,
							                Object *falseBlock);
extern Object *boolean_and(Object *self, Object *other);
extern Object *boolean_or(Object *self, Object *other);
extern Object *boolean_xor(Object *self, Object *other);
extern Object *boolean_eq(Object *self, Object *other);
extern Object *boolean_not(Object *self, Object *other);

#endif // __bool_h__
