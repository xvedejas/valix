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
 
#include <data.h>
#include <mm.h>
#include <cstring.h>
#include <types.h>
#include <main.h>
#include <math.h>

Size nextSize(Size size)
{
    return (size + (size >> 1) + 1);
}

Size valueHash(void *value)
{
    /* This seems to work fine enough for pointers, but we probably want to find
     * a hashing method that distributes results more evenly across 32 bits */
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

//////////////////////////
// Stack Implementation //
//////////////////////////

// FILO data structure

const Size stackInitialCapacity = 4;

Stack *stackNew()
{
	Stack *stack = malloc(sizeof(Stack));
	stack->size = 0;
	stack->capacity = stackInitialCapacity;
	stack->array = malloc(sizeof(void*) * stackInitialCapacity);
	return stack;
}

void stackPush(Stack *stack, void *value)
{
	if (stack->size >= stack->capacity)
	{
		stack->capacity = nextSize(stack->capacity);
		stack->array = realloc(stack->array, stack->capacity);
	}
	stack->array[stack->size++] = value;
}

void *stackPop(Stack *stack)
{
	/// todo: shrink the stack if the size is significantly lower than capacity
	return stack->array[--stack->size];
}

void stackDel(Stack *stack)
{
	free(stack->array);
	free(stack);
}

////////////////////////////////
// InternTable Implementation //
////////////////////////////////

/* The intern table returns a number (0, 1, 2, 3...) for each unique string
 * given. The same number is always returned for the same string. */

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

/////////////////////
// Array Functions //
/////////////////////

u32 maxItem(u32 *array, Size n)
{
    register u32 v = 0;
    Size i = n;
    while (i --> 0)
        v = max(v, array[i]);
    return v;
}

void _doRadixSort(u32 *array, Size n, Size digit)
{
    while (true)
    {
        Size b = bit(digit);
        Size i = 0, j = 0;
        for (; j < n; j++)
        {
            u32 val = array[j];
            if (!(val & b))
            {
                Size v = array[i];
                array[i] = val;
                array[j] = v;
                i++;
            }
        }
        if (digit == 0) return;
        digit -= 1;
        if (i > n - i)
        {
            _doRadixSort(array + i, n - i, digit);
            n = i;
        }
        else
        {
            _doRadixSort(array, i, digit);
            array += i;
            n -= i;
        }
    }
}

void radixSort(u32 *array, Size n)
{
    u32 maxDigit = floorlog2(maxItem(array, n)); // position of highest set bit
    _doRadixSort(array, n, maxDigit);
}

void insertSort(u32 *array, Size n)
{
    Size i, j;
    for (i = 0; i < n; i++)
    {
        u32 val = array[i];
        for (j = 0; j < i; j++)
        {
            u32 *index = array + j;
            if (val < *index)
            {
                memmove(index + 1, index, (i - j) * sizeof(u32));
                *index = val;
                break;
            }
        }
    }
}

void quickSort(u32 *array, Size n)
{
    while (true)
    {
        if (n < 2)
            return;
        else if (n > 8)
        {
            u32 pivot = mid(array[0], array[n >> 1], array[n - 1]);
            Size i = 0, j = 0;
            for (; j < n; j++)
            {
                u32 val = array[j];
                if (val <= pivot)
                {
                    Size v = array[i];
                    array[i] = val;
                    array[j] = v;
                    i++;
                }
            }
            
            // recurse into the smaller partition, tail-call into larger
            if (i > n - i)
            {
                quickSort(array + i, n - i);
                n = i;
            }
            else
            {
                quickSort(array, i);
                array += i;
                n -= i;
            }
        }
        /* if the number of elements is small, use insertion sort instead of
         * quick sort */
        else
        {
            insertSort(array, n);
            return;
        }
    }
}
