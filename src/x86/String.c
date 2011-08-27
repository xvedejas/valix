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
 
#include <String.h>
#include <main.h>
#include <vm.h>
#include <string.h>
#include <mm.h>
 
void stringConcat(Object *process)
{
    Object *s2 = pop();
    Object *s1 = pop();
    Size size = s1->string->len + s2->string->len;
    String s = malloc(sizeof(char) * size);
    memcpy(s, s1, s1->string->len);
    memcpy(s + s1->string->len, s2, s2->string->len);
    push(stringNew(size, s));
}

void stringAt(Object *process)
{
    Object *self = pop();
    Object *index = pop();
    push(charNew(self->string->string[index->number->data[0]]));
}

void stringLen(Object *process)
{
    Object *self = pop();
    push(integerNew(self->string->len));
}
 
void stringSetup()
{
    stringProto = objectNew();
    
    methodList stringEntries =
    {
        "asString", yourself,
        "+", stringConcat,
        "at:", stringAt,
        "len", stringLen,
    };
    
    setInternalMethods(stringProto, 4, stringEntries);
}
