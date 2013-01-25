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
 */
#ifndef __string_h__
#define __string_h__
#include <main.h>

void *memset(void *memory, u8 value, Size number);
void *memsetd(void *memory, u32 value, Size number);
void *memcpy(void *dest, void *src, Size count);
void *memcpyd(void *dest, void *src, Size count);
void *memmove(void *dest, const void *src, Size count);
extern String strldup(String src, Size count);
extern String strcpy(String dest, const String source);
extern String strcat(String dest, const String source);
extern String strdup(String s);
extern Size strlen(String s);
extern u32 strcmp(String cs, String ct);
extern u32 strncmp(String cs, String ct, Size n);
extern Size strlcpy(String dest, String src, Size count);
extern Size chrcount(String s, char c);
extern String strchr(const String s, char c);
extern String strncpy(String dest, const String src, Size count);
extern String strncat(String dest, String src, Size count);

bool startswith(String string, String substring);
bool endswith(String string, String substring);

#endif
