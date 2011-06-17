/*  Copyright (C) 2010 Xander Vedejas <xvedejas@gmail.com>
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

///////////////////
// Map Interface //
///////////////////

typedef enum
{
    nullKey = 0,
    stringKey = 1,
    valueKey = 2
} MapKeyType;

typedef struct assoc
{
    void *key;
    void *value;
    MapKeyType type;
    struct assoc *next;
} Association;

typedef struct
{
    Association *A; // hashtable A, pointer to first item in array
    Association *B; // hashtable B, pointer to first item in array
    Size entriesA;
    Size entriesB;
    Size capacityA;
    Size capacityB;
} Map;

Map *mapNew();
Map *mapNewSize(Size startingSize);
void *mapGet(Map *map, void *key, MapKeyType type);
#define mapGetVal(map, key) mapGet(map, (void*)key, valueKey)
void _mapSet(Map *map, void *key, void *value, MapKeyType type, bool incrementallyResize);
/* Use this function to both add keys to the map or change their value */
#define mapSet(map, key, value, type) _mapSet(map, key, value, type, true)
#define mapSetVal(map, key, value) mapSet(map, (void*)key, (void*)value, valueKey)
bool mapRemove(Map *map, void *key, MapKeyType type);
#define mapRemoveVal(map, key) mapRemove(map, (void*)key, valueKey)
void mapDel(Map *map);
void mapDebug(Map *map);

///////////////////////////
// InternTable Interface //
///////////////////////////

/* The internal interned value is one more than actual, so that a 0
 * may represent undefined value. */

typedef struct
{
    Map *entries;
    Size count;
} InternTable;

InternTable *internTableNew();
Size internString(InternTable *table, String string);
void internTableDel(InternTable *table);
bool isStringInterned(InternTable *table, String string);

/////////////////////
// Stack Interface //
/////////////////////

typedef struct
{
    void **bottom;  // pointer to first element in the stack array
    Size entries;
    Size capacity;  // size of the array
} Stack;

extern Stack *stackNew();
extern void stackPush(Stack *stack, void *element);
extern void *stackPop(Stack *stack);
extern void stackDel(Stack *stack);
extern void stackDebug(Stack *stack);
extern void *stackTop(Stack *stack);

/////////////////////////////
// StringBuilder Interface //
/////////////////////////////

typedef struct
{
    String s;
    Size size;
    Size capacity;
} StringBuilder;

extern StringBuilder *stringBuilderNew(String initial);
/* note: Del will destroy both the string builder and its contents */
extern void stringBuilderDel(StringBuilder *sb);
extern void stringBuilderAppend(StringBuilder *sb, String s);
void stringBuilderAppendN(StringBuilder *sb, String s, Size len);
void stringBuilderAppendChar(StringBuilder *sb, char c);
/* note: ToString will destroy the string builder! */
extern String stringBuilderToString(StringBuilder *sb);

/////////////////////
// Queue Interface //
/////////////////////

typedef struct
{
    void **array;
    Size start, end; // indexes
    Size size; // static
} Queue;

extern Queue *queueNew(Size size);
extern void enqueue(Queue *queue, void *value);
extern void *dequeue(Queue *queue);

#endif
