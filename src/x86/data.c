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
#include <data.h>
#include <string.h>
#include <mm.h>
#include <threading.h>

/* For computing hashes is this pre-randomized table... */
u8 lookupTable[256] =
{
    0x78, 0x91, 0xBB, 0xD5, 0x53, 0xE2, 0x1D, 0x75,
    0x15, 0x30, 0xA0, 0x58, 0xEA, 0x33, 0xFE, 0xC2,
    0xA7, 0x6F, 0x7D, 0xB8, 0xEE, 0xD0, 0xFC, 0xF0,
    0xAA, 0xD7, 0x8C, 0x82, 0xDF, 0xEF, 0xE7, 0x42,
    0x5E, 0x4B, 0x77, 0x14, 0x0A, 0x7B, 0x9A, 0x68,
    0x01, 0x5D, 0xCA, 0x20, 0xCD, 0x31, 0xC1, 0x12,
    0x76, 0x7C, 0xFD, 0xA4, 0x83, 0x64, 0x8D, 0xC5,
    0x5F, 0x93, 0xC8, 0x2D, 0x88, 0xF1, 0x60, 0x81,
    0xD6, 0xC7, 0xB6, 0x84, 0x18, 0xA9, 0x10, 0x36,
    0xF7, 0x65, 0xAF, 0xC0, 0x0D, 0x46, 0x89, 0x35,
    0xA3, 0x9E, 0x09, 0xB9, 0xCC, 0xDA, 0x96, 0xF6,
    0x8E, 0xB4, 0xFA, 0xE1, 0xE3, 0xFB, 0xB1, 0x16,
    0xDD, 0x7F, 0x98, 0x79, 0x2C, 0x4D, 0x5A, 0x24,
    0x7A, 0x4F, 0x38, 0xA6, 0x54, 0x70, 0x00, 0xF9,
    0x56, 0xAB, 0xDE, 0x21, 0x32, 0x06, 0xE5, 0xAE,
    0x08, 0x04, 0x7E, 0x3B, 0x4A, 0x3F, 0x2A, 0x6D,
    0xB0, 0x71, 0xE0, 0x8A, 0x48, 0xAC, 0xB0, 0xCB,
    0x4C, 0x27, 0x1F, 0x41, 0xC9, 0x6E, 0xD3, 0xBE,
    0x44, 0xA5, 0x49, 0x22, 0x6C, 0x0E, 0x40, 0xDC,
    0xF3, 0x17, 0x63, 0xDB, 0x3E, 0x86, 0x74, 0xE4,
    0xAD, 0x34, 0x1B, 0x9D, 0x69, 0xE6, 0x52, 0xC4,
    0x8F, 0x1C, 0x97, 0x99, 0x6A, 0x6B, 0x66, 0xF2,
    0xBA, 0xED, 0x92, 0xE8, 0x87, 0x5C, 0x3D, 0x03,
    0xD4, 0xF8, 0x19, 0xD9, 0x13, 0x26, 0xC6, 0x61,
    0x67, 0x80, 0x59, 0xB7, 0x95, 0x2F, 0x50, 0x23,
    0x45, 0x8B, 0xD8, 0x47, 0xFF, 0x5B, 0xCE, 0x51,
    0x37, 0xF4, 0x0F, 0x07, 0xD2, 0x1A, 0x0C, 0xF5,
    0xB5, 0x3C, 0x94, 0x2B, 0x11, 0x3A, 0x39, 0x1E,
    0x85, 0xA2, 0x9F, 0xD1, 0x57, 0x9B, 0x29, 0x73,
    0x02, 0xB3, 0xEB, 0xEC, 0x25, 0xE9, 0xC3, 0xBC,
    0x9C, 0x28, 0xCF, 0x72, 0x55, 0xB2, 0xBD, 0x90,
    0x4E, 0x43, 0x05, 0xBF, 0x2E, 0xA8, 0x62, 0xA1
};

u32 getStringHash(String string)
{
    u32 hash, i;
    for (hash = i = 0; string[i]; i++)
        hash ^= lookupTable[(Size)string[i]];
    return hash;
}

////////////////////////
// Map Implementation //
////////////////////////

void _mapExpand(Map *map);
void _mapDelAssoc(Association *assoc, Association *previous);
void _mapMoveAtoB(Map *map);
bool _mapKeysEquivalent(void *key1, void *key2, MapKeyType type1, MapKeyType type2);
Association *_mapFindBucket(Map *map, void *key, MapKeyType type, bool *inTableA, Association **previous);
void _mapSet(Map *map, void *key, void *value, MapKeyType type, bool incrementallyResize);

// The next size to choose when the map must grow
#define newMapSize(oldSize) ((oldSize << 1) + 1)

