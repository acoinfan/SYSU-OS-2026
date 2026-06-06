#ifndef PROGRAM_H
#define PROGRAM_H

#include "list.h"
#include "thread.h"
#include "scheduler.h"

// #define ListItem2PCB(ADDRESS, LIST_ITEM) ((PCB *)((int)(ADDRESS) - (int)&((PCB *)0)->LIST_ITEM))

struct ElfSegment {
    UserSegment userSegment; // 对应的段
    uint32 vaddr;      // 段起始虚拟地址
    uint32 memsz;      // 段在内存中的长度
    uint32 filesz;     // 文件中实际数据长度
    uint32 offset;     // 文件内偏移
    VPageFlags flags;  // 段权限：读/写/执行
};


// 注意, 这里左闭右开
struct ELFConfig {
    uint32 entry;             // 程序入口
    ElfSegment segments[ELF_SEGMENT_AMOUNT];   // 实际数量可配, 目前包含TEXT DATA BSS
    uint32 segment_count;

    // Stack
    uint32 stack_top;         // 初始栈顶虚拟地址
    uint32 stack_begin;       // 栈底(小地址)
    uint32 stack_size;        // 栈总大小(包括预先分配+懒分配)
    uint32 stack_pages;       // 预先分配页数(起码是1,保证栈能维持基本功能)
    
    // Heap
    uint32 heap_begin;        // HEAP起始地点
    uint32 heap_size;         // HEAP总大小

    // TLS
    uint32 tls_begin;
    uint32 tls_size;
    
    // MMAP
    uint32 mmap_begin;
    uint32 mmap_size;
};

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

    /*
    @param mode 0 for ELF, 1 for Function(debug)
    @param filename ELF filename / Function Address
    */
    int executeProcess(const char *filename, int priority, int mode = 0);
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

    // 初始化进程页目录表
    int createProcessPageDirectory();

    // 初始化进程UserVirtualPool
    bool createUserVirtualPool(PCB *process, const ELFConfig& elfConf, int mode = 0);

    // ELF解析器
    ELFConfig parseELF(const char* filename);

    // Function to ELFConfig 
    void func2ELF(ELFConfig& elfConf, const void* entry);

    void dumpELFConfig(const ELFConfig& elfConf);

    // 页表切换
    void activateProgramPage(PCB *program);
    
    //
    void MESA_WakeUp(PCB *program);

    // fork();
    int fork();

    // execve();
    int execve();

    void exit(int ret);
private:
    // 用于COW过程中, 把paddrStart对应的内容, 复制到owner页表下对应地址为vaddrStart的地方, 总计复制count页
    bool setupCOWPages(const uint32 pgdir, const uint32 paddrStart, const uint32 vaddrStart, 
                       const uint32 count, const uint16 owner);
    // 用于COW过程中, 连续复制页的清除, 释放从idx 0到count - 1总计count个
    void rollbackCOWSetup(uint32 pgdir, uint32 paddrStart, uint32 vaddrStart, uint32 count, uint16 owner);
    
    // COW复制进程
    bool copyProcess(PCB* parent, PCB* child);
};

void program_exit();

void load_process(const void* entry);
#endif
