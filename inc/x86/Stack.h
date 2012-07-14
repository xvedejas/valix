/*  Copyright (C) 2012 Xander Vedejas <xvedejas@gmail.com>
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
 
#ifndef __Stack_h__
#define __Stack_h__
#include <main.h>

/////////////////////
// Stack Interface //
/////////////////////

// FILO data structure

typedef struct
{
	/* "size" is the number of items, "capacity" is the number of items the
	 * stack can hold until a resize is needed. */
	Size size, capacity;
	void **array;
} Stack;

Stack *stackNew();
void stackPush(Stack *stack, void *value);
void *stackPop(Stack *stack);
void stackDel(Stack *stack);

