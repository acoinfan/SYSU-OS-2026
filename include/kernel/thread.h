#ifndef THREAD_H
#define THREAD_H

#include "list.h"
#include "os_constant.h"
#include "enum.h"
#include "address_pool.h"
#include "fileSys/file_manager.h"

typedef void (*ThreadFunction)(void *);

struct ProcessStartStack
{
    // popad
    int edi;
    int esi;
    int ebp;
    int esp_dummy;
    int ebx;
    int edx;
    int ecx;
    int eax;
    
    // pop余下的
    int gs;
    int fs;
    int es;
    int ds;

    // iret初始化的
    int eip;
    int cs;
    int eflags;
    int esp;
    int ss;
};

struct ThreadStartStack {
    // switch_to 期待弹出的 4 个寄存器
    uint32 edi;
    uint32 esi;
    uint32 ebx;
    uint32 ebp;

    // switch_to 执行 ret 时弹出的目标入口
    uint32 function_entry; // 指向 load_process
    uint32 return_address; // 指向 program_exit
    uint32 parameter;      // 传给 load_process 的参数 (elfConf.entry)
};

struct fs_context {
    void*     current_fs;      // ⭐ 当前处于哪个文件系统实例（哪块盘）
    uint16    cwd_cluster;     // 当前盘下的工作目录簇号
    char      cwd_path[256];   // 绝对路径字符串（如 "/mnt/sdb/docs"）
};

#define MAX_ARGC_COUNT 16
#define MAX_ARGV_LENGTH 128

// PCB占一个整页
struct PCB
{
    int *stack;                      // 栈指针，用于调度时保存esp
    char name[MAX_PROGRAM_NAME + 1]; // 线程名
    enum ProgramStatus status;       // 线程的状态
    int priority;                    // 线程优先级
    int pid;                         // 线程pid
    int ticks;                       // 线程时间片总时间
    int ticksPassedBy;               // 线程已执行时间
    ListItem tagInGeneralList;       // 线程队列标识
    ListItem tagInAllList;           // 线程队列标识

    int pageDirectoryAddress;        // 页目录表地址
    UserVAddressPool userVirtual;    // 用户程序虚拟地址池
    int parentPid;            // 父进程pid
    int retValue;             // 返回值

    bool needExecveReload = false;
    bool needFork = false;
    void* entry;
    void* entry_kernel;
    int argc;
    char (*argv)[MAX_ARGV_LENGTH];
    fs_context fs_info;
    File fd_table[MAX_FD_COUNT]; // per-process file descriptor table
};

static_assert(sizeof(PCB) <= PAGE_SIZE, "PCB must fit in one page");
static_assert(__builtin_offsetof(PCB, stack) == 0, "PCB.stack must stay at offset 0");

/*
                       内核栈
+---------------------------------------------------+  <- (uint32)pcb + PAGE_SIZE (页的最顶端), 且 tss.esp0 也永远指向这里
|  ProcessStartStack (初始化的假现场 / 运行时的真现场) |  
+---------------------------------------------------+  <- (uint32)pcb + PAGE_SIZE - sizeof(ProcessStartStack) 
|  ThreadStack (线程栈)                              |  
+---------------------------------------------------+ 
|                                                   |
|                                                   |
|                                                   |
|                                                   |
|                                                   |
|                内核栈向下生长 (栈空间)              |
|                        v                          |
|                                                   |
|                        ^                          |
|               PCB 结构体向上扩展 (堆空间)           |
|                                                   |
+---------------------------------------------------+
|  PCB 结构体变量 (pid, status, pageDirectory...)   |  
+---------------------------------------------------+  <- (uint32)pcb : pcb 指针指向这里 (页的最底端)
*/
#endif
