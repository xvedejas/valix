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
 */
#ifndef __main_h__
#define __main_h__

#define assembly __asm__ __volatile__

#define offsetof(st, m) __builtin_offsetof(st, m)

#define likely(x)       __builtin_expect((x), 1)
#define unlikely(x)     __builtin_expect((x), 0)

#define __release__ true

#define u8max (0xFF)
#define u16max (0xFFFF)
#define u32max (0xFFFFFFFF)
#define u64max (0xFFFFFFFFFFFFFFFF)
#define bit(n) (1 << n)
#define min(x,y) (((x)<(y))?(x):(y))
#define max(x,y) (((x)>(y))?(x):(y))
#define mid(x,y,z) (((x)>(y))?(((y)>(z))?y:((x)>(z)?z:x)):(((y)>(z))?(((x)>(z))?x:z):y))
#define delta(x,y) (max(x,y)-min(x,y))
#define swap(x,y) { typeof(x) a = x; y = a; x = y; }

#define NULL ((void*)0)

#define panic(message, args...) { printf("\n[-PANIC----------]\n");\
    printf(message, ## args); _panic(__FILE__, __LINE__); }
#define assert(boolean, message, args...)\
    { if (unlikely(!(boolean))) {panic(message, ## args);} }

typedef unsigned char  u8;
typedef          char  s8;
typedef unsigned short u16;
typedef          short s16;
typedef unsigned int   u32;
typedef          int   s32;
typedef unsigned long  u64;
typedef          long  s64;
typedef          long long int smax;
typedef unsigned long long int umax;
typedef unsigned int   Size; /* processor word */
typedef enum { false, true } bool;

typedef s8 *String;

volatile umax timerTicks;
const Size systemStackSize;
void timerWait(int ticks);

extern u8 inb(u16 port);
extern void outb(u16 port, u8 data);
extern u16 inw(u16 port);
extern void inws(u16 port, u16 *buffer, Size size);
extern void outw(u16 port, u16 data);
extern void outws(u16 port, u16 *data, Size size);
extern u32 ind(u16 port);
extern void outd(u16 port, u32 data);

extern void printf(const char *format, ...);
extern void putch(u8 c);
extern void _panic(String file, u32 line);

extern void *linkKernelEntry;
extern void *linkKernelEnd;

extern void halt();
extern void reboot();
extern void gdtInstall(void);
extern void timerInstall();
extern void quickRestart();

extern void pciinfo();

bool withinISR;

#endif