void _mapExpand(Map *map)
{
    mutexAcquireLock(map->lock);
    if (map->B == NULL)
    {
        map->capacityB = newMapSize(map->capacityA);
        map->B = calloc(map->capacityB, sizeof(Association));
        map->entriesB = 0;
    }
    else
        panic("Old hashtable not destroyed fast enough, resize of map failed!");
    mutexReleaseLock(map->lock);
}

void _mapDelAssoc(Association *assoc, Association *previous)
{
    assert(previous == NULL || previous->next == assoc, "Map error");
    if (assoc->next != NULL)
    {
        assoc->key = assoc->next->key;
        assoc->value = assoc->next->value;
        assoc->type = assoc->next->type;
        Association *newNext = assoc->next->next;
        free(assoc->next);
        assoc->next = newNext;
        return;
    }
    if (previous == NULL)
    {
        assoc->type = nullKey;
        assoc->next = NULL;
    }
    else
    {
        previous->next = NULL;
        free(assoc);
    }
}

void _mapDeleteTable(Size capacity, Association *table)
{
    Association *assoc;
    Size i;
    for (i = 0; i < capacity; i++)
    {
        assoc = &table[i];
        if (assoc->type == nullKey)
            continue;
        while (assoc->next != NULL)
            _mapDelAssoc(assoc->next, assoc);
    }
    free(table);
}

void _mapMoveAtoB(Map *map)
{
    assert(map->B != NULL, "Map error");
    /* move any one item from hashtable A to hashtable B by finding the first
     * assocation in the table and iterating */
    Association *assoc;
    int i;
    for (i = 0; i < map->capacityA; i++)
    {
        assoc = &map->A[i];
        if (assoc->type != nullKey)
            break;
    }
    void *key = assoc->key,
         *value = assoc->value;
    MapKeyType type = assoc->type;
    // remove the association (from A)
    _mapDelAssoc(assoc, NULL);
    map->entriesA--;
    if (map->entriesA == 0)
    {
        // Here, we are done moving items from A to B. Swap B for A, and delete A.
        free(map->A);
        map->A = map->B;
        map->entriesA = map->entriesB;
        map->capacityA = map->capacityB;
        map->entriesB = 0;
        map->capacityB = 0;
        map->B = NULL;
    }
    // re-add the association
    _mapSet(map, key, value, type, false);
}

bool _mapKeysEquivalent(void *key1, void *key2, MapKeyType type1, MapKeyType type2)
{
    switch (type1)
    {
        case nullKey:
            panic("Map error: null key");
        break;
        case stringKey:
            if (type2 == stringKey)
                return (strcmp(key1, key2) == 0);
        break;
        case valueKey:
            if (type2 == valueKey)
                return key1 == key2;
        break;
        default:
            panic("Map error");
        break;
    }
    return false;
}

/* Return the exact association whose key matches. "hashtable" should be a reference argument */
Association *_mapFindBucket(Map *map, void *key, MapKeyType type, bool *inTableA, Association **previous)
{
    Size hash;
    switch (type)
    {
        case nullKey:
            panic("Map error: null key");
        break;
        case stringKey:
        {
            hash = getStringHash(key);
        } break;
        case valueKey:
        {
            hash = (Size)key;
        } break;
    }
    Size hashA = hash % map->capacityA;
    Association *assoc = &map->A[hashA];
    *previous = NULL;
    do
    {
        if (_mapKeysEquivalent(key, assoc->key, type, assoc->type))
        {
            *inTableA = true;
            if (*previous != NULL) assert((*previous)->next == assoc, "Map error");
            return assoc;
        }
        *previous = assoc;
        assoc = assoc->next;
    } while (assoc != NULL);
    /* Look again, this time in table B if it exists */
    *previous = NULL;
    if (map->B != NULL)
    {
        Size hashB = hash % map->capacityB;
        Association *assoc = &map->B[hashB];
        do
        {
            if (_mapKeysEquivalent(key, assoc->key, type, assoc->type))
            {
                *inTableA = false;
                if (*previous != NULL) assert((*previous)->next == assoc, "Map error");
                return assoc;
            }
            *previous = assoc;
            assoc = assoc->next;
        } while (assoc != NULL);
    }
    return NULL;
}

/* Returns NULL if the key is not found, otherwise returns the value */
void *mapGet(Map *map, void *key, MapKeyType type)
{
    mutexAcquireLock(map->lock);
    // Find the key
    bool inTableA;
    Association *previous;
    Association *assoc = _mapFindBucket(map, key, type, &inTableA, &previous);
    void *value;
    if (assoc == NULL)
        value = NULL;
    else
        value = assoc->value;
    mutexReleaseLock(map->lock);
    return value;
}

/* The incrementally resize variable is needed to avoid resizing all at
 * once due to recursion from calling _mapSet from _mapMoveAtoB */
