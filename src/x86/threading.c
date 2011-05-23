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
 *
 */

#include <threading.h>
#include <mm.h>

#define saveStackRegs(thread) \
    __asm__ __volatile__("mov %%esp, %0" : "=r"(thread->esp)); \
    __asm__ __volatile__("mov %%ebp, %0" : "=r"(thread->ebp));
    
#define loadStackRegs(thread) \
    __asm__ __volatile__("mov %0, %%ebp" :: "r"(thread->ebp));  \
    __asm__ __volatile__("mov %0, %%esp" :: "r"(thread->esp));

#define sendEOI() \
    __asm__ __volatile__("mov $0x20, %%eax;" ::: "eax");   \
    __asm__ __volatile__("outb %%al, $0x20;sti" ::: "eax");

Thread *currentThread;
u32 threadCount;
volatile u32 ticksUntilSwitch = 0;
const u32 systemClockFreq = 5000;

Thread *getCurrentThread()
{
    return currentThread;
}

void threadingInstall(void *stackPointer)
{
    pidCount = 0;
    Thread *kernelThread = kalloc(sizeof(Thread), NULL);
    kernelThread->stack = (void*)(stackPointer - systemStackSize);
    kernelThread->pid = pidCount++;
    kernelThread->name = "Kernel Init Thread";
    kernelThread->status = running;
    /* The kernel init thread gets highest priority (0). This is because the
     * thread exits as soon as it has initialized all other systems and it is
     * highest priority to finish the initialization stage. */
    kernelThread->next = kernelThread;
    kernelThread->previous = kernelThread;
    kernelThread->waitingNext = NULL;
    currentThread = kernelThread;
    threadCount = 1;
    threadingLockObj = 0;
}

void threadPromote(Thread *thread)
{
    /* Move a thread up so it's next. This is desired for quick response for
     * threads that were previously waiting on a mutex or sleeping. */
    threadingLock();
    thread->previous->next = thread->next;
    thread->next->previous = thread->previous;
    thread->next = currentThread->next;
    currentThread->next->previous = thread;
    currentThread->next = thread;
    thread->previous = currentThread;
    ticksUntilSwitch = 0;
    threadingUnlock();
}

void endThread()
{
    killThread(currentThread);
    for (;;) asm("hlt");
}

void switchThreads()
{
    if (unlikely(currentThread->next == currentThread)) return; /* Only one thread */
    assert(currentThread->next != NULL, "Threading error"); /* Shouldn't happen in a circular list */
    
    Thread *destination = currentThread->next; /* Thread we want to switch to */
    while (destination->status != running && destination->status != ready &&
        destination != currentThread) destination = destination->next;
    
    if (unlikely(destination == currentThread)) /* No thread to switch to. Return. */
        return;
    
    /// todo: note the available stack size remaining for the thread. If the
    /// amount is unusually low, show a warning. If the stack size is zero or
    /// negative, panic.
    
    switch (destination->status)
    {
        case ready:
        {
            destination->esp = (void*)(destination->stack + systemStackSize);
            destination->ebp = destination->esp;
            destination->status = running;
            saveStackRegs(currentThread);
            currentThread = destination;
            loadStackRegs(currentThread);
            asm volatile("push %0" :: "r"(endThread));
            sendEOI();
            asm volatile("jmp *%0" :: "m"(currentThread->func));
            panic("Jump failed!");
        } break;
        case running:
        {
            /* We simply load our new stack. When we return and exit from the
             * interrupt handler, we will return not to the previous thread
             * but to another. */
            saveStackRegs(currentThread);
            currentThread = destination;
            loadStackRegs(currentThread);
            sendEOI();
            return;
        } break;
        default:
            panic("This should never be reached");
    }
    panic("This should never be reached");
}

