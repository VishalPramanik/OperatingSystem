#include "pcb.h"


PCB::PCB(int id) {

    pid = id;
    parent = NULL;
    children = new List();
    thread = NULL;
    exitStatus = -9999;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        openFileTable[i] = NULL;
    }

}



PCB::~PCB() {

    delete children;

}



void PCB::AddChild(PCB* pcb) {

    children->Append(pcb);


}


int PCB::RemoveChild(PCB* pcb){

    return children->RemoveItem(pcb);

}


bool PCB::HasExited() {
    return exitStatus == -9999 ? false : true;
}


void decspn(int arg) {
    PCB* pcb = (PCB*)arg;
    if (pcb->HasExited()) pcbManager->DeallocatePCB(pcb);
    else pcb->parent = NULL;
}


void PCB::DeleteExitedChildrenSetParentNull() {
    if(!children->IsEmpty()){
        children->Mapcar(decspn);
    }
    else{
        parent = NULL;
    }
}



int PCB::AddOpenFile(OpenFile* file) {
    for (int i = 2; i < MAX_OPEN_FILES; i++) {
        if (openFileTable[i] == NULL) {
            openFileTable[i] = file;
            return i;
        }
    }
    return -1; // No space available
}



OpenFile* PCB::GetOpenFile(int fd) {
    if (fd >= 0 && fd < MAX_OPEN_FILES && openFileTable[fd] != nullptr) {
        return openFileTable[fd];
    }
    return nullptr; // Invalid file descriptor
}

bool PCB::CloseOpenFile(int fd) {
    // Validate fd
    if (fd < 2 || fd >= MAX_OPEN_FILES || openFileTable[fd] == NULL) {
        return false; // invalid fd
    }
    delete openFileTable[fd];
    openFileTable[fd] = NULL;
    return true;
}
