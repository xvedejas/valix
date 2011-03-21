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

///////////////////////////////
// GenericSet Implementation //
///////////////////////////////

/* The GenericSet Implementation forms the backbone of the unordered, hashed
 * abstract datatypes "map" and "set". Allowing both strings and pointers as
 * keys, associations are used to map the keys to values. */

u32 getStringHash(GenericSet *set, String string)
{
    // Algorithm is "Jenkins Hash"
    u32 hash, i;
    for (hash = i = 0; string[i]; i++)
    {
        hash += string[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash % set->tableSize;
}

u32 getValueHash(GenericSet *set, void *value)
{
    return ((Size)value) % set->tableSize;
}

u32 getHash(GenericSet *set, void *value, HashType type)
{
    switch (type)
    {
        case hashedNull:
            panic("Error, null type");
        break;
        case hashedValue:
            return getValueHash(set, value);
        break;
        case hashedString:
            return getStringHash(set, value);
        break;
        case hashedValueAssociation:
            return getValueHash(set, ((Association*)value)->key);
        break;
        case hashedStringAssociation:
            return getStringHash(set, ((Association*)value)->key);
        break;
        default:
            panic("This should never be called");
        break;
    }
    return 0;
}

Association *associationNew(void *key, void *value)
{
    Association *assoc = malloc(sizeof(Association));
    assoc->key = key;
    assoc->value = value;
    return assoc;
}

GenericSet *genericSetNewSize(Size size)
{
    assert(size > 2, "GenericSet hash table cannot be of size less than 3");
    GenericSet *set = calloc(1, sizeof(GenericSet) + size * sizeof(GenericSetItem));
    set->tableSize = size;
    return set;
}

GenericSet *genericSetNew() /* Default size 11 */
{
    return genericSetNewSize(11);
}

void genericSetDel(GenericSet *set)
{
    inline void del(GenericSetItem *item)
    {
        if (item->next != NULL) del(item->next);
        if (item->type == hashedValueAssociation || item->type == hashedStringAssociation)
            free(item->value); /* free the association */
        free(item);
    }
    
    int i;
    for (i = 0; i < set->tableSize; i++)
    {
        GenericSetItem *item = &set->table[i];
        if (item->type == hashedValueAssociation || item->type == hashedStringAssociation)
        {
            free(item->value); /* free the association */
            printf("freed assoc\n");
        }
        if (item->next != NULL)
            del(item->next);
    }
    free(set);
}

bool _valuesAreEquivalent(void *value1, HashType type1, void *value2, HashType type2)
{
    if (type1 != type2)
        return false;
    
    switch (type1)
    {
        case hashedNull:
            panic("GenericSet Error: Null value");
        break;
        case hashedValue:
            return value1 == value2;
        break;
        case hashedString:
            return strcmp(value1, value2) == 0;
        break;
        case hashedValueAssociation:
            return ((Association*)value1)->key == ((Association*)value2)->key;
        break;
        case hashedStringAssociation:
            return strcmp(((Association*)value1)->key, ((Association*)value2)->key) == 0;
        break;
        default:
            panic("This should never be called");
        break;
    }
    return false;
}

// value is a value, start of string, or pointer to Association struct
void genericSetAdd(GenericSet *set, void *value, HashType type)
{
    GenericSetItem *item = &set->table[getHash(set, value, type)];
    if (item->type == hashedNull)
    {
        item->type = type;
        item->value = value;
        return;
    }
    if (unlikely(_valuesAreEquivalent(value, type, item->value, item->type)))
    {
        // If we are given an association and this association is not used,
        // then we should discard the association.
        if (type == hashedValueAssociation || type == hashedStringAssociation)
            free(value);
        return; /* Silently reject duplicates */
    }
    while (item->next != NULL)
    {
        item = item->next;
        if (unlikely(_valuesAreEquivalent(value, type, item->value, item->type)))
        {
            // If we are given an association and this association is not used,
            // then we should discard the association.
            if (type == hashedValueAssociation || type == hashedStringAssociation)
                free(value);
            return; /* Silently reject duplicates */
        }
    } 
    item->next = malloc(sizeof(GenericSetItem));
    item->next->value = value;
    item->next->type = type;
    item->next->next = NULL;
}

void *genericSetGet(GenericSet *set, void *value, HashType type)
{
    GenericSetItem *item = &set->table[getHash(set, value, type)];
    if (item->type == hashedNull)
        return NULL;
    do
    {
        if (_valuesAreEquivalent(value, type, item->value, item->type))
        {
            if (type == hashedValueAssociation || type == hashedStringAssociation)
                return ((Association*)item->value)->value;
            else
                return item->value;
        }
    } while ((item = item->next) != NULL);
    return NULL;
}

bool genericSetHas(GenericSet *set, void *value, HashType type)
{
    return (bool)genericSetGet(set, value, type);
}

/* In an association, returns value only */
void *genericSetRemove(GenericSet *set, void *value, HashType type)
{
    GenericSetItem *item = &set->table[getHash(set, value, type)];
    GenericSetItem *previous = NULL;
    if (item->type == hashedNull)
        return NULL;
    do
    {
        if (_valuesAreEquivalent(value, type, item->value, item->type))
        {
            void *returnValue;
            if (type == hashedValueAssociation || type == hashedStringAssociation)
            {
                returnValue = ((Association*)item->value)->value;
                free((Association*)item->value);
            }
            else
                returnValue = item->value;
            
            if (item->next != NULL)
            {
                item->value = item->next->value;
                item->type = item->next->type;
                GenericSetItem *toFree = item->next;
                item->next = item->next->next;
                free(toFree);
            }
            else
            {
                if (previous == NULL)
                {
                    // Not in chain, in table
                    item->type = hashedNull;
                }
                else
                {
                    free(item);
                    previous->next = NULL;
                }
            }
            return returnValue;
        }
        previous = item;
    } while ((item = item->next) != NULL);
    return NULL;
}

/* Alter the size of the set copy */
GenericSet *genericSetCopySize(GenericSet *set, Size size)
{
    GenericSet *setNew = genericSetNewSize(size);
    int i;
    for (i = 0; i < set->tableSize; i++)
    {
        GenericSetItem *item = &set->table[i];
        do
        {
            if (item->type != hashedNull)
                genericSetAdd(setNew, item->value, item->type);
        }
        while ((item = item->next) != NULL);
    }
    return setNew;
}

/* Shallow copy of the set */
GenericSet *genericSetCopy(GenericSet *set)
{
    return genericSetCopySize(set, set->tableSize);
}


void genericSetUnion(GenericSet *dest, GenericSet *src)
{
    int i;
    for (i = 0; i < src->tableSize; i++)
    {
        GenericSetItem *item = &src->table[i];
        do
        {
            if (item->type != hashedNull)
                genericSetAdd(dest, item->value, item->type);
        }
        while ((item = item->next) != NULL);
    }
}

void genericSetIntersection(GenericSet *dest, GenericSet *src)
{
    int i;
    for (i = 0; i < dest->tableSize; i++)
    {
        GenericSetItem *item = &dest->table[i];
        do
        {
            if (item->type != hashedNull)
            {
                if (!genericSetHas(src, item->value, item->type))
                    genericSetRemove(dest, item->value, item->type);
            }
        }
        while ((item = item->next) != NULL);
    }
}

void genericSetDifference(GenericSet *dest, GenericSet *src)
{
    int i;
    for (i = 0; i < src->tableSize; i++)
    {
        GenericSetItem *item = &src->table[i];
        do
        {
            if (item->type != hashedNull)
                genericSetRemove(dest, item->value, item->type);
        }
        while ((item = item->next) != NULL);
    }
}

void genericSetDebug(GenericSet *set)
{
    int i;
    for (i = 0; i < set->tableSize; i++)
    {
        GenericSetItem *item = &set->table[i];
        
        do
        {
            printf("%i. ", i);
            switch (item->type)
            {
                case hashedNull:
                    printf("Null");
                break;
                case hashedValue:
                    printf("value: %i", item->value);
                break;
                case hashedString:
                    printf("value: %s", item->value);
                break;
                case hashedValueAssociation:
                    printf("key: %i value: %s", ((Association*)item->value)->key, ((Association*)item->value)->value);
                break;
                case hashedStringAssociation:
                    printf("key: %s value: %s", ((Association*)item->value)->key, ((Association*)item->value)->value);
                break;
                default:
                    panic("This should never be called");
                break;
            }
            printf("\n");
        } while ((item = item->next) != NULL);
    }
}

//////////////////////////////////
// StringBuilder Implementation //
//////////////////////////////////

StringBuilder *stringBuilderNew(String initial)
{
    StringBuilder *sb = malloc(sizeof(StringBuilder));
    sb->size = strlen(initial);
    sb->capacity = sb->size * 2;
    sb->s = malloc(sb->capacity * sizeof(char));
    strcpy(sb->s, initial);
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
    if (sb->size + len > sb->capacity)
    {
        sb->capacity = (sb->size + len) * 2;
        sb->s = realloc(sb->s, sizeof(char) * sb->capacity);
    }
    strcat(sb->s, s);
    sb->size += len;
}

void stringBuilderMerge(StringBuilder *sb1, StringBuilder *sb2)
{
    stringBuilderAppend(sb2, "\0");
    stringBuilderAppend(sb1, sb2->s);
    stringBuilderDel(sb2);
}

/* Delete the string builder; get a string result */
String stringBuilderToString(StringBuilder *sb)
{
    stringBuilderAppend(sb, "\0");
    String s = sb->s;
    free(sb);
    return s;
}
