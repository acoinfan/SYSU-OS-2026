#ifndef PROGRAM_H
#define PROGRAM_H

#include "list.h"
#include "thread.h"
#include "scheduler.h"

// #define ListItem2PCB(ADDRESS, LIST_ITEM) ((PCB *)((int)(ADDRESS) - (int)&((PCB *)0)->LIST_ITEM))

class ProgramManager
{
public:
    List allPrograms;   // 所有状态的线程/进程的队列
    PCB *running;       // 当前执行的线程

    int USER_CODE_SELECTOR;  // 用户代码段选择子
    int USER_DATA_SELECTOR;  // 用户数据段选择子
    int USER_STACK_SELECTOR; // 用户栈段选择子

    SchedulerType sType;      // 当前 scheduler 类型
    RRScheduler rrScheduler;
    FIFSScheduler fifsScheduler;
public:
    ProgramManager();
    ~ProgramManager();
    
    void initialize(SchedulerType _sType);

    void initializeTSS();
    // 创建一个线程并放入就绪队列

    // function：线程执行的函数
    // parameter：指向函数的参数的指针
    // name：线程的名称
    // priority：线程的优先级

    // 成功，返回pid；失败，返回-1
    int executeThread(ThreadFunction function, void *parameter, const char *name, int priority);

    // 分配一个PCB
    PCB *allocatePCB();
    // 归还一个PCB
    // program：待释放的PCB
    void releasePCB(PCB *program);

    // 执行线程调度
    void schedule();

    // 切换线程调度器
    void switch_scheduler(SchedulerType type);


    //
    void MESA_WakeUp(PCB *program);
};

void program_exit();

#endif
