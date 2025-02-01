// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

//#define HW1_SEMAPHORES
//#define HW1_LOCKS
//#define HW1_CONDITION
//#define HW1_ELEVATOR

#include "copyright.h"
#include "system.h"
#include "thread.h"
#include "synch.h"



// testnum is set in main.cc
int testnum = 1;

// Declare the shared variable
int SharedVariable;

int numThreadsActive; // used to implement barrier upon completion


#if defined(HW1_SEMAPHORES)

Semaphore *semaphore = new Semaphore("SharedVariable Semaphore", 1);  // Initialize semaphore
void SimpleThread(int which) {
    int num, val;

    for (num = 0; num < 5; num++) {
        // entry section
        semaphore->P();  // Wait (down) - enter critical section
        val = SharedVariable;
        printf("*** thread %d sees value %d\n", which, val);
        currentThread->Yield(); 
        SharedVariable = val + 1;
        semaphore->V();  // Signal (up) - exit critical section
        // exit section

        currentThread->Yield();  // Yield after incrementing and releasing semaphore
    }
    numThreadsActive=numThreadsActive-1;
    while (numThreadsActive != 0){
       currentThread->Yield(); 
    }
    val = SharedVariable;
    printf("Thread %d sees final value %d\n", which, val);
}
// #else
// void SimpleThread(int which) {
//     int num, val;

//     for (num = 0; num < 5; num++) {
//         val = SharedVariable;
//         printf("*** thread %d sees value %d\n", which, val);
//         currentThread->Yield(); 
//         SharedVariable = val + 1; 
//         currentThread->Yield();  // Yield after incrementing and releasing semaphore
//     }

//     numThreadsActive=numThreadsActive-1;
//     while (numThreadsActive != 0){
//        currentThread->Yield(); 
//     }

//     val = SharedVariable;
//     printf("Thread %d sees final value %d\n", which, val);
// }

void
ThreadTestNew(int n) {
    DEBUG('t', "Entering SimpleTest");
    Thread *t;
    numThreadsActive = n;
    printf("NumthreadsActive = %d\n", numThreadsActive);

    for(int i=1; i<n; i++)
    {
        t = new Thread("forked thread");
        t->Fork(SimpleThread,i);
    }
    SimpleThread(0);
}

// #endif


#elif defined(HW1_LOCKS)
Lock *lock = new Lock("SharedVariable Lock");  // Initialize the lock

void SimpleThread(int which) {
    int num, val;

    for (num = 0; num < 5; num++) {
        // entry section
        lock->Acquire();  // Acquire the lock - enter critical section
        val = SharedVariable;
        printf("*** thread %d sees value %d\n", which, val);
        currentThread->Yield(); 
        SharedVariable = val + 1;
        lock->Release();  // Release the lock - exit critical section
        // exit section

        currentThread->Yield();  // Yield after incrementing and releasing the lock
    }

    // Decrement numThreadsActive and wait for all threads to finish
    numThreadsActive = numThreadsActive - 1;

    // Yield while other threads are still active
    while (numThreadsActive != 0) {
        currentThread->Yield(); 
    }

    // Print final value of SharedVariable
    val = SharedVariable;
    printf("Thread %d sees final value %d\n", which, val);
}

// #else
// void SimpleThread(int which) {
//     int num, val;

//     for (num = 0; num < 5; num++) {
//         val = SharedVariable;
//         printf("*** thread %d sees value %d\n", which, val);
//         currentThread->Yield(); 
//         SharedVariable = val + 1; 
//         currentThread->Yield();  // Yield after incrementing and releasing semaphore
//     }

//     val = SharedVariable;
//     printf("Thread %d sees final value %d\n", which, val);
// }
void
ThreadTestNew(int n) {
    DEBUG('t', "Entering SimpleTest");
    Thread *t;
    numThreadsActive = n;
    printf("NumthreadsActive = %d\n", numThreadsActive);

    for(int i=1; i<n; i++)
    {
        t = new Thread("forked thread");
        t->Fork(SimpleThread,i);
    }
    SimpleThread(0);
}
// #endif

#elif defined(HW1_CONDITION)

Lock * pingLock;
Condition *cv;

