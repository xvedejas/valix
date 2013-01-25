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
#ifndef __mm_h__
#define __mm_h__
#include <main.h>
#include <threading.h>

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

struct multibootStructure
{
    u32 flags;              // 0
    u32 memLower;           // 4
    u32 memUpper;           // 8
    u32 bootDevice;         // 12
    String cmdline;         // 16
    u32 modsCount;          // 20
    u32 *modsAddr;          // 24
    u32 syms[4];            // 28, 32, 36, 40
    u32 mmapLength;         // 44
    MMapField *mmapAddr;    // 48
    u32 drivesLength;       // 52
    u32 *drivesAddr;        // 56
    u32 configTable;        // 60
    String bootLoaderName;  // 64
    u32 apmTable;           // 68
    u32 vbeControlInfo;     // 72
    u32 vbeModeInfo;        // 76
    u16 vbeMode;            // 80
    u16 vbeInterfaceSeg;    // 82
    u16 vbeInterfaceOff;    // 84
    u16 vbeInterfaceLen;    // 86
    u64 framebufferAddr;    // 88
    u32 framebufferPitch;   // 96
    u32 unknown;            // 100
    u32 framebufferWidth;   // 104
    u32 framebufferHeight;  // 108
    u8 framebufferBpp;      // 112
    u8 framebufferType;     // 113
    union
    {
        struct
        {
            u32 framebufferPaletteAddr;
            u16 framebufferPaletteNumColors;
        };
        struct
        {
            u8 framebufferRedFieldPosition;
            u8 framebufferRedMaskSize;
            u8 framebufferGreenFieldPosition;
            u8 framebufferGreenMaskSize;
            u8 framebufferBlueFieldPosition;
            u8 framebufferBlueMaskSize;
        };
    };
} __attribute__((__packed__));

typedef struct multibootStructure MultibootStructure;

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

extern void sweep();
extern Size memUsed();
extern Size memFree();
extern void mmInstall(MultibootStructure *multiboot);

/* malloc() will allocate memory then identify the allocated memory with the
 * current thread. When the thread ends, it will automatically free the memory
 * that it keeps track of. Use kalloc() for memory which has a lifetime not
 * associated with the current thread. */
#define malloc(_size) _kalloc(_size, getCurrentThread(), __FILE__, __LINE__)
#define kalloc(_size, _thread) _kalloc(_size, _thread, __FILE__, __LINE__)
#define free(_mem) _free(_mem, __FILE__, __LINE__)

extern void *_kalloc(Size size, struct thread *thread, char *file, Size line);
extern void *calloc(Size amount, Size elementSize);
extern bool isAllocated(void *memory);
extern void _free(void *memory, char *file, Size line);
extern void *realloc(void *memory, Size size);
extern void meminfo();
extern void coreDump();
extern void freeThread(struct thread *thread);
/* Given a pointer to allocated memory, find the size of the allocated block.
 * Returns zero if given a pointer that does not point to allocated memory. */
extern Size memBlockSize(void *memptr);

#endif
