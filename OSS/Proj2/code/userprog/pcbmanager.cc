// pcbmanager.cc

#include "pcbmanager.h"

PCBManager::PCBManager(int initialMaxProcesses) {
    this->maxProcesses = initialMaxProcesses + 1; // Include PID 0
    bitmap = new BitMap(this->maxProcesses);
    pcbs = new PCB*[this->maxProcesses];
    pcbManagerLock = new Lock("PCBManagerLock");

    // Mark PID 0 as used to reserve it
    bitmap->Mark(0);

    for(int i = 0; i < this->maxProcesses; i++) {
        pcbs[i] = NULL;
    }
}

PCBManager::~PCBManager() {
    delete bitmap;
    delete[] pcbs;
    delete pcbManagerLock;
}

PCB* PCBManager::AllocatePCB() {
    pcbManagerLock->Acquire();
    int pid = bitmap->Find();
    if (pid == -1) {
        pcbManagerLock->Release();
        return NULL; // No available PID
    }
    pcbs[pid] = new PCB(pid);
    bitmap->Mark(pid);
    pcbManagerLock->Release();
    return pcbs[pid];
}

int PCBManager::DeallocatePCB(PCB* pcb) {
    pcbManagerLock->Acquire();
    int pid = pcb->pid;
    if (pid > 0 && pid < maxProcesses) {
        bitmap->Clear(pid); // Release the PID
        delete pcbs[pid];
        pcbs[pid] = NULL;
    }
    pcbManagerLock->Release();
    return 0;
}

PCB* PCBManager::GetPCB(int pid) {
    if (pid > 0 && pid < maxProcesses) {
    return pcbs[pid];
    }
    return NULL;
}

int PCBManager::GetMaxProcesses() {
    return maxProcesses;
}

// pcbmanager.cc
int PCBManager::GetNextFreePid() {
    pcbManagerLock->Acquire();
    int pid = bitmap->Find();
    pcbManagerLock->Release();
    return (pid == -1) ? maxProcesses + 1 : pid; // If no PID available, return an out-of-range value
}
