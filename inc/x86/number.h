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
#ifndef __number_h__
#define __number_h__
#include <main.h>
#include <vm.h>

extern void integerAdd(Object *process);
extern void integerSub(Object *process);
extern void integerMul(Object *process);
extern void integerDiv(Object *process);
extern void integerMod(Object *process);
extern void integerAsString(Object *process);
extern void integerAnd(Object *process);
extern void integerOr(Object *process);
extern void integerXor(Object *process);
extern void integerExp(Object *process);
extern void integerFactorial(Object *process);
extern void integerToDo(Object *process);
extern void integerToByDo(Object *process);
extern void integerEq(Object *process);
extern void integerSetup();

#endif