void
PingPong(int which)
{
    char *msg = (char *) which;
    int i;

    pingLock->Acquire();
    for (i = 0; i < 5; i++) {
		printf("%s\n", msg);
		cv->Signal(pingLock);
		cv->Wait(pingLock);
    }
    pingLock->Release();
}

void Ping()
{
    pingLock = new Lock("cv");
    cv = new Condition("ping pong");

    char *ping = "ping";
    char *pong = "pong";

    Thread *t = new Thread(ping);
    t->Fork(PingPong, (int) ping);

    PingPong((int) pong);

 }
// #endif

#elif defined(HW1_ELEVATOR)
void Elevator(int numFloors);
void ArrivingGoingFromTo(int atFloor, int toFloor);

typedef struct Person {
    int id;
    int atFloor;
    int toFloor;
} Person;

class ELEVATOR {
public:
    ELEVATOR(int numFloors);
    ~ELEVATOR();
    void hailElevator(Person *p);
    void start();

private:
    int currentFloor;
    Condition **entering;
    Condition **leaving;
    int *personsWaiting;
    int occupancy;
    int maxOccupancy;
    Lock *elevatorLock;
};

int nextPersonID = 1;
Lock *personIDLock = new Lock("PersonIDLock");

ELEVATOR *e;

void ELEVATOR::start() {

    while (1) {
        elevatorLock->Acquire();  // 0. Acquire elevatorLock to synchronize elevator operations
        // Check if the elevator has any active passengers or if anyone is waiting
        bool noActivePersons = (occupancy == 0);  // Elevator is empty, check if there are any people waiting
        for (int i = 0; i < maxOccupancy; i++) {  // Loop over each floor to see if anyone is waiting
            if (personsWaiting[i] > 0) {
                noActivePersons = false;  // Someone is waiting on this floor
                break;
            }
        }

        if (noActivePersons) {
            // No one is in the elevator and no one is waiting, so the elevator should stop
            printf("Elevator is at first floor, waiting for new passengers...\n");
            elevatorLock->Release();  // Release the lock before idling
            currentThread->Yield();   // Yield the CPU and wait for a new request (person hailing the elevator)
            continue;  // Wait until someone hails the elevator again
        }

        // B. While there are active persons (passengers inside or waiting people)
        
        // 1. Signal persons inside elevator to get off if they're at their destination
        leaving[currentFloor - 1]->Broadcast(elevatorLock);  // Broadcast to everyone who wants to leave at this floor

        // 2. Signal persons at the current floor to get in, one at a time, and check the occupancy limit
        while (personsWaiting[currentFloor - 1] > 0 && occupancy < maxOccupancy) {
            entering[currentFloor - 1]->Signal(elevatorLock);  // Signal one person to enter
            personsWaiting[currentFloor - 1]--;  // Reduce count of people waiting at this floor
            occupancy++;  // Increment occupancy as someone enters
        }

        // 2.5 Release elevatorLock after signaling to allow other threads to access shared data
        elevatorLock->Release();

        // 3. Simulate time for moving between floors
        for (int j = 0; j < 1000000; j++) {
            currentThread->Yield();  // Simulate elevator moving between floors by yielding the CPU
        }

        // 4. Go to next floor
        currentFloor = (currentFloor % maxOccupancy) + 1;  // Move to the next floor (loop around if at top floor)
        printf("Elevator arrives on floor %d\n", currentFloor);
    }
}



void ElevatorThread(int numFloors) {
    printf("Elevator with %d floors was created!\n", numFloors);
    e = new ELEVATOR(numFloors);
    e->start();
}


ELEVATOR::ELEVATOR(int numFloors) {
    currentFloor = 1;  // Start the elevator at the first floor
    occupancy = 0;  // Initially, the elevator is empty
    maxOccupancy = 5;  // Set maximum occupancy for the elevator

    entering = new Condition*[numFloors];  // Condition for people to enter on each floor
    leaving = new Condition*[numFloors];   // Condition for people to leave on each floor
    personsWaiting = new int[numFloors];   // Tracks how many people are waiting on each floor
    elevatorLock = new Lock("ElevatorLock");  // Lock for elevator state

    // Initialize conditions and waiting arrays for each floor
    for (int i = 0; i < numFloors; i++) {
        entering[i] = new Condition("Entering " + i);  // Condition variable for entering on floor i
        leaving[i] = new Condition("Leaving " + i);    // Condition variable for leaving on floor i
        personsWaiting[i] = 0;  // Initially, no one is waiting on any floor
    }
}


