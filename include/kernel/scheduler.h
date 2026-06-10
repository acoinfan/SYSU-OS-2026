#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "list.h"
#include "thread.h"
#include "screen.h"

#define ListItem2PCB(ADDRESS, LIST_ITEM) ((PCB *)((int)(ADDRESS) - (int)&((PCB *)0)->LIST_ITEM))


class RRScheduler {
private:
    const List* allPrograms;
    List readyPrograms;
public:
    void initialize(const List& allPrograms);
    void enqueue(PCB*);

    PCB* pickNext();
    void MESA_Wakeup(PCB*);
    //
    // 返回是否需要 schedule
    //
    bool onTick(PCB* running);
};

class FIFSScheduler {
private:
    const List* allPrograms;
    List readyPrograms;
public:
    void initialize(const List& allPrograms);
    void enqueue(PCB*);

    PCB* pickNext();
    void MESA_Wakeup(PCB*);
    //
    // 返回是否需要 schedule
    //
    bool onTick(PCB* running);    
};

#endif