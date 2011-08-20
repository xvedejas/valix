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
#include <mm.h>
#include <string.h>
#include <threading.h>

const u32 mmMagic = 0x9001DEAD;
MemoryHeader *firstFreeBlock = NULL;
MemoryHeader *firstUsedBlock = NULL;
bool mmInstalled = false;
Mutex mmLockMutex;

/* This function finds what memory we have available and allocates it as free blocks */
void mmInstall(MultibootStructure *multiboot)
{
    // Make sure we have mmap* fields
    assert(multiboot->flags & bit(6), "No memory found!");
    
    // Fix offset of mmap struct
    MMapField *mmap = multiboot->mmapAddr;

    // Keep track of the last free block found
    MemoryHeader *previousFreeBlock = NULL;

    // Find the addresses that the kernel starts at and ends at
    Size kernelStart = (Size)&linkKernelEntry;
    Size kernelEnd = (Size)&linkKernelEnd;

    void addFreeBlock(Size base, Size length)
    {   
        if (unlikely((Size)base < 0x100000)) 
            return;
        
        if (unlikely(length < sizeof(MemoryHeader) + sizeof(char)))
            return;

        MemoryHeader *header = (MemoryHeader*)base;
        header->size         = length - sizeof(MemoryHeader);
        header->startMagic   = mmMagic;
        header->next         = NULL;
        header->previous     = NULL;
        header->free         = true;
        header->endMagic     = mmMagic;

        // Add the block header to the list
        if (firstFreeBlock == NULL)
            firstFreeBlock = header;
        else
            previousFreeBlock->next = header;

        previousFreeBlock = header;
    }

    while (mmap - multiboot->mmapAddr < multiboot->mmapLength)
    {
        assert(mmap != NULL, "Memory check fail");
        if (mmap->type == 1) // free block
        {
            u32 mmap_end = mmap->baseAddr + mmap->length;

            if (mmap->baseAddr >= kernelStart && mmap_end <= kernelEnd)
                continue; /* kernel completely covers block */
            else if (mmap->baseAddr >= kernelStart && mmap->baseAddr <= kernelEnd)
                /* kernel partially covers block at beginning of block */
                addFreeBlock(kernelEnd, mmap->length + mmap->baseAddr - kernelEnd);
            else if (mmap_end >= kernelStart && mmap_end <= kernelEnd)
                /* kernel partially covers block at end of block */
                addFreeBlock(mmap->baseAddr, kernelStart - mmap->baseAddr);
            else if (kernelStart >= mmap->baseAddr && kernelEnd <= mmap_end)
            {   /* kernel is within the block */
                addFreeBlock(mmap->baseAddr, kernelStart - mmap->baseAddr);
                addFreeBlock(kernelEnd, mmap_end - kernelEnd);
            }
            else /* everything is alright */
                addFreeBlock(mmap->baseAddr, mmap->length);
        }
        mmap = (MMapField*)((Size)mmap + mmap->size + sizeof(mmap->size));
    }
    firstUsedBlock = NULL;
    mmInstalled = true;
    mmLockMutex.multiplicity = 0;
    mmLockMutex.locked = false;
    mmLockMutex.threadsWaiting = NULL;
}

/* Memory is kept track of in two linked lists, one for free blocks and the other
 * for used blocks. The following functions allow us to add or remove blocks from
 * the linked lists safely. */
inline void removeFromFreeList(MemoryHeader *block)
{
    assert(block->free, "MM Fatal Error");

    if (block->previous == NULL)
    {   
        if (unlikely(firstFreeBlock != block))
            panic("MM Fatal Error");
        
        firstFreeBlock = block->next;
        firstFreeBlock->previous = NULL;
    }
    else
    {   
        if (unlikely(firstFreeBlock == block))
            panic("MM Fatal Error");
        
        block->previous->next = block->next;
        block->next->previous = block->previous;
    }
    block->next = NULL;
    block->previous = NULL;
}

