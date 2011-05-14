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
 */
#ifndef __mm_h__
#define __mm_h__
#include <main.h>

struct thread;

typedef struct
{
    u32 size; // this structure size
    u32 baseAddr;
    u32 baseAddrHigh;
    u32 length; // memory length
    u32 lengthHigh;
    u32 type;   // if (type == 1), ram is free
} MMapField;

typedef struct
{
    u32 flags;            // 0
    u32 memLower;        // 4
    u32 memUpper;        // 8
    u32 bootDevice;      // 12
    String cmdline;          // 16
    u32 modsCount;       // 20
    u32 *modsAddr;       // 24
    u32 syms[4];          // 28, 32, 36, 40
    u32 mmapLength;      // 44
    MMapField *mmapAddr; // 48
    u32 drivesLength;    // 52
    u32 *drivesAddr;     // 56
    u32 configTable;     // 60
    String bootLoaderName; // 64
    u32 apmTable;           // 68
    u32 vbeControlInfo;    // 72
    u32 vbeModeInfo;       // 76
    u16 vbeMode;            // 80
    u16 vbeInterfaceSeg;   // 82
    u16 vbeInterfaceOff;   // 84
    u16 vbeInterfaceLen;   // 86
    u64 framebufferAddr;    // 88
    u32 framebufferPitch;   // 96
    u32 framebufferWidth;   // 100
    u32 framebufferHeight;  // 104
    u8 framebufferBpp;      // 108
    u8 framebufferType;     // 109
    u8 colorInfo[6];        // 110, 111, 112, 113, 114, 115
} MultibootStructure;

typedef struct memoryHeader
{
    u32 startMagic;
    Size size;
    struct thread *thread;
    bool free;
    struct memoryHeader *previous, *next;
    u32 endMagic;
    Size start[0]; // use &start to get the memory directly after the header
} MemoryHeader;

#ifdef __release
#define sweep() _sweep();
#else
#define sweep()
#endif

extern void _sweep();
extern Size memUsed();
extern Size memFree();
extern void mmInstall(MultibootStructure *multiboot);
extern void *malloc(Size size);
extern void *kalloc(Size size, struct thread *thread);
extern void *calloc(Size amount, Size elementSize);
extern void free(void *memory);
extern void *realloc(void *memory, Size size);
extern void meminfo();
extern void coreDump();
extern void freeThread(struct thread *thread);

#endif
