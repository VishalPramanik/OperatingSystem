// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(const char* debugName, int initialValue)
{
    name = debugName;
    value = initialValue;
    queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    while (value == 0) { 			// semaphore not available
	queue->Append((void *)currentThread);	// so go to sleep
	currentThread->Sleep();
    } 
    value--; 					// semaphore available, 
						// consume its value
    
    (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = (Thread *)queue->Remove();
    if (thread != NULL)	   // make thread ready, consuming the V immediately
	scheduler->ReadyToRun(thread);
    value++;
    (void) interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
Lock::Lock(const char* debugName) {
    name = (char*)debugName;         // Store the debug name for tracking
    free = false;                  // Lock is initially FREE
    lockHolder = NULL;               // No thread holds the lock initially
    queue = new List();     // List to hold waiting threads
}
Lock::~Lock() {
    delete queue;
}
void Lock::Acquire() {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);  // Disable interrupts for atomicity

    // If the lock is already held, block the current thread
    while (free) {
        queue->Append(currentThread);  // Add the current thread to the waiting queue
        currentThread->Sleep();  // Put the current thread to sleep until the lock is available
    }

    // Once the lock is free, acquire it
    free = true;
    lockHolder = currentThread;  // Set the current thread as the lock holder

    (void) interrupt->SetLevel(oldLevel);  // Re-enable
}
void Lock::Release() {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);  // Disable interrupts for atomicity

    // Only the thread that holds the lock can release it
    if (lockHolder == currentThread) {
        // Mark the lock as FREE
        free = false;
        lockHolder = NULL;

        // Wake up the next thread waiting for the lock, if any
        if (!queue->IsEmpty()) {
            Thread *nextThread = (Thread*)queue->Remove();
            scheduler->ReadyToRun(nextThread);  // Move the next waiting thread to the ready queue
        }
    }

    (void) interrupt->SetLevel(oldLevel);  // Re-enable interrupts
}

bool Lock::isHeldByCurrentThread() {
    return lockHolder == currentThread;
}

Condition::Condition(const char* debugName) { 
    name = (char*)debugName;  // Store the debug name for debugging
    queue = new List(); 
}
Condition::~Condition() {
    delete queue;  // Delete the wait queue
}
void Condition::Wait(Lock* conditionLock) { 
    
    //ASSERT(FALSE); 
    IntStatus oldLevel = interrupt->SetLevel(IntOff);  // Disable interrupts for atomicity

    // Ensure that the current thread holds the conditionLock
    ASSERT(conditionLock->isHeldByCurrentThread());

    // Add the current thread to the wait queue
    queue->Append((void *)currentThread);

    // Release the conditionLock before going to sleep
    conditionLock->Release();

    // Put the current thread to sleep until it is woken up
    currentThread->Sleep();

    // When woken up, re-acquire the lock before continuing
    conditionLock->Acquire();

    (void) interrupt->SetLevel(oldLevel);  
    
    }
void Condition::Signal(Lock* conditionLock) {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);  // Disable interrupts for atomicity

    // Ensure that the current thread holds the conditionLock
    ASSERT(conditionLock->isHeldByCurrentThread());

    // If there is a thread waiting, wake it up
    if (!queue->IsEmpty()) {
        Thread* thread = (Thread *)queue->Remove();
        scheduler->ReadyToRun(thread);  // Move the thread to the ready queue
    }

    (void) interrupt->SetLevel(oldLevel);  
 }
void Condition::Broadcast(Lock* conditionLock) {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);  // Disable interrupts for atomicity

    // Ensure that the current thread holds the conditionLock
    ASSERT(conditionLock->isHeldByCurrentThread());

    // Wake up all waiting threads
    while (!queue->IsEmpty()) {
        Thread* thread = (Thread *)queue->Remove();
        scheduler->ReadyToRun(thread);  // Move the thread to the ready queue
    }

    (void) interrupt->SetLevel(oldLevel);
 }