inline void addToFreeList(MemoryHeader *block)
{
    assert(block->free, "MM Fatal Error");
    
    // make sure both block->previous and next are null before adding
    assert(block->previous == NULL, "MM Fatal Error");
    assert(block->next == NULL, "MM Fatal Error");
    
    // insert at the beginning
    if (likely(firstFreeBlock != NULL))
    {   
        block->next = firstFreeBlock;
        firstFreeBlock->previous = block;
    }
    firstFreeBlock = block;
}

inline void removeFromUsedList(MemoryHeader *block)
{
    /* This assert tends to fire if modifying mm structures while
     * looping through them. Don't do this, it is bad! */
    assert(!block->free, "MM Fatal Error");

    if (block->previous == NULL)
    {
        assert(firstUsedBlock == block, "MM Fatal Error");
        firstUsedBlock = block->next;
        firstUsedBlock->previous = NULL;
    }
    else
    {
        assert(firstUsedBlock != block, "MM Fatal Error");
        block->previous->next = block->next;
        block->next->previous = block->previous;
    }
    block->next = NULL;
    block->previous = NULL;
}

inline void addToUsedList(MemoryHeader *block)
{
    assert(!block->free, "MM Fatal Error");
    // make sure both block->previous and next are null before adding
    assert(block->previous == NULL, "MM Fatal Error");
    assert(block->next == NULL, "MM Fatal Error");
    // insert at the beginning
    if (firstUsedBlock != NULL)
    {   
        block->next = firstUsedBlock;
        firstUsedBlock->previous = block;
    }
    firstUsedBlock = block;
}

void *kalloc(Size size, Thread *thread)
{
    /* Thread may be null */
    mutexAcquireLock(&mmLockMutex);
    
    assert(mmInstalled, "MM Fatal Error");
    assert(size != 0, "Cannot allocate 0-length memory block!");
    MemoryHeader *currentBlock = firstFreeBlock;
    sweep();
    while (true)
    {
        assert(currentBlock != NULL, "Out of Memory! Was looking for %i bytes", size);
            
        if (unlikely(currentBlock->size >= size &&
            (currentBlock->size <= size + sizeof(MemoryHeader))))
        {   
            // Block is just large enough to be used, not large enough to split
            removeFromFreeList(currentBlock);
            currentBlock->free = false;
            addToUsedList(currentBlock);
            sweep();
            mutexReleaseLock(&mmLockMutex);
            currentBlock->thread = thread;
            return (void*)&currentBlock->start;
        }
        else if (currentBlock->size > size)
        {   
            // Split into a block just the right size and one for leftovers
            removeFromFreeList(currentBlock);
            MemoryHeader *newBlock = /* For leftover memory */
                (MemoryHeader*)(currentBlock->start + size);
            newBlock->size = (Size)currentBlock->size - size - sizeof(MemoryHeader);
            newBlock->free = true;
            newBlock->startMagic = mmMagic;
            newBlock->previous = NULL;
            newBlock->next = NULL;
            newBlock->endMagic = mmMagic;
            addToFreeList(newBlock);

            assert(newBlock->size + size + sizeof(MemoryHeader) ==
                currentBlock->size, "Header check fail");

            currentBlock->size = size;
            currentBlock->free = false;
            addToUsedList(currentBlock);

            sweep();
            currentBlock->thread = thread;
            mutexReleaseLock(&mmLockMutex);
            return (void*)&currentBlock->start;
        }
        else // Block not large enough. Next.
            currentBlock = currentBlock->next;
    }
    panic("MM Fatal Error");
    return NULL; // here to make compiler happy
}

void *malloc(Size size)
{
    return kalloc(size, getCurrentThread());
}

