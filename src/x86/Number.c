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
#include <vm.h>
#include <Number.h>
#include <mm.h>

void numbersInstall()
{
    integerProto = object_send(objectProto, symbol("new"));
    Object *integerMT = object_send(objectMT, symbol("new:"), 22);
    integerProto->methodTable = integerMT;
}

Object *integer32_new(s32 value)
{
    Object *new = object_send(integerProto, symbol("new"));
    Number *data = malloc(sizeof(Number) + sizeof(s32));
    new->data = data;
    data->type = integer32;
    data->data[0] = (Size)value;
    return new;
}

Object *integer64_new(s64 value)
{
    Object *new = object_send(integerProto, symbol("new"));
    Number *data = malloc(sizeof(Number) + sizeof(s64));
    new->data = data;
    data->type = integer64;
    data->data[0] = (Size)value;
    return new;
}
