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

    int selector;

    selector = asm_add_global_descriptor(USER_CODE_LOW, USER_CODE_HIGH);
    USER_CODE_SELECTOR = (selector << 3) | 0x3;

    selector = asm_add_global_descriptor(USER_DATA_LOW, USER_DATA_HIGH);
    USER_DATA_SELECTOR = (selector << 3) | 0x3;

    selector = asm_add_global_descriptor(USER_STACK_LOW, USER_STACK_HIGH);
    USER_STACK_SELECTOR = (selector << 3) | 0x3;

    initializeTSS();

    switch (sType) {
        case SchedulerType::RR:
            rrScheduler.initialize(allPrograms);
            break;
        case SchedulerType::FIFS:
            fifsScheduler.initialize(allPrograms);
            break;
    }
}

void ProgramManager::initializeTSS()
{

    int size = sizeof(TSS);
    int address = (int)&tss;

    memset((char *)address, 0, size);
    tss.ss0 = STACK_SELECTOR; // 内核态堆栈段选择子

    int low, high, limit;

    limit = size - 1;
    low = (address << 16) | (limit & 0xff);
    // DPL = 0
    high = (address & 0xff000000) | ((address & 0x00ff0000) >> 16) | ((limit & 0xff00) << 16) | 0x00008900;

    int selector = asm_add_global_descriptor(low, high);
    // RPL = 0
    asm_ltr(selector << 3);
    tss.ioMap = address + size;
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
        activateProgramPage(next);
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

void ProgramManager::dumpELFConfig(const ELFConfig& elfConf) {
    printf("ELFConfig:");
    printf("  entry      = 0x%x", elfConf.entry);
    printf("  segment_cnt= %d", elfConf.segment_count);
    for (uint32 i = 0; i < elfConf.segment_count; ++i) {
        const ElfSegment& seg = elfConf.segments[i];
        printf("  seg[%d]: type=%d vaddr=0x%x memsz=%u filesz=%u offset=%u flags=0x%x",
               i, (int)seg.userSegment, seg.vaddr, seg.memsz, seg.filesz, seg.offset, (uint32)seg.flags);
    }
    printf("  stack_top  = 0x%x", elfConf.stack_top);
    printf("  stack_begin= 0x%x", elfConf.stack_begin);
    printf("  stack_size = %u", elfConf.stack_size);
    printf("  stack_pages= %u", elfConf.stack_pages);
    printf("  heap_begin = 0x%x", elfConf.heap_begin);
    printf("  heap_size  = %u", elfConf.heap_size);
    printf("  tls_begin  = 0x%x", elfConf.tls_begin);
    printf("  tls_size   = %u", elfConf.tls_size);
    printf("  mmap_begin = 0x%x", elfConf.mmap_begin);
    printf("  mmap_size  = %u", elfConf.mmap_size);
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

int ProgramManager::executeProcess(const char *filename, int priority, int mode) 
{
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    ELFConfig elfConf;
    // load ELF
    if (mode == 0) {
        elfConf = parseELF(filename);
    // load Function
    } else if (mode == 1) {
        func2ELF(elfConf, filename);
    }
    dumpELFConfig(elfConf);

    // 在线程创建的基础上初步创建进程的PCB
    int pid = executeThread((ThreadFunction)load_process,
                            (void*)elfConf.entry, filename, priority);
    if (pid == -1)
    {
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    // 找到刚刚创建的PCB
    PCB *process = ListItem2PCB(allPrograms.back(), tagInAllList);

    // 创建进程的页目录表
    process->pageDirectoryAddress = createProcessPageDirectory();
    if (!process->pageDirectoryAddress)
    {
        process->status = ProgramStatus::DEAD;
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    // 创建进程的虚拟地址池
    bool res = createUserVirtualPool(process, elfConf, 1);
    printf("create res: %d, pid: %d\n", res, pid);

    if (!res)
    {
        process->status = ProgramStatus::DEAD;
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    interruptManager.setInterruptStatus(status);

    return pid;
}

int ProgramManager::createProcessPageDirectory()
{
    // 从内核地址池中分配一页存储用户进程的页目录表
    int vaddr = memoryManager.allocatePages(AddressPoolType::KERNEL, 1, VP_RW);
    if (!vaddr)
    {
        //printf("can not create page from kernel\n");
        return 0;
    }

    memset((char *)vaddr, 0, PAGE_SIZE);

    // 复制内核目录项到虚拟地址的高1GB
    int *src = (int *)(0xfffff000 + 0x300 * 4);
    int *dst = (int *)(vaddr + 0x300 * 4);
    for (int i = 0; i < 256; ++i)
    {
        dst[i] = src[i];
    }
    // 用户进程页目录表的最后一项指向用户进程页目录表本身
    ((int *)vaddr)[1023] = memoryManager.vaddr2paddr(vaddr) | 0x7;
    
    return vaddr;
}

bool ProgramManager::createUserVirtualPool(PCB *process, const ELFConfig& elfConf, int mode) {
    // PCB* saved = programManager.running;
    // // 预先进入对应页表做设置
    // activateProgramPage(process);
    // 计算BitMap Node等大小, 并预先分配对应KernelVA
    SegBoundary segBoundary = {};

    // 固定Segment分配和初始化
    for (int i = 0; i < elfConf.segment_count; i++) {
        switch (elfConf.segments[i].userSegment) {
            case UserSegment::TEXT: {
                segBoundary.text.start = elfConf.segments[i].vaddr;
                segBoundary.text.end = segBoundary.text.start + elfConf.segments[i].memsz - 1;
                break;
            }
            case UserSegment::DATA: {
                segBoundary.data.start = elfConf.segments[i].vaddr;
                segBoundary.data.end = segBoundary.data.start + elfConf.segments[i].memsz - 1;
                break;                
            }
            case UserSegment::BSS: {
                segBoundary.bss.start = elfConf.segments[i].vaddr;
                segBoundary.bss.end = segBoundary.bss.start + elfConf.segments[i].memsz - 1;
                break;                
            }
            default:
                continue;
        }
    }

    // 可变Segment分配与初始化, 注意长度-左闭右闭转化
    VAPConfig heapConf, stackConf, mmapConf, TLSConf;

    heapConf.is_static = true;
    heapConf.start_addr = elfConf.heap_begin;
    heapConf.end_addr = elfConf.heap_begin + elfConf.heap_size - 1;
    heapConf.static_privilege = (VPageFlags)(VP_RW | VP_USER);
    heapConf.length = elfConf.heap_size / PAGE_SIZE;
    heapConf.privilegePtr = 0;
    
    stackConf.is_static = true;
    stackConf.start_addr = elfConf.stack_begin;
    stackConf.end_addr = elfConf.stack_top - 1;
    stackConf.static_privilege = (VPageFlags)(VP_RW | VP_USER);
    stackConf.length = elfConf.stack_size / PAGE_SIZE;
    stackConf.privilegePtr = 0;

    mmapConf.is_static = false;
    mmapConf.start_addr = elfConf.mmap_begin;
    mmapConf.end_addr = elfConf.mmap_begin + elfConf.mmap_size - 1;
    mmapConf.static_privilege = VP_CLEAR;
    mmapConf.length = elfConf.mmap_size / PAGE_SIZE;
    mmapConf.privilegePtr = 0;           // 需要申请
    
    TLSConf.is_static = false;
    TLSConf.start_addr = elfConf.tls_begin;
    TLSConf.end_addr = elfConf.tls_begin + elfConf.tls_size - 1;
    TLSConf.static_privilege = VP_CLEAR;
    TLSConf.length = elfConf.tls_size / PAGE_SIZE;
    TLSConf.privilegePtr = 0;
    
    // DEBUG:
    printf("Heap Start: 0x%x, Heap End: 0x%x\nHeap Size: 0x%x, Heap Page: %d\n", heapConf.start_addr, heapConf.end_addr ,heapConf.length * PAGE_SIZE, heapConf.length);
    printf("TLS Start: 0x%x, TLS End: 0x%x\nTLS Size: 0x%x, TLS Page: %d\n", TLSConf.start_addr,TLSConf.end_addr, TLSConf.length * PAGE_SIZE, TLSConf.length);
    printf("mmap Start: 0x%x, mmap End: 0x%x\nmmap Size: 0x%x, mmap Page: %d\n", mmapConf.start_addr,mmapConf.end_addr, mmapConf.length * PAGE_SIZE, mmapConf.length);
    printf("Stack Start: 0x%x, Stack End: 0x%x\nStack Size: 0x%x, Stack Page: %d\n", stackConf.start_addr, stackConf.end_addr, stackConf.length * PAGE_SIZE, stackConf.length);

    // 统一分配: Bitmap连续分配, privilege连续分配
    uint32 heapBitmapSize = (heapConf.length + 7) >> 3;
    uint32 stackBitmapSize = (stackConf.length + 7) >> 3;
    uint32 mmapBitmapSize = (mmapConf.length + 7) >> 3;
    uint32 TLSBitmapSize = (TLSConf.length + 7) >> 3;

    printf("debug1: %d %d %d %d sum: %d\n", heapBitmapSize, stackBitmapSize, mmapBitmapSize, TLSBitmapSize, heapBitmapSize + stackBitmapSize + mmapBitmapSize + TLSBitmapSize);
    uint32 mmapPrivilegesSize = mmapConf.length * sizeof(VPageFlags);
    uint32 TLSPrivilegesSize = TLSConf.length * sizeof(VPageFlags);

    
    uint32 totalBitmapSize = ALIGN(PAGE_SIZE, heapBitmapSize + stackBitmapSize + mmapBitmapSize + TLSBitmapSize);
    uint32 totalPrivilegesSize = ALIGN(PAGE_SIZE, mmapPrivilegesSize + TLSPrivilegesSize);
    printf("debug: %d %d\n", totalBitmapSize, totalPrivilegesSize);
    // printf("debug: %d %d %d %d\n", elfConf.heap_size, elfConf.stack_size, elfConf.mmap_size, elfConf.tls_size);
    ASSERT(totalBitmapSize != 0);
    ASSERT(totalPrivilegesSize != 0);

    // 直接分配, 失败直接返回
    uint32 bitmapStart = memoryManager.allocatePages(AddressPoolType::KERNEL, totalBitmapSize / PAGE_SIZE, VP_RW);
    uint32 privilegesStart = memoryManager.allocatePages(AddressPoolType::KERNEL, totalPrivilegesSize / PAGE_SIZE, VP_RW);

    printf("debug: %d %d %d %d\n", bitmapStart, totalBitmapSize / PAGE_SIZE, privilegesStart, totalPrivilegesSize / PAGE_SIZE);

    if (!bitmapStart || !privilegesStart) {
        memoryManager.releasePages(AddressPoolType::KERNEL, bitmapStart, totalBitmapSize / PAGE_SIZE);
        memoryManager.releasePages(AddressPoolType::KERNEL, privilegesStart, totalPrivilegesSize / PAGE_SIZE);
        return false;
    }

    // 清空原始信息
    memset((void*)bitmapStart, 0, totalBitmapSize);
    memset((void*)privilegesStart, 0, totalPrivilegesSize);

    uint16 owner = process->pid;
    // 创建资源后, 初始化
    if (mode == 0) {
        // TODO: load_from_disk();
    } else if (mode == 1) {
        // LOAD FUNC

        // bitmap
        uint32 cntPtr = bitmapStart;

        heapConf.bitmap = (char*)cntPtr;
        cntPtr += heapBitmapSize;

        stackConf.bitmap = (char*)cntPtr;
        cntPtr += stackBitmapSize;

        mmapConf.bitmap = (char*)cntPtr;
        cntPtr += mmapBitmapSize;

        TLSConf.bitmap = (char*)cntPtr;
        cntPtr += TLSBitmapSize;
        
        // privileges
        mmapConf.privilegePtr = cntPtr;
        cntPtr += mmapPrivilegesSize;

        TLSConf.privilegePtr = cntPtr;
        cntPtr += TLSPrivilegesSize;

        // initialize userVA
        process->userVirtual.initialize(segBoundary, heapConf, stackConf,
                                    mmapConf, TLSConf);
        
        // copy on write: .text
        uint32* pgdir = (uint32*)process->pageDirectoryAddress;
        uint32 vaddr = segBoundary.text.start;
        uint32 paddrStart = ALIGN(elfConf.entry, PAGE_SIZE);

        // 遍历涉及的PTE和物理页, 同时设置cow
        for (uint32 paddr = paddrStart; vaddr < segBoundary.text.end; vaddr += PAGE_SIZE, paddr += PAGE_SIZE) {
            uint32 pde_idx = vaddr >> 22;
            uint32* pde = &pgdir[pde_idx];
            uint32 tmpPTEtableptr = 0;

            if (!((*pde) & 0x00000001)) {
                // 从内核物理地址空间中分配一个页表 
                int page = memoryManager.allocatePhysicalPages(AddressPoolType::KERNEL, 1);
                if (!page) {
                    // 释放已分配
                    // 事实上,由于先设置了其他Physical Page的合法COW
                    // 前序的分配会使得前面的COW状态不对, 这里的ref和rmap没有更新不用清除, 后续的未有操作不用清除
                    for (uint32 paddrClear = paddrStart, vaddrClear = segBoundary.text.start; 
                        paddrClear < paddr; paddrClear += PAGE_SIZE, vaddrClear += PAGE_SIZE) {
                        // 关键是计算pte_paddr
                        uint32 pde_clear_idx = vaddrClear >> 22;
                        uint32* pde_clear = &pgdir[pde_clear_idx];
                        uint32 pteTablePAptr_clear = *pde_clear & PTE_GET_ADDRESS;
                        uint32 pte_clear_idx = (vaddrClear >> 12) & 0x3FF;
                        uint32 pte_clear_paddr = (uint32)(&((uint32*)pteTablePAptr_clear)[pte_clear_idx]);
                        memoryManager.rmapManager.detach(&memoryManager.pageinfos[PA2PGI(paddrClear)], 
                                                        pte_clear_paddr, 0, owner);
                    }
                    return false;
                } else {
                    memoryManager.pageinfos[PA2PGI(page)].clear();
                    memoryManager.pageinfos[PA2PGI(page)].incRef();
                    memoryManager.pageinfos[PA2PGI(page)].setFlag(PG_KERNEL | PG_LOCKED);
                }
                // 使页目录项指向页表, 同时设置权限位
                *pde = page | 0x7;
                // 初始化页表
                tmpPTEtableptr = memoryManager.mapTemp(AddressPoolType::KERNEL, page);
                ASSERT(tmpPTEtableptr);             // 严重错误, 无法恢复
                memset((void*)((tmpPTEtableptr) & 0xfffff000), 0, PAGE_SIZE);
                memoryManager.unmapTemp(AddressPoolType::KERNEL);
            }

            // 设置其他指向该Physical Page的PTE的COW
            bool res = memoryManager.setCOW(&memoryManager.pageinfos[PA2PGI(paddr)]);

        
            // 设置自身的COW Part1
            uint32 pteTablePAptr = *pde & PTE_GET_ADDRESS;
            uint32 tmp = memoryManager.mapTemp(AddressPoolType::KERNEL, pteTablePAptr);
            uint32 pte_idx = (vaddr >> 12) & 0x3FF;
            uint32 pte_paddr = tmp + (pte_idx << 2);
            // 若分配失败, 释放
            // 事实上,由于先设置了其他Physical Page的合法COW
            // 前序的分配会使得前面的COW状态不对, 这里的ref和rmap没有更新不用清除, 后续的未有操作不用清除
            if (!res || !tmp) {
                if (tmp) {
                    memoryManager.unmapTemp(AddressPoolType::KERNEL);
                }
                // 清理前序分配
                for (uint32 paddrClear = paddrStart, vaddrClear = segBoundary.text.start; 
                    paddrClear < paddr; paddrClear += PAGE_SIZE, vaddrClear += PAGE_SIZE) {
                    // 关键是计算pte_paddr
                    uint32 pde_clear_idx = vaddrClear >> 22;
                    uint32* pde_clear = &pgdir[pde_clear_idx];
                    uint32 pteTablePAptr_clear = *pde_clear & PTE_GET_ADDRESS;
                    uint32 pte_clear_idx = (vaddrClear >> 12) & 0x3FF;
                    uint32 pte_clear_paddr = (uint32)(&((uint32*)pteTablePAptr_clear)[pte_clear_idx]);
                    memoryManager.rmapManager.detach(&memoryManager.pageinfos[PA2PGI(paddrClear)], 
                                                    pte_clear_paddr, 0, owner);
                }
                return false;
                
            }
            // 设置自身的COW Part2
            
            // 设置自身PTE
            *(uint32*)tmp = paddr | PTE_PRESENT | PTE_USER_ACCESS | PTE_COW;
            if (pte_paddr)
            memoryManager.unmapTemp(AddressPoolType::KERNEL);
            // 插入rmap, 如果当前进程=owner, 必须要传真pte-vaddr(高一点那个)
            if (programManager.running && programManager.running->pid == owner) {
                memoryManager.rmapManager.attach(&memoryManager.pageinfos[PA2PGI(paddr)],
                                                pte_paddr, memoryManager.toPTE(vaddr), owner);    
            } else {
                memoryManager.rmapManager.attach(&memoryManager.pageinfos[PA2PGI(paddr)],
                                                pte_paddr, PTE_ANON_VADDR, owner);                
            }
        }
        return true;
    } else if (mode == 2) {
        // TODO: FORK LOAD
    } else {
        ASSERT(0);
    }
    // 装载
}

// 注意, 这里使用的是左闭右开定义
void ProgramManager::func2ELF(ELFConfig& elfConf, const void* entry) {
    elfConf.entry = (uint32)entry;
    
    elfConf.segment_count = 3;   // 仅TEXT, 只读, 1页大小
    elfConf.segments[0].userSegment = UserSegment::TEXT;
    elfConf.segments[0].filesz = PAGE_SIZE;
    elfConf.segments[0].flags = VP_USER;
    elfConf.segments[0].memsz = PAGE_SIZE;
    elfConf.segments[0].offset = 0;
    elfConf.segments[0].vaddr = USER_VADDR_START;

    elfConf.segments[1].userSegment = UserSegment::DATA;
    elfConf.segments[1].filesz = 0;
    elfConf.segments[1].flags = (VPageFlags)(VP_USER | VP_RW);
    elfConf.segments[1].memsz = 0;
    elfConf.segments[1].offset = 0;
    elfConf.segments[1].vaddr = USER_VADDR_START + elfConf.segments[0].memsz;

    elfConf.segments[2].userSegment = UserSegment::BSS;
    elfConf.segments[2].filesz = 0;
    elfConf.segments[2].flags = (VPageFlags)(VP_USER | VP_RW);
    elfConf.segments[2].memsz = 0;
    elfConf.segments[2].offset = 0;
    elfConf.segments[2].vaddr = USER_VADDR_START + elfConf.segments[0].memsz;

    // Stack
    elfConf.stack_top = STACK_TOP;
    elfConf.stack_size = STACK_SIZE;
    elfConf.stack_begin = elfConf.stack_top - elfConf.stack_size;
    elfConf.stack_pages = STACK_PREALLOC_PAGE;
    ASSERT(elfConf.stack_pages * PAGE_SIZE <= elfConf.stack_size);


    uint32 cnt_end = USER_VADDR_START;
    for (int i = 0; i < elfConf.segment_count; i++) {
        cnt_end += elfConf.segments[i].memsz;
    }

    // heap 
    elfConf.heap_begin = cnt_end;
    elfConf.heap_size = HEAP_SIZE;
    cnt_end += elfConf.heap_size;
    // tls
    elfConf.tls_begin = cnt_end;
    elfConf.tls_size = MAX_THREAD_AMOUNT * PAGE_SIZE;
    cnt_end += elfConf.tls_size;
    // mmap
    elfConf.mmap_begin = cnt_end;
    elfConf.mmap_size = elfConf.stack_begin - elfConf.mmap_begin;
    
    dumpELFConfig(elfConf);
    // asm_halt();
    ASSERT(elfConf.mmap_size % PAGE_SIZE == 0);

    return;
}

void ProgramManager::activateProgramPage(PCB *program)
{
    int paddr = PAGE_DIRECTORY;

    if (program->pageDirectoryAddress)
    {
        tss.esp0 = (int)program + PAGE_SIZE;
        paddr = memoryManager.vaddr2paddr(program->pageDirectoryAddress);
    }

    asm_update_cr3(paddr);
}

// TODO
ELFConfig ProgramManager::parseELF(const char* filename) {
    return {};
}

void load_process(const void *entry)
{
    interruptManager.disableInterrupt();

    PCB *process = programManager.running;
    ProcessStartStack *interruptStack = (ProcessStartStack *)((int)process + PAGE_SIZE - sizeof(ProcessStartStack));

    interruptStack->edi = 0;
    interruptStack->esi = 0;
    interruptStack->ebp = 0;
    interruptStack->esp_dummy = 0;
    interruptStack->ebx = 0;
    interruptStack->edx = 0;
    interruptStack->ecx = 0;
    interruptStack->eax = 0;

    // 保留字段gs
    interruptStack->gs = 0;
    
    interruptStack->fs = programManager.USER_DATA_SELECTOR;
    interruptStack->es = programManager.USER_DATA_SELECTOR;
    interruptStack->ds = programManager.USER_DATA_SELECTOR;

    interruptStack->eip = (int)entry;
    interruptStack->cs = programManager.USER_CODE_SELECTOR;   // 用户模式平坦模式
    interruptStack->eflags = (0 << 12) | (1 << 9) | (1 << 1); // IOPL, IF = 1 开中断, MBS = 1 默认

    interruptStack->esp = memoryManager.allocatePages(AddressPoolType::USER, 1, (VPageFlags)(VP_USER|VP_RW), UserSegment::STACK);

    if (interruptStack->esp == 0)
    {
        printf("can not build process!\n");
        process->status = ProgramStatus::DEAD;
        asm_halt();
    }
    interruptStack->esp += PAGE_SIZE;
    interruptStack->ss = programManager.USER_STACK_SELECTOR;

    asm_start_process((int)interruptStack);
}

