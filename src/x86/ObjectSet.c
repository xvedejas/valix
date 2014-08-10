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
 
#include <main.h>
#include <ObjectSet.h>
#include <mm.h>
#include <vm.h>

/* We begin with table A, and table B is NULL. When A gets nearly full (~75%),
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

ObjectSet *objectSetNew(Size initialSize)
{
    ObjectSet *set = malloc(sizeof(ObjectSet));
    sweep();
    set->sizeA = max(initialSize, objectSetInitialSize);
    set->sizeB = 0;
    set->entriesA = 0;
    set->entriesB = 0;
    set->A = calloc(sizeof(ObjectSetBucket), set->sizeA);
    set->B = NULL;
    return set;
}

/* This is the threshhold at which we need to start expanding the hash table */
Size _objectSetThreshhold(Size size)
{
    return (size >> 1) + (size >> 2); // 75% rounded down
}

/* This is the next size of hashtable to create */
Size _objectSetNextSize(Size oldSize)
{
    return oldSize << 1; // 200%
}

void _objectSetTestResize(ObjectSet *set)
{
    /* This is called after adding an element to A when B is NULL. See if we
     * need to create a new B because we've exceeded the threshhold */
    
    assert(set->B == NULL, "ObjectSet error");
    
    if (set->entriesA < _objectSetThreshhold(set->sizeA))
        return;
    
    set->sizeB = _objectSetNextSize(set->sizeA);
    set->entriesB = 0;
    set->B = calloc(sizeof(ObjectSetBucket), set->sizeB);
}

// (Forward declaration)
bool _objectSetAdd(ObjectSet *set, Object *obj, bool doMove);

/* Copies one object from table A to table B. If there are no more objects in
 * table A, then it deallocates table A, replacing it with table B */
void _objectSetCopyFromAToB(ObjectSet *set)
{
    if (set->entriesA == 0)
    {
        // done moving stuff, get rid of A and move B to A
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
        free(next); /// there is a problem here, "next" is sometimes 0x1...
        /// requires more debugging
    }
    else
    {
        set->A[i].key = NULL;
    }
    set->entriesA--;
    
    assert(!_objectSetAdd(set, obj, false), "Stringset error");
}

extern MemoryHeader *firstUsedBlock; /// to remove later

/* Returns true if already in the set */
bool _objectSetAdd(ObjectSet *set, Object *obj, bool doMove)
{
    /// this function seems to be writing outside of allocated memory
    /// after it is called 12 times...
    Size hash = valueHash(obj);
    ObjectSetBucket *bucket = &set->A[hash % set->sizeA];
    while (true)
    {
        if (bucket->key == obj)
            return true; // already here, no need to add
        if (bucket->next == NULL)
            break;
        bucket = bucket->next;
    }
    /* At end of chain in A (bucket->next is NULL) */
    
    /* But if we have a B table, we want to put it there */
    if (set->B != NULL)
    {
        bucket = &set->B[hash % set->sizeB];
        while (true)
        {
            if (bucket->key == obj)
                return true; // already here, no need to add
            if (bucket->next == NULL)
                break;
            bucket = bucket->next;
        }
        /* At end of chain in B (bucket->next is NULL). Adding to B. */
        set->entriesB++;
    }
    else
    {
        set->entriesA++;
        _objectSetTestResize(set);
    }
    
    /* Add the entry. "bucket" might be in either table. */
    assert(firstUsedBlock->previous == NULL, "Sweep failed");
    /* If the bucket has no key, we're at the first element in that bucket.
     * If the bucket does have a key, we need to allocate a new element. */
    if (bucket->key != NULL)
    {
        bucket->next = malloc(sizeof(ObjectSetBucket));
        bucket = bucket->next;
        bucket->next = NULL;
    }
    assert(firstUsedBlock->previous == NULL, "Sweep failed");
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
    printf("  ObjectSet at %x\n", set);
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
