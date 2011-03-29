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

//////////////////////////
// GenericSet Interface //
//////////////////////////

typedef struct
{
    void *key;
    void *value;
} Association;

typedef enum hashType
{
    hashedNull = 0, /* 0 so that a calloc defaults to these null values */
    hashedValue,
    hashedString,
    hashedValueAssociation,
    hashedStringAssociation
} HashType;

typedef struct setItem
{
    void *value;
    HashType type;
    struct setItem *next;
} GenericSetItem;

typedef struct
{
    Size tableSize;
    GenericSetItem table[0];
} GenericSet;

typedef GenericSet Set;
typedef GenericSet StrSet; // values are strings
typedef GenericSet Map;
typedef GenericSet StrMap; // Keys are strings; probably want to use this

Association *associationNew(void *key, void *value);

#define setNew()                   genericSetNew()
#define setDel(set)                genericSetDel(set)
#define setAdd(set, value)         genericSetAdd(set, value, hashedValue)
#define setGet(set, value)         genericSetGet(set, value, hashedValue)
#define setHas(set, value)         genericSetHas(set, value, hashedValue)
#define setRemove(set, value)      genericSetRemove(set, value, hashedValue)
#define setCopy(set)               genericSetCopy(set)
#define setUnion(dest, src)        genericSetUnion(dest, src)
#define setIntersection(dest, src) genericSetIntersection(dest, src)
#define setDifference(dest, src)   genericSetDifference(dest, src)
#define setForEach(set) { int _i; for (_i = 0; _i < set->tableSize; _i++) { GenericSetItem *_item = &set->table[_i]; do { void *value = _item->value; {
#define setForEachEnd() }} while ((_item = _item->next) != NULL); } }

#define strSetNew()                   genericSetNew()
#define strSetDel(set)                genericSetDel(set)
#define strSetAdd(set, value)         genericSetAdd(set, value, hashedString)
#define strSetGet(set, value)         genericSetGet(set, value, hashedString)
#define strSetHas(set, value)         genericSetHas(set, value, hashedString)
#define strSetRemove(set, value)      genericSetRemove(set, value, hashedString)
#define strSetCopy(set)               genericSetCopy(set)
#define strSetUnion(dest, src)        genericSetUnion(dest, src)
#define strSetIntersection(dest, src) genericSetIntersection(dest, src)
#define strSetDifference(dest, src)   genericSetDifference(dest, src)

#define mapNew()                   genericSetNew()
#define mapDel(set)                genericSetDel(set)
#define mapAdd(set, key, value)    genericSetAdd(set, associationNew(key, (void*)value), hashedValueAssociation)
#define mapGet(set, key)           genericSetGet(set, key, hashedValueAssociation)
#define mapSet(set, key, value)    { genericSetGet(set, key, hashedValueAssociation)->value = value; }
#define mapHas(set, key)           genericSetHas(set, key, hashedValueAssociation)
#define mapRemove(set, key)        genericSetRemove(set, key, hashedValueAssociation)
#define mapCopy(set)               genericSetCopy(set)
#define mapUnion(dest, src)        genericSetUnion(dest, src)
#define mapIntersection(dest, src) genericSetIntersection(dest, src)
#define mapDifference(dest, src)   genericSetDifference(dest, src)

#define strMapNew()                   genericSetNew()
#define strMapDel(set)                genericSetDel(set)
#define strMapAdd(set, key, value)    genericSetAdd(set, associationNew(key, value), hashedStringAssociation)
#define strMapGet(set, key)           genericSetGet(set, key, hashedStringAssociation)
#define strMapSet(set, key, value)    { genericSetGet(set, key, hashedStringAssociation)->value = value; }
#define strMapHas(set, key)           genericSetHas(set, key, hashedStringAssociation)
#define strMapRemove(set, key)        genericSetRemove(set, key, hashedStringAssociation)
#define strMapCopy(set)               genericSetCopy(set)
#define strMapUnion(dest, src)        genericSetUnion(dest, src)
#define strMapIntersection(dest, src) genericSetIntersection(dest, src)
#define strMapDifference(dest, src)   genericSetDifference(dest, src)

GenericSet *genericSetNewSize(Size size);
GenericSet *genericSetNew(); /* Default size 11, does not change */
void genericSetDel(GenericSet *set);
bool _valuesAreEquivalent(void *value1, HashType type1, void *value2, HashType type2);
// value is a value, start of string, or pointer to Association struct
void genericSetAdd(GenericSet *set, void *value, HashType type);
void *genericSetGet(GenericSet *set, void *value, HashType type);
bool genericSetHas(GenericSet *set, void *value, HashType type);
void *genericSetRemove(GenericSet *set, void *value, HashType type);
/* Alter the size of the set copy */
GenericSet *genericSetCopySize(GenericSet *set, Size size);
/* Shallow copy of the set */
GenericSet *genericSetCopy(GenericSet *set);
void genericSetUnion(GenericSet *dest, GenericSet *src);
void genericSetIntersection(GenericSet *dest, GenericSet *src);
void genericSetDifference(GenericSet *dest, GenericSet *src);
void genericSetDump(GenericSet *set);

///////////////////////// **********************************
// ArrayList Interface // ** WARNING: NOT YET IMPLEMENTED **
///////////////////////// **********************************

/* Use for data that ought to keep order or be indexable. */

typedef struct
{
	Size size,
	     capacity;
	void *buckets[0];
} ArrayList;

ArrayList *arrayListNew(); /* Default capacity 4, does expand */
void arrayListAdd(ArrayList *list, void *value);
void arrayListInsert(ArrayList *list, void *value, Size index);
void *arrayListAt(ArrayList *list, Size index);
void *arrayListPop(ArrayList *list, Size index);

/* Note that when searching for items in the ArrayList, whether using Remove or
 * Has, this is an "is-a" relationship, not an equivalence relationship. That
 * means that two identical strings are not the same string if they reside in
 * different places in memory. */

void *arrayListRemove(ArrayList *list, void *value);
bool arrayListHas(ArrayList *list, void *value);

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
// Stack Interface //
/////////////////////

typedef struct
{
    Size topIndex;
    Size capacity;
    void **array;
} Stack;

Stack *stackNew();
void stackPush(Stack *stack, void *value);
void *stackPop(Stack *stack);
Size stackSize(Stack *stack);
void *stackTop(Stack *stack);
void stackDel(Stack *stack);

#endif
