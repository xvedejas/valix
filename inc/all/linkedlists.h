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
#ifndef __ll_h__
#define __ll_h__

// simple linked list utilities
//
// struct item
// {
//    struct item *next;
// };

#define ll_append(firstItem, newItem)                         \
{                                                             \
    if (firstItem == NULL)                                    \
        firstItem = newItem;                                  \
    else                                                      \
    {                                                         \
        assert((newItem)->next == NULL, "linked list error"); \
        typeof(firstItem) _item = (firstItem);                \
        for (; _item->next != NULL; _item = _item->next) {}   \
        _item->next = (newItem);                              \
    }                                                         \
}    
                                                     
#define ll_append_new(firstItem, newItemType)                     \
({                                                                \
    typeof(firstItem) newItem = calloc(1, sizeof(newItemType));   \
    if (firstItem == NULL)                                        \
        firstItem = newItem;                                      \
    else                                                          \
    {                                                             \
        assert((newItem)->next == NULL, "linked list error");     \
        typeof(firstItem) _item = (firstItem);                    \
        for (; _item->next != NULL; _item = _item->next) {}       \
        _item->next = (newItem);                                  \
    }                                                             \
    newItem;                                                      \
})                                                         

// Remove all items for which (item condition) holds

#define ll_remove(firstItem, condition, once)             \
{                                                         \
    typeof(firstItem) _item = firstItem;                  \
    typeof(firstItem) _previous = NULL;                   \
    for (; _item != NULL; _item = _item->next)            \
    {                                                     \
        if ((_item condition) && _previous != NULL)       \
        {                                                 \
            _previous->next = _item->next;                \
            if (once) break;                              \
        }                                                 \
        else                                              \
            _previous = _item;                            \
    }                                                     \
}                                                         

#define ll_find(firstItem, condition)                     \
({                                                        \
    typeof(firstItem) _item = firstItem;                  \
    typeof(firstItem) _result = NULL;                     \
    for (; _item != NULL; _item = _item->next)            \
    {                                                     \
        if (_item condition)                              \
        {                                                 \
            _result = _item;                              \
            break;                                        \
        }                                                 \
    }                                                     \
    _result;                                              \
})

#define ll_find2(firstItem, condition1, condition2)       \
({                                                        \
    typeof(firstItem) _item = firstItem;                  \
    typeof(firstItem) _result = NULL;                     \
    for (; _item != NULL; _item = _item->next)            \
    {                                                     \
        if ((_item condition1) && (_item condition2))     \
        {                                                 \
            _result = _item;                              \
            break;                                        \
        }                                                 \
    }                                                     \
    _result;                                              \
})

#define ll_map(firstItem, function)                       \
{                                                         \
    typeof(firstItem) _item = firstItem;                  \
    for (; _item != NULL; _item = _item->next)            \
        function(_item);                                  \
}                                                         

#define ll_all(firstItem, function)                       \
({                                                        \
    typeof(firstItem) _item = firstItem;                  \
    bool _truth = true;                                   \
    for (; _item != NULL; _item = _item->next)            \
    {                                                     \
        if (!function(_item))                             \
        {                                                 \
            _truth = false;                               \
            break;                                        \
        }                                                 \
    }                                                     \
    _truth;                                               \
})          

// call free() on each item in the linked list
#define ll_free(firstItem)                                \
{                                                         \
    typeof(firstItem) _item = firstItem;                  \
    typeof(firstItem) _next;                              \
    for (; _item != NULL; _item = _next)                  \
    {                                                     \
        _next = _item->next                               \
        free(_item);                                      \
    }                                                     \
}

#endif // __ll_h__