void schedule()
{
    /* This function runs the scheduler. It is this function that is called on
     * each and every clock cycle. Because it is only called from an IRQ
     * handler, system interrupts are disabled during it, so we have no need
     * to lock threading. */
    
    /* Look through all threads to see if we need to wake any */
    Thread *thread = currentThread->next;
    do
    {
        /* We don't worry about paused threads here. They will be waked by the
         * action of unlocking a mutex most likely. */
        if (thread->status == sleeping)
        {
            // printf("sleeping: %s now: %x overat: %x\n", thread->name, timerTicks, thread->sleepOverTime);
            if (timerTicks > thread->sleepOverTime)
            {
                /* Wake up the thread */
                thread->status = running;
                threadPromote(thread);
            }
        }
        else if (thread->status == zombie)
        {
            /// todo: if the thread has a mutex lock or is waiting on a mutex,
            /// then unlock the mutex or remove it from the waiting queue.
            if (thread == currentThread)
            {
                if (unlikely(thread->next == thread))
                {
                    printf("\n\nNo threads left! Idling.\n");
                    ThreadFunc idle() { while (true) asm("hlt"); };
                    spawn("Idle thread", idle);
                    ticksUntilSwitch = 0;
                }
                assert(ticksUntilSwitch == 0, "Threading error");
            }
            thread->next->previous = thread->previous;
            thread->previous->next = thread->next;
            freeThread(thread);
        }
    } while ((thread = thread->next) != currentThread);
    
    /* Now we will see if we want to switch threads */
    if (ticksUntilSwitch == 0)
    {
        ticksUntilSwitch = 20;
        if (!threadingLockObj) switchThreads();
    }
    ticksUntilSwitch--;
}

Thread *spawn(String name, ThreadFunc (*func)())
{
    threadingLock();
    Thread *thread = (Thread*)kalloc(sizeof(Thread), NULL);
    thread->stack = kalloc(systemStackSize, thread);
    thread->pid = pidCount++;
    thread->name = name;
    thread->status = ready;
    thread->func = func;
    thread->next = currentThread->next;
    thread->next->previous = thread;
    thread->previous = currentThread;
    thread->waitingNext = NULL;
    currentThread->next = thread;
    threadingUnlock();
    return thread;
}

void killThread(Thread *thread)
{
    thread->status = zombie;
    if (thread == currentThread)
        ticksUntilSwitch = 0;
}

void sleep(u32 milliseconds)
{
    threadingLock();
    currentThread->sleepOverTime = timerTicks + milliseconds * systemClockFreq / 1000;
    printf("will sleep until %i\n", currentThread->sleepOverTime);
    currentThread->status = sleeping;
    ticksUntilSwitch = 0;
    threadingUnlock();
    while (currentThread->status == sleeping);
}

Mutex *mutexNew()
{
    Mutex *mutex = malloc(sizeof(Mutex));
    mutex->locked = false;
    mutex->multiplicity = 0;
    mutex->threadsWaiting = NULL;
    return mutex;
}

MutexReply mutexAcquireLock(Mutex *mutex)
{
    if (mutex->locked && mutex->thread != currentThread)
    {
        threadingLock();
        /// Todo: here detect potential deadlocks
        if (mutex->threadsWaiting == NULL)
            mutex->threadsWaiting = currentThread;
        else
        {
            Thread *thread = mutex->threadsWaiting;
            do thread = thread->waitingNext;
            while (thread->waitingNext != NULL);
            thread->waitingNext = currentThread;
        }
        threadingUnlock();
        /* Wait until the mutex is unlocked, when this thread will be unpaused */
        currentThread->status = paused;
        while (currentThread->status == paused);
    }
    mutex->multiplicity++;
    mutex->thread = currentThread;
    mutex->locked = true;
    return (MutexReply){ .accepted = true, .mutex = mutex };
}

/* When a mutex lock is completely released (multiplicity 0) then we want
 * to immediately jump to the thread that was waiting on the lock, to
 * reduce system I/O latency. */
void mutexReleaseLock(Mutex *mutex)
{
    assert(mutex->locked && mutex->multiplicity, "Attempted to free mutex that was not locked");
    assert(mutex->thread == currentThread,
        "Attempted to free mutex that was not allocated to the current thread");
    mutex->multiplicity--;
    if (!mutex->multiplicity)
    {
        mutex->locked = false;
        if (mutex->threadsWaiting != NULL)
        {
            Thread *thread = mutex->threadsWaiting;
            mutex->threadsWaiting = thread->waitingNext;
            thread->waitingNext = NULL;
            thread->status = running;
            threadPromote(thread);
        }
    }
}

void mutexDel(Mutex *mutex)
{
    mutexAcquireLock(mutex);
    free(mutex);
}

void threadsDebug()
{
    threadingLock();
    printf("\n [[ RUNNING THREADS ]] \n");
    String statuses[] = {"ready", "running", "sleeping", "paused", "zombie"};
    Thread *thread = currentThread;
    do
    {
        printf("%i Thread: %s status: %s\n", thread->pid, thread->name, statuses[thread->status]);
    } while ((thread = thread->next) != currentThread);
    threadingUnlock();
}
