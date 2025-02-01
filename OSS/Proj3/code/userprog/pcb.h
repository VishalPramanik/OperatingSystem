#ifndef PCB_H
#define PCB_H
#define MAX_OPEN_FILES 64

#include "list.h"
#include "pcbmanager.h"

class Thread;
class PCBManager;
extern PCBManager* pcbManager;

class PCB {

    public:
        PCB(int id);
        ~PCB();
        int pid;
        PCB* parent;
        Thread* thread;
        int exitStatus;
        OpenFile* openFileTable[MAX_OPEN_FILES];
        void AddChild(PCB* pcb);
        int RemoveChild(PCB* pcb);
        bool HasExited();
        void DeleteExitedChildrenSetParentNull();
        int AddOpenFile(OpenFile* openFile);
        OpenFile* GetOpenFile(int fd);
        bool CloseOpenFile(int fd);


    private:
        List* children;
        

};

#endif // PCB_H