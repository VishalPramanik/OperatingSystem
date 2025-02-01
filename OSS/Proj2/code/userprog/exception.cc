// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "system.h"
#include "list.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions
//	are in machine.h.
//----------------------------------------------------------------------

void doExit(int status) {
    int pid = currentThread->space->pcb->pid;
    printf("System Call: [%d] invoked Exit.\n", pid);
    printf("Process [%d] exits with [%d]\n", pid, status);

    currentThread->space->pcb->exitStatus = status;

    // Manage PCB memory As a parent process
    PCB* pcb = currentThread->space->pcb;
    // Delete exited children and set parent null for non-exited ones
    pcb->DeleteExitedChildrenSetParentNull();

    // Manage PCB memory As a child process
    if (pcb->parent == NULL) {
        pcbManager->DeallocatePCB(pcb);
    }

    // Delete address space only after use is completed
    delete currentThread->space;
    // Finish current thread only after all the cleanup is done
    currentThread->Finish();
}

void incrementPC() {
    int oldPCReg = machine->ReadRegister(PCReg);

    machine->WriteRegister(PrevPCReg, oldPCReg);
    machine->WriteRegister(PCReg, oldPCReg + 4);
    machine->WriteRegister(NextPCReg, oldPCReg + 8);
}

void childFunction(int functionAddr) {
    // Restore the child's address space
    currentThread->space->RestoreState();

    // Initialize registers for the new address space
    currentThread->space->InitRegisters();

    // Set the program counter to the function address
    machine->WriteRegister(PCReg, functionAddr);
    machine->WriteRegister(NextPCReg, functionAddr + 4);
    machine->WriteRegister(PrevPCReg, functionAddr - 4);

    // Start running the child process
    machine->Run();

    // Should never reach here
    ASSERT(FALSE);
}

int doFork(int functionAddr) {
    // Get the parent's PID
    int pid = currentThread->space->pcb->pid;
    printf("System Call: [%d] invoked Fork.\n", pid);

    // 1. Check if sufficient memory exists to create new process
    unsigned int n = currentThread->space->GetNumPages();
    if (n > mm->GetFreePageCount()) {
        int potentialChildPid = pcbManager->GetNextFreePid();
        printf("Not Enough Memory for Child Process %d\n", potentialChildPid);
        return -1;
    }

    // 2. SaveUserState for the parent thread
    currentThread->SaveUserState();

    // 3. Create a new address space for child by copying parent address space
    AddrSpace* childAddrSpace = new AddrSpace(currentThread->space);

    // 4. Create a new thread for the child and set its addrSpace
    Thread* childThread = new Thread("childThread");
    childThread->space = childAddrSpace;

    // 5. Create a PCB for the child and connect it all up
    PCB* pcb = pcbManager->AllocatePCB();
    if (pcb == NULL) {
        printf("Error: Unable to allocate PID for new process\n");
        delete childAddrSpace;
        delete childThread;
        return -1;
    }
    pcb->thread = childThread;
    pcb->parent = currentThread->space->pcb;
    currentThread->space->pcb->AddChild(pcb);
    childAddrSpace->pcb = pcb;

    // 6. Save the child's user state (empty for now)
    childThread->SaveUserState();

    // 7. Restore register state of parent user-level process
    currentThread->RestoreUserState();

    // 8. Call thread->Fork on Child, passing functionAddr as argument
    childThread->Fork(childFunction, functionAddr);

    // 9. Print message for child creation using parent's PID
    unsigned int numPages = currentThread->space->GetNumPages();
    printf("Process [%d] Fork: start at address [0x%x] with [%d] pages memory\n", pid, functionAddr, numPages);

    // 10. Return child's PID
    return pcb->pid;
}

int doExec(char* filename) {
    // Use progtest.cc:StartProcess() as a guide
    int pid = currentThread->space->pcb->pid;
    printf("System Call: [%d] invoked Exec.\n", pid);

    // 1. Open the file and check validity
    OpenFile* executable = fileSystem->Open(filename);
    AddrSpace* space;

    if (executable == NULL) {
        printf("Unable to open file %s\n", filename);
        return -1;
    }

    PCB* temp_pcb = currentThread->space->pcb;
    temp_pcb->thread = currentThread;

    // 6. Delete current address space
    delete currentThread->space;

    // 2. Create new address space
    space = new AddrSpace(executable, temp_pcb);

    // 3. Check if Addrspace creation was successful
    if (space->valid != true) {
        printf("Could not create AddrSpace\n");
        return -1;
    }

    // Steps 4 and 5 may not be necessary!!
    // 4. Create a new PCB for the new addrspace
    space->pcb = temp_pcb;

    // 7. Set the addrspace for currentThread
    currentThread->space = space;

    // 8. delete executable; // close file
    delete executable;

    // 9. Initialize registers for new addrspace
    currentThread->space->InitRegisters(); // set the initial register values

    // 10. Initialize the page table
    currentThread->space->RestoreState(); // load page table register
    printf("Exec Program: [%d] loading [%s]\n", pid, filename);

    // 11. Run the machine now that all is set up
    machine->Run(); // jump to the user program
    ASSERT(FALSE); // Execution never reaches here

    return 0;
}

