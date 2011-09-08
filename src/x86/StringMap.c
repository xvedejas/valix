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

#include <StringMap.h>
#include <data.h>
#include <mm.h>
#include <cstring.h>

const Size stringMapInitialSize = 5;

/* We begin with bucket A, and bucket B is NULL. When A gets nearly full,
 * we create a new bucket B that is larger. When inserting to the map, we
 * insert to B and then copy an item over from A to B. Once A is empty,
 * A becomes B and B is set to NULL again. This means we insert to B unless it
 * is NULL.
 * 
 * Our threshhold for A will be 75% full. Once A is 75% full we create B to
 * be 200% the size of A (twice as large) expaning the total capacity to 300%
 * the size of A.
 * 
 */

StringMap *stringMapNew()
{
    StringMap *map = malloc(sizeof(StringMap));
    map->sizeA = stringMapInitialSize;
    map->sizeB = 0;
    map->entriesA = 0;
    map->entriesB = 0;
    map->A = calloc(sizeof(StringMapBucket), stringMapInitialSize);
    map->B = NULL;
    return map;
}

Size _stringMapThreshhold(Size size)
{
    return (size >> 1) + (size >> 2); // 75% rounded down
}

Size _stringMapNextSize(Size oldSize)
{
    return oldSize << 1; // 200%
}

void _stringMapTestResize(StringMap *map)
{
    /* Here, we have just added an element to A. B should be NULL. See if we
     * need to create B. */
    
    assert(map->B == NULL, "stringMap error");
    
    if (map->entriesA < _stringMapThreshhold(map->sizeA))
        return;
    
    map->sizeB = _stringMapNextSize(map->sizeA);
    map->entriesB = 0;
    map->B = calloc(sizeof(StringMapBucket), map->sizeB);
}

bool _stringMapSet(StringMap *map, String key, void *value, bool doMove);

void _stringMapCopyFromAToB(StringMap *map)
{
    if (map->entriesA == 0)
    {
        // done moving stuff, get rid of A and move B to A
        assert(map->entriesA == 0, "stringMap error");
        free(map->A);
        map->A = map->B;
        map->entriesA = map->entriesB;
        map->sizeA = map->sizeB;
        map->B = NULL;
        map->entriesB = 0;
        map->sizeB = 0;
        return;
    }

    Size i;
    for (i = 0; map->A[i].key == NULL; i++);
    
    String key = map->A[i].key;
    void *value = map->A[i].value;
    
    StringMapBucket *next = map->A[i].next;
    
    if (next != NULL)
    {
        map->A[i].key = next->key;
        map->A[i].value = next->value;
        map->A[i].next = next->next;
        free(next);
    }
    else
    {
        map->A[i].key = NULL;
        map->A[i].value = NULL;
    }
    map->entriesA--;
    
    assert(!_stringMapSet(map, key, value, false), "StringMap error");
}

/* Returns true if key was already in map (and so value modified) */
bool _stringMapSet(StringMap *map, String key, void *value, bool doMove)
{
    Size hash = stringHash(key);
    StringMapBucket *bucket = &map->A[hash % map->sizeA];
    while (true)
    {
        if (strcmp(bucket->key, key) == 0)
        {
            bucket->value = value;
            return true;
        }
        if (bucket->next == NULL)
            break;
        bucket = bucket->next;
    }
    if (map->B != NULL)
    {
        bucket = &map->B[hash % map->sizeB];
        while (true)
        {
            if (strcmp(bucket->key, key) == 0)
            {
                bucket->value = value;
                return true;
            }
            if (bucket->next == NULL)
                break;
            bucket = bucket->next;
        }
        map->entriesB++;
    }
    else
    {
        map->entriesA++;
        _stringMapTestResize(map);
    }
    
    /* Add the entry */
    
    if (bucket->key != NULL)
    {
        bucket->next = malloc(sizeof(StringMapBucket));
        bucket = bucket->next;
        bucket->next = NULL;
    }
    bucket->key = key;
    bucket->value = value;
    
    if (map->B != NULL && doMove)
        _stringMapCopyFromAToB(map);
    return false;
}

bool stringMapSet(StringMap *map, String key, void *value)
{
    return _stringMapSet(map, key, value, true);
}

void *stringMapGet(StringMap *map, String key)
{
    Size hash = stringHash(key);
    StringMapBucket *bucket = &map->A[hash % map->sizeA];
    
    do
    {
        if (strcmp(bucket->key, key) == 0)
            return bucket->value;
    } while ((bucket = bucket->next) != NULL);
    
    if (map->B != NULL)
    {
        bucket = &map->B[hash % map->sizeB];
        do
        {
            if (strcmp(bucket->key, key) == 0)
                return bucket->value;
        } while ((bucket = bucket->next) != NULL);
    }
    return NULL;
}

void stringMapDebug(StringMap *map)
{
    Size i;
    printf(" ===[A size: %x]=== \n", map->sizeA);
    for (i = 0; i < map->sizeA; i++)
    {
        StringMapBucket *bucket = &map->A[i];
        do
        {
            if (bucket->key != NULL)
                printf("key %s value %x\n", bucket->key, bucket->value);
        }
        while ((bucket = bucket->next) != NULL);
    }
    if (map->B != NULL)
    {
        printf(" ===[B size: %x]=== \n", map->sizeB);
        for (i = 0; i < map->sizeB; i++)
        {
            StringMapBucket *bucket = &map->B[i];
            do
            {
                if (bucket->key != NULL)
                    printf("key %s value %x\n", bucket->key, bucket->value);
            }
            while ((bucket = bucket->next) != NULL);
        }
    }
    printf(" ===[end debug]===\n");
}