void *realloc(void *memory, Size size)
{
    // change the size of a single block of allocated memory
    mutexAcquireLock(&mmLockMutex);
    sweep();
    MemoryHeader *block = (MemoryHeader*)((Size)memory - sizeof(MemoryHeader));
    MemoryHeader *nextBlock = (MemoryHeader*)((Size)&block->start + block->size);
    if (nextBlock->startMagic == mmMagic && nextBlock->endMagic == mmMagic && nextBlock->free)
    {
        if (block->size + 2 * sizeof(MemoryHeader) + nextBlock->size < size)
        {
            /* Big enough to split nextBlock */
            Size room = block->size + nextBlock->size;
            removeFromFreeList(nextBlock);
            block->size = size;
            MemoryHeader *newBlock = /* For leftover memory */
                (MemoryHeader*)(&block->start + size);
            newBlock->size = room - size;
            newBlock->free = true;
            newBlock->startMagic = mmMagic;
            newBlock->previous = NULL;
            newBlock->next = NULL;
            newBlock->endMagic = mmMagic;
            addToFreeList(newBlock);
            sweep();
            mutexReleaseLock(&mmLockMutex);
            return memory;
        }
        else if (block->size + sizeof(MemoryHeader) + nextBlock->size < size)
        {
            /* Big enough to use nextBlock, not big enough to split */
            removeFromFreeList(nextBlock);
            block->size += sizeof(MemoryHeader) + nextBlock->size;
            sweep();
            mutexReleaseLock(&mmLockMutex);
            return memory;
        }
        /* else, not big enough, continue */
    }
    void *new_mem = malloc(size);
    memcpy(new_mem, memory, size);
    free(memory);
    mutexReleaseLock(&mmLockMutex);
    return new_mem;
}

void *calloc(Size amount, Size elementSize)
{
    void *mem = malloc(amount * elementSize);
    return memset(mem, 0, amount * elementSize);
}

bool isAllocated(void *memory)
{
    if (memory == NULL)
        return false;
    MemoryHeader *header = (MemoryHeader*)(memory - sizeof(MemoryHeader));
    if (header->startMagic != mmMagic || header->endMagic != mmMagic)
        return false;
    if (header->free)
        return false;
    return true;
}

void free(void *memory)
{
    mutexAcquireLock(&mmLockMutex);
    if (unlikely(memory == NULL))
    {
        printf("Attempted to free NULL pointer...ignoring.\n");
        mutexReleaseLock(&mmLockMutex);
        return;
    }
    
    if (!mmInstalled)
        panic("MM Fatal Error, MM not installed");

    sweep();

    MemoryHeader *header = (MemoryHeader*)(memory - sizeof(MemoryHeader));
    
    if (unlikely(header->startMagic != mmMagic || header->endMagic != mmMagic))
    {
        /* This is not necessarily a bad thing, just happens when the memory given to be
         * freed is in the stack or something, not handled by the memory manager */
        printf("Incorrect freeing of unallocated pointer at %x, ignoring.\n", memory);
        mutexReleaseLock(&mmLockMutex);
        return;
    }
    
    if (unlikely(header->free))
    {
        /* This is not a bad thing, but probably shouldn't happen either. */
#ifndef __release__
        printf("Memory at %x already freed, ignoring.\n", memory);
#endif
        mutexReleaseLock(&mmLockMutex);
        return;
    }
    
    removeFromUsedList(header);
    header->free = true;
    addToFreeList(header);

    MemoryHeader *merge(MemoryHeader *a, MemoryHeader *b)
    {   
        assert((Size)a + sizeof(MemoryHeader) + a->size == (Size)b, "Merge failed");
        a->size += b->size + sizeof(MemoryHeader);
        removeFromFreeList(b);
        return a;
    }

    MemoryHeader *blockEnd(MemoryHeader *block)
    {   
        return (MemoryHeader*)(&block->start + block->size);
    }

    MemoryHeader *block = firstFreeBlock;

    while (block != NULL)
    {   
        MemoryHeader *next = block->next;
        if (blockEnd(header) == block)
        {   
            merge(header, block);
        }
        else if (blockEnd(block) == header)
        {
            merge(block, header);
            header = block;
        }
        block = next;
    }

    sweep();
    mutexReleaseLock(&mmLockMutex);
}