void Elevator(int numFloors) {
    Thread *t = new Thread("Elevator");
    t->Fork((VoidFunctionPtr)ElevatorThread, numFloors);
}

void ELEVATOR::hailElevator(Person *p) {
    // 1. Increment the count of persons waiting at the person's current floor
    personsWaiting[p->atFloor - 1]++;  // Increment the number of people waiting on this floor

    // 2. Hail the elevator if it's idle
    elevatorLock->Acquire();  // Acquire the lock to safely modify shared data

    // 3. Wait for elevator to arrive at the person's current floor
    entering[p->atFloor - 1]->Wait(elevatorLock);  // Wait for the elevator to arrive

    // 5. Get into the elevator
    printf("Person %d got into the elevator.\n", p->id);  // Print when the person actually gets into the elevator

    // 6. Decrement the count of persons waiting at this floor
    personsWaiting[p->atFloor - 1]--;  // Decrement since the person has boarded the elevator

    // 7. Increment the count of persons inside the elevator
    occupancy++;  // Increment occupancy as someone enters the elevator

    // 8. Wait for the elevator to reach the person's destination floor
    leaving[p->toFloor - 1]->Wait(elevatorLock);  // Wait for the elevator to arrive at the destination floor

    // 9. Get out of the elevator
    printf("Person %d got out of the elevator.\n", p->id);  // Print when the person gets out at the destination

    // 10. Decrement the number of persons inside the elevator
    occupancy--;  // Decrement occupancy as the person leaves the elevator

    // 11. Release the lock to allow other threads to access the elevator state
    elevatorLock->Release();  // Release the lock after all operations are done
}



void PersonThread(int person) {
    Person *p = (Person *)person;
    printf("Person %d wants to go from floor %d to %d\n", p->id, p->atFloor, p->toFloor);
    e->hailElevator(p);
}

int getNextPersonID() {
    int personID = nextPersonID;
    personIDLock->Acquire();
    nextPersonID++;
    personIDLock->Release();
    return personID;
}



void ArrivingGoingFromTo(int atFloor, int toFloor) {
    Person *p = new Person;
    p->id = getNextPersonID();
    p->atFloor = atFloor;
    p->toFloor = toFloor;

    Thread *t = new Thread("Person " + p->id);
    t->Fork((VoidFunctionPtr)PersonThread, (int)p);
}

void ElevatorTest(int numFloors, int numPersons) {
    Elevator(numFloors);

    for (int i = 0; i < numPersons; i++) {
        int atFloor = (Random() % numFloors) + 1;
        int toFloor = -1;
        do {
            toFloor = (Random() % numFloors) + 1;
        } while (atFloor == toFloor);

        ArrivingGoingFromTo(atFloor, toFloor);

        // Simulate delay
        for (int j = 0; j < 1000000; j++) {
            currentThread->Yield();
        }
    }
}


#else
    void SimpleThread(int which);
    //----------------------------------------------------------------------
    // SimpleThread
    // 	Loop 5 times, yielding the CPU to another ready thread 
    //	each iteration.
    //
    //	"which" is simply a number identifying the thread, for debugging
    //	purposes.
    //----------------------------------------------------------------------

    // Prototype for SimpleThread (already present in threadtest.cc)

    void
    SimpleThread(int which)
    {
        int num;
        
        for (num = 0; num < 5; num++) {
        printf("*** thread %d looped %d times\n", which, num);
            currentThread->Yield();
        }
    }
    //----------------------------------------------------------------------
    // ThreadTest1
    // 	Set up a ping-pong between two threads, by forking a thread 
    //	to call SimpleThread, and then calling SimpleThread ourselves.
    //----------------------------------------------------------------------

    void
    ThreadTest1()
    {
        DEBUG('t', "Entering ThreadTest1");

        Thread *t = new Thread("forked thread");

        t->Fork(SimpleThread, 1);
        SimpleThread(0);
    }



    //----------------------------------------------------------------------
    // ThreadTest
    // 	Invoke a test routine.
    //----------------------------------------------------------------------

    void
    ThreadTest()
    {
        switch (testnum) {
        case 1:
        ThreadTest1();
        break;
        default:
        printf("No test specified.\n");
        break;
        }
    }

#endif