void _mapSet(Map *map, void *key, void *value, MapKeyType type, bool incrementallyResize)
{
    mutexAcquireLock(map->lock);
    /* Gives the first bucket in the hashtable according to the hash of the key.
     * We want to choose table B if it exists to add to, since it is the larger
     * table available. */
    Size hash;
    switch (type)
    {
        case nullKey:
            panic("Map error: null key");
        break;
        case stringKey:
        {
            hash = getStringHash(key);
        } break;
        case valueKey:
        {
            hash = (Size)key;
        } break;
    }
    Association *assoc;
    if (map->B == NULL)
    {
        hash = hash % map->capacityA;
        assoc = &map->A[hash];
    }
    else
    {
        hash = hash % map->capacityB;
        assoc = &map->B[hash];
    }
    // Find the key
    for (; true; assoc = assoc->next)
    {
        // key already in map
        if (_mapKeysEquivalent(key, assoc->key, type, assoc->type))
        {
            assoc->value = value;
            mutexReleaseLock(map->lock);
            return;
        }
        // key not in map, add
        if (assoc->type == nullKey)
        {
            assoc->key = key;
            assoc->value = value;
            assoc->type = type;
        }
        else if (assoc->next == NULL)
        {
            assoc->next = malloc(sizeof(Association));
            assoc->next->key = key;
            assoc->next->value = value;
            assoc->next->type = type;
            assoc->next->next = NULL;
        }
        else
            continue;
        // See if we added to A or B
        if (map->B == NULL)
        {
            map->entriesA++;
            // Expand when hashtable A is full
            if (map->entriesA > map->capacityA)
            {
                _mapExpand(map);
                _mapMoveAtoB(map);
            }
        }
        else
        {
            map->entriesB++;
            if (incrementallyResize)
                _mapMoveAtoB(map);
        }
        mutexReleaseLock(map->lock);
        return;
    }
    mutexReleaseLock(map->lock);
}

/* Use this function to both add keys to the map or change their value */
void mapSet(Map *map, void *key, void *value, MapKeyType type)
{
    _mapSet(map, key, value, type, true);
}

/* Removes a key; Returns true or false depending on success */
bool mapRemove(Map *map, void *key, MapKeyType type)
{
    mutexAcquireLock(map->lock);
    bool inTableA;
    Association *previous;
    Association *assoc = _mapFindBucket(map, key, type, &inTableA, &previous);
    if (inTableA)
        map->entriesA--;
    else
        map->entriesB--;
    if (assoc == NULL)
    {
        mutexReleaseLock(map->lock);
        return false;
    }
    // delete!
    _mapDelAssoc(assoc, previous);
    mutexReleaseLock(map->lock);
    return true;
}

Map *mapNew()
{
    return mapNewSize(5);
}

Map *mapNewSize(Size startingSize)
{
    Map *map = malloc(sizeof(Map));
    map->entriesA = 0;
    map->entriesB = 0;
    map->capacityA = startingSize;
    map->capacityB = 0;
    map->A = calloc(map->capacityA, sizeof(Association));
    map->B = NULL;
    map->lock = mutexNew();
    return map;
}

/* Deletes the map */
void mapDel(Map *map)
{
    mutexAcquireLock(map->lock);
    _mapDeleteTable(map->capacityA, map->A);
    if (map->B != NULL)
        _mapDeleteTable(map->capacityB, map->B);
    mutexDel(map->lock);
    free(map);
}

void mapDebug(Map *map)
{
    printf("Map debug status:\n");
    printf("Total capacity: %i Total entries: %i\n", map->capacityA + map->capacityB, map->entriesA + map->entriesB);
    printf("  == Hashtable A ==\n");
    Size i;
    for (i = 0; i < map->capacityA; i++)
    {
        Association *assoc = &map->A[i];
        printf("    Bucket %i\n", i);
        do
        {
            if (assoc->type == stringKey)
                printf("       type %i key %s value %x\n", assoc->type, assoc->key, assoc->value);
            else if (assoc->type == valueKey)
                printf("       type %i key %x value %x\n", assoc->type, assoc->key, assoc->value);
        }
        while ((assoc = assoc->next) != NULL);
    }
    printf("  == Hashtable B ==\n");
    for (i = 0; i < map->capacityB; i++)
    {
        Association *assoc = &map->B[i];
        printf("    Bucket %i\n", i);
        do
        {
            if (assoc->type == stringKey)
                printf("       type %i key %s value %x\n", assoc->type, assoc->key, assoc->value);
            else if (assoc->type == valueKey)
                printf("       type %i key %x value %x\n", assoc->type, assoc->key, assoc->value);
        }
        while ((assoc = assoc->next) != NULL);
    }
}

