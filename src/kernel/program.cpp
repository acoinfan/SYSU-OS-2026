#include "scheduler.h"
#include "stdlib.h"
#include "interrupt.h"
#include "asm_utils.h"
#include "stdio.h"
#include "thread.h"
#include "os_modules.h"

const int PCB_SIZE = 4096;                   // PCB的大小，4KB。
char PCB_SET[PCB_SIZE * MAX_PROGRAM_AMOUNT]; // 存放PCB的数组，预留了MAX_PROGRAM_AMOUNT个PCB的大小空间。
bool PCB_SET_STATUS[MAX_PROGRAM_AMOUNT];     // PCB的分配状态，true表示已经分配，false表示未分配。

ProgramManager::ProgramManager() {}

ProgramManager::~ProgramManager() {}

void ProgramManager::initialize(SchedulerType _sType)
{
    allPrograms.initialize();
    running = nullptr;
    sType = _sType;
    
    for (int i = 0; i < MAX_PROGRAM_AMOUNT; ++i)
    {
        PCB_SET_STATUS[i] = false;
    }

    switch (sType) {
        case SchedulerType::RR:
            rrScheduler.initialize(allPrograms);
            break;
        case SchedulerType::FIFS:
            fifsScheduler.initialize(allPrograms);
            break;
    }
}

int ProgramManager::executeThread(ThreadFunction function, void *parameter, const char *name, int priority)
{
    // 关中断，防止创建线程的过程被打断
    printf("call Execute Thread\n");
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    // 分配一页作为PCB
    PCB *thread = allocatePCB();

    if (!thread)
        return -1;

    // 初始化分配的页
    memset(thread, 0, PCB_SIZE);

    for (int i = 0; i < MAX_PROGRAM_NAME && name[i]; ++i)
    {
        thread->name[i] = name[i];
    }

    thread->status = ProgramStatus::READY;
    thread->priority = priority;
    thread->ticks = priority * 10;
    thread->ticksPassedBy = 0;
    thread->pid = ((int)thread - (int)PCB_SET) / PCB_SIZE;

    // 线程栈
    thread->stack = (int *)((int)thread + PCB_SIZE);
    thread->stack -= 7;
    thread->stack[0] = 0;
    thread->stack[1] = 0;
    thread->stack[2] = 0;
    thread->stack[3] = 0;
    thread->stack[4] = (int)function;
    thread->stack[5] = (int)program_exit;
    thread->stack[6] = (int)parameter;

    allPrograms.push_back(&(thread->tagInAllList));
    switch (sType) {
        case SchedulerType::RR:
            rrScheduler.enqueue(thread);
            break;
        case SchedulerType::FIFS:
            fifsScheduler.enqueue(thread);
            break;
    }
    

    // 恢复中断
    interruptManager.setInterruptStatus(status);
    printf("end alloc\n");
    return thread->pid;
}

void ProgramManager::schedule()
{
    printf("Call Schedule\n");
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();
    
    if (running->status == ProgramStatus::DEAD)
    {
        releasePCB(running);
    }

    PCB* next;
    switch (sType) {
        case SchedulerType::RR:
            next = rrScheduler.pickNext();
            break;
        case SchedulerType::FIFS:
            next = fifsScheduler.pickNext();
            break;
    }
    PCB* cur = running;

    if (next)
    {
        next->status = ProgramStatus::RUNNING;
        running = next;
        asm_switch_thread(cur, next);
    }

    interruptManager.setInterruptStatus(status);
}

void program_exit()
{
    PCB *thread = programManager.running;
    thread->status = ProgramStatus::DEAD;

    if (thread->pid)
    {
        printf("dead schedule\n");
        programManager.schedule();
    }
    else
    {
        interruptManager.disableInterrupt();
        printf("pid0 exit, halt\n");
        asm_halt();
    }
}

PCB *ProgramManager::allocatePCB()
{
    for (int i = 0; i < MAX_PROGRAM_AMOUNT; ++i)
    {
        if (!PCB_SET_STATUS[i])
        {
            PCB_SET_STATUS[i] = true;
            return (PCB *)((int)PCB_SET + PCB_SIZE * i);
        }
    }

    return nullptr;
}

void ProgramManager::releasePCB(PCB *program)
{
    int index = ((int)program - (int)PCB_SET) / PCB_SIZE;
    PCB_SET_STATUS[index] = false;
}

void ProgramManager::switch_scheduler(SchedulerType _sType) 
{
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();
    if (_sType != sType) {
        sType = _sType;
        switch (sType)
        {
        case SchedulerType::RR:
            rrScheduler.initialize(allPrograms);
            break;
        
        case SchedulerType::FIFS:
            fifsScheduler.initialize(allPrograms);
            break;
        }
    }
    interruptManager.setInterruptStatus(status);
    return;
}

void ProgramManager::MESA_WakeUp(PCB *program) {
    program->status = ProgramStatus::READY;
    //printf("wake up program, pid: %d\n", program->pid);
    switch (sType) {
        case SchedulerType::RR:
            rrScheduler.MESA_Wakeup(program);
            break;
        case SchedulerType::FIFS:
            fifsScheduler.MESA_Wakeup(program);
            break;
    }
}