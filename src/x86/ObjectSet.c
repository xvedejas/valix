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
 
#include <main.h>
#include <ObjectSet.h>
#include <mm.h>
#include <vm.h>

/* We begin with bucket A, and bucket B is NULL. When A gets nearly full,
 * we create a new bucket B that is larger. When inserting to the Set, we
 * insert to B and then copy an item over from A to B. Once A is empty,
 * A becomes B and B is set to NULL again. This means we insert to B unless it
 * is NULL.
 * 
 * Our threshhold for A will be 75% full. Once A is 75% full we create B to
 * be 200% the size of A (twice as large) expaning the total capacity to 300%
 * the size of A.
 * 
 */

const Size objectSetInitialSize = 5;

ObjectSet *objectSetNew()
{
    ObjectSet *set = malloc(sizeof(ObjectSet));
    set->sizeA = objectSetInitialSize;
    set->sizeB = 0;
    set->entriesA = 0;
    set->entriesB = 0;
    set->A = calloc(sizeof(ObjectSetBucket), objectSetInitialSize);
    set->B = NULL;
    return set;
}

Size _objectSetThreshhold(Size size)
{
    return (size >> 1) + (size >> 2); // 75% rounded down
}

Size _objectSetNextSize(Size oldSize)
{
    return oldSize << 1; // 200%
}

void _objectSetTestResize(ObjectSet *set)
{
    /* Here, we have just added an element to A. B should be NULL. See if we
     * need to create B. */
    
    assert(set->B == NULL, "ObjectSet error");
    
    if (set->entriesA < _objectSetThreshhold(set->sizeA))
        return;
    
    set->sizeB = _objectSetNextSize(set->sizeA);
    set->entriesB = 0;
    set->B = calloc(sizeof(ObjectSetBucket), set->sizeB);
}

bool _objectSetAdd(ObjectSet *set, Object *obj, bool doMove);

void _objectSetCopyFromAToB(ObjectSet *set)
{
    if (set->entriesA == 0)
    {
        // done moving stuff, get rid of A and move B to A
        assert(set->entriesA == 0, "ObjectSet error");
        free(set->A);
        set->A = set->B;
        set->entriesA = set->entriesB;
        set->sizeA = set->sizeB;
        set->B = NULL;
        set->entriesB = 0;
        set->sizeB = 0;
        return;
    }

    Size i;
    for (i = 0; set->A[i].key == NULL; i++);
    
    Object *obj = set->A[i].key;
    
    ObjectSetBucket *next = set->A[i].next;
    
    if (next != NULL)
    {
        set->A[i].key = next->key;
        set->A[i].next = next->next;
        free(next);
    }
    else
    {
        set->A[i].key = NULL;
    }
    set->entriesA--;
    
    assert(!_objectSetAdd(set, obj, false), "Stringset error");
}

/* Returns true if already in the set */
bool _objectSetAdd(ObjectSet *set, Object *obj, bool doMove)
{
    Size hash = valueHash(obj);
    ObjectSetBucket *bucket = &set->A[hash % set->sizeA];
    while (true)
    {
        if (bucket->key == obj)
            return true;
        if (bucket->next == NULL)
            break;
        bucket = bucket->next;
    }
    if (set->B != NULL)
    {
        bucket = &set->B[hash % set->sizeB];
        while (true)
        {
            if (bucket->key == obj)
                return true;
            if (bucket->next == NULL)
                break;
            bucket = bucket->next;
        }
        set->entriesB++;
    }
    else
    {
        set->entriesA++;
        _objectSetTestResize(set);
    }
    
    /* Add the entry */
    
    if (bucket->key != NULL)
    {
        bucket->next = malloc(sizeof(ObjectSetBucket));
        bucket = bucket->next;
        bucket->next = NULL;
    }
    bucket->key = obj;
    
    if (set->B != NULL && doMove)
        _objectSetCopyFromAToB(set);
    return false;
}

bool objectSetAdd(ObjectSet *set, Object *obj)
{
    return _objectSetAdd(set, obj, true);
}

/* Returns true if successful, false if the object wasn't in the set */
bool objectSetRemove(ObjectSet *set, Object *obj)
{
    Size hash = valueHash(obj);
    ObjectSetBucket *bucket = &set->A[hash % set->sizeA];
    ObjectSetBucket *previous = NULL;
    while (true)
    {
        if (bucket->key == obj)
        {
            ObjectSetBucket *next = bucket->next;
            if (next != NULL)
            {
                bucket->key = next->key;
                bucket->next = next->next;
                free(next);
            }
            else if (previous == NULL)
            {
                bucket->key = NULL;
            }
            else
            {
                previous->next = bucket->next;
                free(bucket);
            }
            return true;
        }
        if (bucket->next == NULL)
            break;
        previous = bucket;
        bucket = bucket->next;
    }
    if (set->B != NULL)
    {
        bucket = &set->B[hash % set->sizeB];
        ObjectSetBucket *previous = NULL;
        while (true)
        {
            if (bucket->key == obj)
            {
                ObjectSetBucket *next = bucket->next;
                if (next != NULL)
                {
                    bucket->key = next->key;
                    bucket->next = next->next;
                    free(next);
                }
                else if (previous == NULL)
                {
                    bucket->key = NULL;
                }
                else
                {
                    previous->next = bucket->next;
                    free(bucket);
                }
                return true;
            }
            if (bucket->next == NULL)
                break;
            previous = bucket;
            bucket = bucket->next;
        }
    }
    return false;
}

bool objectSetHas(ObjectSet *set, Object *obj)
{
    Size hash = valueHash(obj);
    ObjectSetBucket *bucket = &set->A[hash % set->sizeA];
    while (true)
    {
        if (bucket->key == obj)
            return true;
        if (bucket->next == NULL)
            break;
        bucket = bucket->next;
    }
    if (set->B != NULL)
    {
        bucket = &set->B[hash % set->sizeB];
        while (true)
        {
            if (bucket->key == obj)
                return true;
            if (bucket->next == NULL)
                break;
            bucket = bucket->next;
        }
    }
    return false;
}

void objectSetDebug(ObjectSet *set)
{
    Size i;
    printf(" ===[A size: %x]=== \n", set->sizeA);
    for (i = 0; i < set->sizeA; i++)
    {
        ObjectSetBucket *bucket = &set->A[i];
        do
        {
            if (bucket->key != NULL)
                printf("entry %x\n", bucket->key);
        }
        while ((bucket = bucket->next) != NULL);
    }
    if (set->B != NULL)
    {
        printf(" ===[B size: %x]=== \n", set->sizeB);
        for (i = 0; i < set->sizeB; i++)
        {
            ObjectSetBucket *bucket = &set->B[i];
            do
            {
                if (bucket->key != NULL)
                    printf("entry %x\n", bucket->key);
            }
            while ((bucket = bucket->next) != NULL);
        }
    }
    printf(" ===[end debug]===\n");
}