////////////////////////////////
// InternTable Implementation //
////////////////////////////////

InternTable *internTableNew()
{
    InternTable *table = malloc(sizeof(InternTable));
    table->count = 0;
    table->entries = mapNew();
    return table;
}

Size internString(InternTable *table, String string)
{
    Size value = (Size)mapGet(table->entries, string, stringKey);
    if (value == 0)
    {
        table->count++;
        mapSet(table->entries, string, (void*)table->count, stringKey);
        value = table->count;
    }
    return value - 1;
}

void internTableDel(InternTable *table)
{
    mapDel(table->entries);
    free(table);
}

bool isStringInterned(InternTable *table, String string)
{
    return (bool)mapGet(table->entries, string, stringKey);
}

//////////////////////////
// Stack Implementation //
//////////////////////////

// Grow by 200% each time
#define stackNextSize(currentSize) (currentSize + (currentSize << 1))

Stack *stackNew()
{
    Stack *stack = malloc(sizeof(Stack));
    stack->capacity = 8;
    stack->bottom = malloc(sizeof(void*) * stack->capacity);
    stack->entries = 0;
    stack->lock = mutexNew();
    return stack;
}

void stackPush(Stack *stack, void *element)
{
    mutexAcquireLock(stack->lock);
    stack->bottom[stack->entries] = element;
    stack->entries++;
    if (stack->entries >= stack->capacity) // embiggen, it's full
    {
        stack->capacity = stackNextSize(stack->capacity);
        stack->bottom = realloc(stack->bottom, stack->capacity);
    }
    mutexReleaseLock(stack->lock);
}

void *stackPop(Stack *stack)
{
    mutexAcquireLock(stack->lock);
    if (stack->entries == 0) // empty
        return NULL;
    stack->entries--;
    void *value = stack->bottom[stack->entries];
    mutexReleaseLock(stack->lock);
    return value;
}

void stackDel(Stack *stack)
{
    mutexAcquireLock(stack->lock);
    free(stack->bottom);
    free(stack->lock);
    free(stack);
}

//////////////////////////////////
// StringBuilder Implementation //
//////////////////////////////////

StringBuilder *stringBuilderNew(String initial)
{
    StringBuilder *sb = malloc(sizeof(StringBuilder));
    if (initial == NULL)
    {
        sb->size = 0;
        sb->capacity = 4;
    }
    else
    {
        sb->size = strlen(initial);
        sb->capacity = sb->size * 2;
    }
    sb->s = malloc(sb->capacity * sizeof(char));
    if (initial != NULL)
        memcpy(sb->s, initial, sb->size);
    return sb;
}

void stringBuilderDel(StringBuilder *sb)
{
    free(sb->s);
    free(sb);
}

void stringBuilderAppend(StringBuilder *sb, String s)
{
    Size len = strlen(s);
    if (sb->size + len >= sb->capacity)
    {
        sb->capacity = (sb->size + len) * 2;
        sb->s = realloc(sb->s, sizeof(char) * sb->capacity);
    }
    memcpy(sb->s + sb->size, s, len);
    sb->size += len;
}

void stringBuilderAppendChar(StringBuilder *sb, char c)
{
    if (sb->size + 1 >= sb->capacity)
    {
        sb->capacity = (sb->size + 1) * 2;
        sb->s = realloc(sb->s, sizeof(char) * sb->capacity);
    }
    sb->s[sb->size++] = c;
}

void stringBuilderAppendN(StringBuilder *sb, String s, Size len)
{
    if (sb->size + len >= sb->capacity)
    {
        sb->capacity = (sb->size + len) * 2;
        sb->s = realloc(sb->s, sizeof(char) * sb->capacity);
    }
    memcpy(sb->s + sb->size, s, len);
    sb->size += len;
}

/* Delete the string builder; get a string result */
String stringBuilderToString(StringBuilder *sb)
{
    sb->s[sb->size++] = '\0';
    String s = sb->s;
    free(sb);
    return s;
}

//////////////////////////
// Queue Implementation //
//////////////////////////

extern Queue *queueNew(Size size)
{
    Queue *queue = malloc(sizeof(Queue));
    queue->size = size;
    queue->array = malloc(sizeof(void*) * queue->size);
    queue->start = 0;
    queue->end = 0;
    return queue;
}

extern void enqueue(Queue *queue, void *value)
{
    if ((queue->end + 1) % queue->size == queue->start) // overflow
        queue->start = (queue->start + 1) % queue->size;
    queue->array[queue->end] = value;
    queue->end = (queue->end + 1) % queue->size;
}

extern void *dequeue(Queue *queue)
{
    if (queue->end == queue->start) // underflow
        return NULL;
    queue->end = (queue->end - 1) % queue->size;
    return queue->array[queue->end];
}
