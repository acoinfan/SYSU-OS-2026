#include "scheduler.h"
#include "debug.h"

// RRSchdeuler
void RRScheduler::initialize(const List& _allPrograms) {
    readyPrograms.clear();
    readyPrograms.initialize();
    allPrograms = &_allPrograms;
    LOG_TRACE("initializing RRScheduler\n");
    for (ListItem* item = allPrograms->head.next; item != allPrograms->back(); item = item->next) {
        PCB* pcb = ListItem2PCB(item, tagInAllList);
        if (pcb->status == ProgramStatus::READY) {
            readyPrograms.push_back(&(pcb->tagInGeneralList));
        }
    }
    LOG_TRACE("RRScheduler Init done\n");
}
void RRScheduler::enqueue(PCB* readyThread) {
    if (readyThread->status != ProgramStatus::READY) {
        LOG_ERROR("Invalid Enqueue for boundary test");
    }
    readyPrograms.push_back(&(readyThread->tagInGeneralList));
    return;
}

PCB* RRScheduler::pickNext() {
    ListItem* item = readyPrograms.front();
    readyPrograms.pop_front();
    if (!item) return nullptr;
    return ListItem2PCB(item, tagInGeneralList);
}

bool RRScheduler::onTick(PCB* running) {
    if (!running)
    {
        // 没有线程在运行，可能是 idle
        return true;
    }

    if (running->status != ProgramStatus::RUNNING)
    {
        return true;
    }

    // 时间片减少
    running->ticks--;
    running->ticksPassedBy++;

    // 时间片耗尽，需要调度
    if (running->ticks <= 0)
    {
        return true;
    }

    // 还没用完时间片，不切换
    return false;
}

void RRScheduler::MESA_Wakeup(PCB* program) {
    readyPrograms.push_front(&(program->tagInGeneralList));
}

// FIFSScheduler
void FIFSScheduler::initialize(const List& _allPrograms) {
    readyPrograms.clear();
    readyPrograms.initialize();
    allPrograms = &_allPrograms;
    LOG_TRACE("initializing FIFSScheduler\n");
    for (ListItem* item = allPrograms->head.next; item != allPrograms->back(); item = item->next) {
        PCB* pcb = ListItem2PCB(item, tagInAllList);
        if (pcb->status == ProgramStatus::READY) {
            readyPrograms.push_back(&(pcb->tagInGeneralList));
        }
    }
    LOG_TRACE("FIFSScheduler Init done\n");
}
void FIFSScheduler::enqueue(PCB* readyThread) {
    if (readyThread->status != ProgramStatus::READY) {
        LOG_ERROR("Invalid Enqueue for boundary test");
    }
    readyPrograms.push_back(&(readyThread->tagInGeneralList));
    return;
}

PCB* FIFSScheduler::pickNext() {
    ListItem* item = readyPrograms.front();
    readyPrograms.pop_front();
    if (!item) return nullptr;
    return ListItem2PCB(item, tagInGeneralList);
}

bool FIFSScheduler::onTick(PCB* running) {
    if (!running)
    {
        // 没有线程在运行，可能是 idle
        return true;
    }

    if (running->status != ProgramStatus::RUNNING)
    {
        return true;
    }

    // 还没用完时间片，不切换
    return false;
}

void FIFSScheduler::MESA_Wakeup(PCB* program) {
    readyPrograms.push_front(&(program->tagInGeneralList));
}