void freeThread(Thread *thread)
{
    mutexAcquireLock(&mmLockMutex);
    if (likely(thread->pid > 0))
        free(thread->stack);
    /* free all blocks left allocated to thread and print warning that
     * they have not been properly freed at thread end. */
    MemoryHeader *currentBlock = firstUsedBlock;
    while (currentBlock != NULL)
    {
        MemoryHeader *next = currentBlock->next;
        if (currentBlock->thread == thread)
        {
            #ifndef __release__
            printf("Dying thread '%s' failed to free %x, size %x, freeing.\n",
                thread->name, &currentBlock->start, currentBlock->size);
            #endif
            free(&currentBlock->start);
        }
        currentBlock = next;
    }
    free(thread);
    mutexReleaseLock(&mmLockMutex);
}

void meminfo()
{
    MemoryHeader *currentBlock = firstFreeBlock;
    printf("=========== MemInfo ===========\n");
    do printf("free block at %x, size %x, magic %x %x\n",
        &currentBlock->start,
        currentBlock->size, currentBlock->startMagic, currentBlock->endMagic);
    while ((currentBlock = currentBlock->next) != NULL);

    currentBlock = firstUsedBlock;
    if (currentBlock == NULL)
        return;
    do printf("used block at %x, size %x, magic %x %x\n",
        &currentBlock->start,
        currentBlock->size, currentBlock->startMagic, currentBlock->endMagic);
    while ((currentBlock = currentBlock->next) != NULL);
}

// Amount of memory used
Size memUsed()
{
    Size amount = 0;
    MemoryHeader *currentBlock = firstUsedBlock;
    if (currentBlock == NULL)
        return 0;
    do
    {
        amount += currentBlock->size;
    } while ((currentBlock = currentBlock->next) != NULL);
    return amount;
}

Size memFree()
{
    Size amount = 0;
    MemoryHeader *currentBlock = firstFreeBlock;
    if (currentBlock == NULL)
        return 0;
    do
    {
        amount += currentBlock->size;
    } while ((currentBlock = currentBlock->next) != NULL);
    return amount;
}

void sweep() // quick tests
{
    mutexAcquireLock(&mmLockMutex);
    MemoryHeader *currentBlock = firstFreeBlock;
    // if assert fails on next line, chances are you used malloc() before
    // the memory manager was initialized.
    assert(currentBlock >= (MemoryHeader*)0x100000, "Sweep failed");
    assert(currentBlock->previous == NULL, "Sweep failed");
    do
    {
        assert(currentBlock->free, "Sweep failed");
        assert(currentBlock->startMagic == mmMagic, "Sweep failed");
        assert(currentBlock->endMagic == mmMagic, "Sweep failed");
        assert((Size)&currentBlock->start ==
            (Size)currentBlock + sizeof(MemoryHeader), "Sweep failed");
    } while ((currentBlock = currentBlock->next) >= (MemoryHeader*)0x100000);

    currentBlock = firstUsedBlock;
    if (currentBlock == NULL)
    {
        mutexReleaseLock(&mmLockMutex);
        return;
    }
    assert(currentBlock->previous == NULL, "Sweep failed");
    do
    {
        assert(!currentBlock->free, "Sweep failed");
        assert(currentBlock->startMagic == mmMagic, "Sweep failed");
        assert(currentBlock->endMagic == mmMagic, "Sweep failed");
        assert((Size)&currentBlock->start ==
            (Size)currentBlock + sizeof(MemoryHeader), "Sweep failed");
    } while ((currentBlock = currentBlock->next) >= (MemoryHeader*)0x100000);
    mutexReleaseLock(&mmLockMutex);
}
