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
#ifndef __data_h__
#define __data_h__
#include <main.h>
#include <threading.h>

///////////////////////
// General Functions //
///////////////////////

extern Size stringHash(String s);
// Increase some size by 50% rounded up
extern Size nextSize(Size size);
// Hash something by pointer value or absolute value
extern Size valueHash(void *value);

///////////////////////////
// InternTable Interface //
///////////////////////////

typedef struct
{
    Size count, capacity;
    String *table;
} InternTable;

extern InternTable *internTableNew();
extern Size internString(InternTable *table, String string);
extern void internTableDel(InternTable *table);
extern bool isStringInterned(InternTable *table, String string);

/////////////////////
// Stack Interface //
/////////////////////

/////////////////////////////
// StringBuilder Interface //
/////////////////////////////

typedef struct
{
    Size capacity, size;
    String s;
} StringBuilder;

extern StringBuilder *stringBuilderNew(String initial);
/* note: Del will destroy both the string builder and its contents */
extern void stringBuilderDel(StringBuilder *sb);
extern void stringBuilderAppend(StringBuilder *sb, String s);
extern void stringBuilderAppendN(StringBuilder *sb, String s, Size len);
extern void stringBuilderAppendChar(StringBuilder *sb, char c);
/* note: ToString will destroy the string builder! */
extern String stringBuilderToString(StringBuilder *sb);
void stringBuilderPrint(StringBuilder *sb);

/////////////////////
// Array Functions //
/////////////////////

void radixSort(u32 *array, Size n);
u32 maxItem(u32 *array, Size n);
void quickSort(u32 *array, Size n);
void insertSort(u32 *array, Size n);

#endif
