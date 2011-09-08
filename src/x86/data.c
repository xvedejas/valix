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
 
#include <data.h>
#include <mm.h>
#include <string.h>
#include <types.h>
#include <main.h>

Size nextSize(Size size)
{
    return (size + (size >> 1) + 1);
}

Size valueHash(void *value)
{
    return (Size)value >> 2;
}

Size stringHash(String s)
{
    /* "Jenkin's Hash" */
    u32 hash, i;
    Size len = strlen(s);
    for (hash = i = 0; i < len; i++)
    {
        hash += s[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
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
        sb->capacity = nextSize(sb->size);
    }
    sb->s = malloc(sizeof(char) * sb->capacity);
    if (initial != NULL)
        memcpy(sb->s, initial, sb->size);
    return sb;
}

void stringBuilderDel(StringBuilder *sb)
{
    free(sb->s);
    free(sb);
}

void stringBuilderAppendN(StringBuilder *sb, String s, Size len)
{
    Size size = sb->size;
    sb->size += len;
    if (sb->size > sb->capacity)
    {
        do sb->capacity = nextSize(sb->capacity);
        while (sb->size > sb->capacity);
        sb->s = realloc(sb->s, sb->capacity);
    }
    memcpy(sb->s + size, s, len);
}

void stringBuilderAppend(StringBuilder *sb, String s)
{
    Size len = strlen(s);
    stringBuilderAppendN(sb, s, len);
}

void stringBuilderAppendChar(StringBuilder *sb, char c)
{
    stringBuilderAppendN(sb, &c, 1);
}

String stringBuilderToString(StringBuilder *sb)
{
    stringBuilderAppendN(sb, "\0", 1);
    String s = sb->s;
    free(sb);
    return s;
}

void stringBuilderPrint(StringBuilder *sb)
{
    Size i;
    for (i = 0; i < sb->size; i++)
        putch(sb->s[i]);
}

////////////////////////////////
// InternTable Implementation //
////////////////////////////////

/* The intern table returns a number (0, 1, 2, 3...) for each unique string
 * given. The same number is always returned for the same string. The limit is
 * the processor word size. */

InternTable *internTableNew()
{
    InternTable *table = malloc(sizeof(InternTable));
    table->count = 0;
    table->capacity = 16;
    table->table = malloc(sizeof(String*) * table->capacity);
    return table;
}

Size internString(InternTable *table, String string)
{
    Size i;
    for (i = 0; i < table->count; i++)
    {
        if (strcmp(string, table->table[i]) == 0)
            return i;
    }
    if (i >= table->capacity)
    {
        table->capacity = nextSize(table->capacity);
        table->table = realloc(table->table, table->capacity);
    }
    assert(i == table->count, "intern error");
    table->table[i] = string;
    table->count++;
    return i;
}

void internTableDel(InternTable *table)
{
    free(table->table);
    free(table);
}

bool isStringInterned(InternTable *table, String string)
{
    Size i;
    for (i = 0; i < table->count; i++)
    {
        if (strcmp(string, table->table[i]) == 0)
            return true;
    }
    return false;
}
