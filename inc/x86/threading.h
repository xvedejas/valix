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

#ifndef __threading_h__
#define __threading_h__
#include <main.h>
#include <data.h>

/* threading.h deals with threads, which are in this case simultaneous
 * executions of the kernel binary. Valix "processes" only exist from a
 * userspace point of view, and are, at the lowest level, actually just kernel
 * threads. */

#define ThreadFunc void __attribute__((fastcall))
#define threadingLock() threadingLockObj++
#define threadingUnlock() threadingLockObj--

const u32 systemClockFreq;
/* threadingLockObj is a counting mutex. While this is greater than 0, threads
 * are not allowed to switch. */
u32 threadingLockObj;
void schedule();
void endThread();
void threadingInstall(void *stackPointer);

///////////////////////////////////////////////////////////////////////////////
// Thread Interface                                                          //
///////////////////////////////////////////////////////////////////////////////

volatile Size pidCount;

/* Each thread is guaranteed to have a process ID (pid) that is unique at any
 * given instant of time. */

typedef enum
{
	/* Ready threads have not yet begun. Once they do, they will be running */
	ready,
	/* Running regularly */
	running,
	/* Sleeping for a certain amount of time */
	sleeping,
	/* Paused until some other thread resumes it. Possibly waiting for a mutex
     * lock. */
	paused,
    /* Kill the zombies! */
    zombie,
} ThreadStatus;

typedef struct thread
{
	String name;
	ThreadStatus status;
	u32 pid;
	/* member "stack" is the lowest memory address of the stack, that is, the
	 * beginning of memory allocated for stack use */
	void *stack;
	void *esp, *ebp;
	/* Any mutex that this thread is waiting for to acquire a lock */
	struct mutex *mutexWaitLock;
	umax sleepOverTime;
	/* The function that this thread begins execution in, specified by spawn */
	ThreadFunc (*func)();
	/* The previous and next threads in the doubly-linked chain */
	struct thread *previous,
		          *next;
    /* If this thread is waiting on a mutex, any other threads in line for the
     * same mutex */
    struct thread *waitingNext;
} Thread;

/* Thread destruction must remove the thread's pid from any mutex that believes
 * the thread might be waiting on it. This should only be a problem when killing
 * threads. */

Thread *spawn(String name, ThreadFunc (*func)());
Thread *getCurrentThread();
void killThread(Thread *thread);
void sleep(u32 milliseconds);
void threadsDebug();

///////////////////////////////////////////////////////////////////////////////
// Mutex Interface                                                           //
///////////////////////////////////////////////////////////////////////////////

/* Mutexes are used to give a thread exclusive use on a resource. */

typedef struct mutex
{
	bool locked;
	/* Thread under control of this mutex */
	Thread *thread;
	/* Linked list of threads waiting on this mutex */
	Thread *threadsWaiting;
} Mutex;

/* A deadlock occurs when a thread that holds a mutex is waiting on a mutex
 * owned by another thread waiting on the first mutex, or any such chain of
 * potentially longer length. It is important to test this before attempting to
 * acquire a lock, to avoid such situations. */

typedef struct
{
	bool accepted;
	/* If accepted, "mutex" is the mutex the process obtained a lock on.
	 * If not accepted, it's the mutex the process must release before
     * obtaining the originally desired lock */
	Mutex *mutex;
} MutexReply;

/* When attempting to gain a lock on a mutex, if accepted, either the thread
 * immediately obtains the lock, or it waits. If rejected, the thread
 * immediately receives the MutexReply, instructing it of conditions that are
 * deemed unsafe due to a previous lock that the thread has already acquired. */

Mutex *mutexNew();
MutexReply mutexAcquireLock(Mutex *mutex);
void mutexReleaseLock(Mutex *mutex);

#endif