int doJoin(int pid) {
    // Print join invocation
    printf("System Call: [%d] invoked Join.\n", currentThread->space->pcb->pid);
    // 1. Check if this is a valid pid and return -1 if not
    PCB* joinPCB = pcbManager->GetPCB(pid);
    if (joinPCB == NULL) return -1;

    // 2. Check if pid is a child of current process
    PCB* pcb = currentThread->space->pcb;
    if (joinPCB->parent != pcb) return -1;

    // 3. Yield until joinPCB has exited
    while (!joinPCB->HasExited()) {
        printf("System Call: [%d] invoked Yield while waiting for child [%d].\n", currentThread->space->pcb->pid, pid);
        currentThread->Yield();
    }

    // 4. Store status and delete joinPCB
    int status = joinPCB->exitStatus;
    pcbManager->DeallocatePCB(joinPCB);

    // 5. Return status
    printf("Process [%d] successfully joined with child process [%d], exit status [%d]\n", currentThread->space->pcb->pid, pid, status);
    return status;
}

int doKill(int pid) {
    // 1. Check if the pid is valid and if not, return -1
    PCB* targetPCB = pcbManager->GetPCB(pid);
    
    if (targetPCB == NULL) {
        printf("Error: Invalid PID [%d] provided to Kill\n", pid);
        return -1;
    }

    // 2. If pid is self, then just exit the process
    if (targetPCB == currentThread->space->pcb) {
        printf("Process [%d] is killing itself\n", pid);
        doExit(-1); // Using -1 to indicate the process was killed
        return 0;
    }

    // 3. Valid kill, pid exists and not self
    Thread* targetThread = targetPCB->thread;

    // Check if target thread is not NULL to avoid segmentation faults
    if (targetThread == NULL) {
        printf("Error: Target thread for process [%d] is NULL\n", pid);
        return -1;
    }

    // Set exit status to indicate process was killed
    targetPCB->exitStatus = -1;

    // Clean up children processes
    targetPCB->DeleteExitedChildrenSetParentNull();

    // Deallocate the PCB and release the PID
    pcbManager->DeallocatePCB(targetPCB);

    // Clean up the target thread's address space
    if (targetThread->space != NULL) {
        delete targetThread->space;
        targetThread->space = NULL;
    }

    // Mark the target thread for termination
    printf("System Call: [%d] invoked Kill.\n", currentThread->space->pcb->pid);
    printf("Process [%d] killed process [%d]\n", currentThread->space->pcb->pid, pid);
    scheduler->RemoveThread(targetThread);

    // 5. Return 0 for success
    return 0;
}


void doYield() {
    int pid = currentThread->space->pcb->pid;
    printf("System Call: [%d] invoked Yield.\n", pid);
    currentThread->Yield();
}

char* readString(int virtualAddr) {
    int i = 0;
    char* str = new char[256];
    unsigned int physicalAddr = currentThread->space->Translate(virtualAddr);

    // Need to get one byte at a time since the string may straddle multiple pages that are not guaranteed to be contiguous in the physicalAddr space
    bcopy(&(machine->mainMemory[physicalAddr]), &str[i], 1);
    while (str[i] != '\0' && i != 256 - 1) {
        virtualAddr++;
        i++;
        physicalAddr = currentThread->space->Translate(virtualAddr);
        bcopy(&(machine->mainMemory[physicalAddr]), &str[i], 1);
    }
    if (i == 256 - 1 && str[i] != '\0') {
        str[i] = '\0';
    }

    return str;
}

void doCreate(char* fileName) {
    printf("Syscall Call: [%d] invoked Create.\n", currentThread->space->pcb->pid);
    fileSystem->Create(fileName, 0);
}


void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt)) {
	    DEBUG('a', "Shutdown, initiated by user program.\n");
   	    interrupt->Halt();
    } else  if ((which == SyscallException) && (type == SC_Exit)) {
        // Implement Exit system call
        doExit(machine->ReadRegister(4));
    } else if ((which == SyscallException) && (type == SC_Fork)) {
        int ret = doFork(machine->ReadRegister(4));
        machine->WriteRegister(2, ret);
        incrementPC();
    } else if ((which == SyscallException) && (type == SC_Exec)) {
        int virtAddr = machine->ReadRegister(4);
        char* fileName = readString(virtAddr);
        int ret = doExec(fileName);
        machine->WriteRegister(2, ret);
        incrementPC();
    } else if ((which == SyscallException) && (type == SC_Join)) {
        int ret = doJoin(machine->ReadRegister(4));
        machine->WriteRegister(2, ret);
        incrementPC();
    } else if ((which == SyscallException) && (type == SC_Kill)) {
        int ret = doKill(machine->ReadRegister(4));
        machine->WriteRegister(2, ret);
        incrementPC();
    } else if ((which == SyscallException) && (type == SC_Yield)) {
        doYield();
        incrementPC();
    } else if((which == SyscallException) && (type == SC_Create)) {
        int virtAddr = machine->ReadRegister(4);
        char* fileName = readString(virtAddr);
        doCreate(fileName);
        incrementPC();
    } else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
}

