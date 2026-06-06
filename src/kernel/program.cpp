#include "scheduler.h"
#include "stdlib.h"
#include "interrupt.h"
#include "asm_utils.h"
#include "stdio.h"
#include "thread.h"
#include "os_modules.h"
#include "syscall.h"
#include "debug.h"

const int PCB_SIZE = 4096;                   // PCB的大小，4KB。
// 存放PCB的数组，预留了MAX_PROGRAM_AMOUNT个PCB的大小空间
char PCB_SET[PCB_SIZE * MAX_PROGRAM_AMOUNT]; 
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
    LOG_TRACE("call Execute Thread\n");
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    // 分配一页作为PCB, 返回清空过的PCB
    PCB *thread = allocatePCB();

    if (!thread)
        return -1;

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
    // 最高的地方留给进程栈
    uint32 total_stack_size = sizeof(ProcessStartStack) + sizeof(ThreadStartStack);
    ASSERT(total_stack_size < (PCB_SIZE - 1024));

    thread->stack = (int *)((uint32)thread + PCB_SIZE - total_stack_size);

    ThreadStartStack* threadStartStack = (ThreadStartStack*)thread->stack;
    threadStartStack->edi = 0;
    threadStartStack->esi = 0;
    threadStartStack->ebx = 0;
    threadStartStack->ebp = 0;
    threadStartStack->function_entry = (int)function;
    threadStartStack->return_address = (int)program_exit;
    threadStartStack->parameter = (int)parameter;

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
    LOG_TRACE("end alloc\n");
    return thread->pid;
}

void ProgramManager::schedule()
{
    LOG_TRACE("Call Schedule\n");
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();
    
    bool isReleased = false;
    if (running->status == ProgramStatus::DEAD)
    {
        releasePCB(running);
        isReleased = true;
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
        LOG_TRACE("switch pid%d to pid%d\n", cur->pid, next->pid);
        next->status = ProgramStatus::RUNNING;
        if (!isReleased) {
            cur->status = ProgramStatus::READY;
            switch (sType) {
                case SchedulerType::RR:
                    rrScheduler.enqueue(cur);
                    break;
                case SchedulerType::FIFS:
                    fifsScheduler.enqueue(cur);
                    break;
    }
        }
        running = next;
        activateProgramPage(next);
        asm_switch_thread(cur, next);
    } else {
        // 不允许释放但不切换
        ASSERT(!isReleased);
    }

    interruptManager.setInterruptStatus(status);
}

void program_exit()
{
    PCB *thread = programManager.running;
    thread->status = ProgramStatus::DEAD;

    if (thread->pid)
    {
        LOG_TRACE("dead schedule\n");
        programManager.schedule();
    }
    else
    {
        interruptManager.disableInterrupt();
        LOG_TRACE("pid0 exit, halt\n");
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
            PCB* res = (PCB*)((int)PCB_SET + PCB_SIZE * i);
            memset(res, 0, PCB_SIZE);
            return res;
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
    LOG_TRACE("ELFConfig:");
    LOG_TRACE("  entry      = 0x%x", elfConf.entry);
    LOG_TRACE("  segment_cnt= %d", elfConf.segment_count);
    for (uint32 i = 0; i < elfConf.segment_count; ++i) {
        const ElfSegment& seg = elfConf.segments[i];
        LOG_TRACE("  seg[%d]: type=%d vaddr=0x%x memsz=%u filesz=%u offset=%u flags=0x%x",
               i, (int)seg.userSegment, seg.vaddr, seg.memsz, seg.filesz, seg.offset, (uint32)seg.flags);
    }
    LOG_TRACE("  stack_top  = 0x%x", elfConf.stack_top);
    LOG_TRACE("  stack_begin= 0x%x", elfConf.stack_begin);
    LOG_TRACE("  stack_size = %u", elfConf.stack_size);
    LOG_TRACE("  stack_pages= %u", elfConf.stack_pages);
    LOG_TRACE("  heap_begin = 0x%x", elfConf.heap_begin);
    LOG_TRACE("  heap_size  = %u", elfConf.heap_size);
    LOG_TRACE("  tls_begin  = 0x%x", elfConf.tls_begin);
    LOG_TRACE("  tls_size   = %u", elfConf.tls_size);
    LOG_TRACE("  mmap_begin = 0x%x", elfConf.mmap_begin);
    LOG_TRACE("  mmap_size  = %u", elfConf.mmap_size);
}

void ProgramManager::MESA_WakeUp(PCB *program) {
    program->status = ProgramStatus::READY;
    LOG_TRACE("wake up program, pid: %d\n", program->pid);
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
    } else {
        ASSERT(0);
        asm_halt();
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
    LOG_TRACE("create res: %d, pid: %d\n", res, pid);

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
        LOG_ERROR("can not create page from kernel\n");
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
    VAPConfigLite heapConf, stackConf, mmapConf, TLSConf;

    heapConf.is_static = true;
    heapConf.start_addr = elfConf.heap_begin;
    heapConf.end_addr = elfConf.heap_begin + elfConf.heap_size - 1;
    heapConf.static_privilege = (VPageFlags)(VP_RW | VP_USER);
    heapConf.length = elfConf.heap_size / PAGE_SIZE;
    
    stackConf.is_static = true;
    stackConf.start_addr = elfConf.stack_begin;
    stackConf.end_addr = elfConf.stack_top - 1;
    stackConf.static_privilege = (VPageFlags)(VP_RW | VP_USER);
    stackConf.length = elfConf.stack_size / PAGE_SIZE;

    mmapConf.is_static = false;
    mmapConf.start_addr = elfConf.mmap_begin;
    mmapConf.end_addr = elfConf.mmap_begin + elfConf.mmap_size - 1;
    mmapConf.static_privilege = VP_CLEAR;
    mmapConf.length = elfConf.mmap_size / PAGE_SIZE;
    
    TLSConf.is_static = false;
    TLSConf.start_addr = elfConf.tls_begin;
    TLSConf.end_addr = elfConf.tls_begin + elfConf.tls_size - 1;
    TLSConf.static_privilege = VP_CLEAR;
    TLSConf.length = elfConf.tls_size / PAGE_SIZE;
    
    // DEBUG:
    LOG_TRACE("Heap Start: 0x%x, Heap End: 0x%x\nHeap Size: 0x%x, Heap Page: %d\n", heapConf.start_addr, heapConf.end_addr ,heapConf.length * PAGE_SIZE, heapConf.length);
    LOG_TRACE("TLS Start: 0x%x, TLS End: 0x%x\nTLS Size: 0x%x, TLS Page: %d\n", TLSConf.start_addr,TLSConf.end_addr, TLSConf.length * PAGE_SIZE, TLSConf.length);
    LOG_TRACE("mmap Start: 0x%x, mmap End: 0x%x\nmmap Size: 0x%x, mmap Page: %d\n", mmapConf.start_addr,mmapConf.end_addr, mmapConf.length * PAGE_SIZE, mmapConf.length);
    LOG_TRACE("Stack Start: 0x%x, Stack End: 0x%x\nStack Size: 0x%x, Stack Page: %d\n", stackConf.start_addr, stackConf.end_addr, stackConf.length * PAGE_SIZE, stackConf.length);

    uint16 owner = process->pid;
    // 创建资源后, 初始化
    if (mode == 0) {
        // TODO: load_from_disk();
    } else if (mode == 1) {
        // LOAD FUNC
        // initialize userVA
        bool initRes = process->userVirtual.initialize(segBoundary, heapConf, stackConf,
                                    mmapConf, TLSConf);
        if (!initRes) return false;

        // copy on write: .text
        uint32 pgdir = process->pageDirectoryAddress;
        uint32 vaddrStart = segBoundary.text.start;
        // 注意,传入的是entry在内核的虚拟地址, 需要转化为物理地址
        uint32 paddrStart = memoryManager.vaddr2paddr(ALIGN(PAGE_SIZE, elfConf.entry));
        uint32 total_bytes = (segBoundary.text.end + 1 - segBoundary.text.start);
        ASSERT(total_bytes % PAGE_SIZE == 0);
        uint32 total_pages = total_bytes / PAGE_SIZE;
        // 遍历涉及的PTE和物理页, 同时设置cow
        bool res = setupCOWPages(pgdir, paddrStart, vaddrStart, total_pages, owner);
        return res;
    } else {
        ASSERT(0);
        asm_halt();
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
        // 内核栈将只会是PCB所在的那个页, 如果要取保存的栈, 从顶部取
        tss.esp0 = (int)program + PAGE_SIZE;
        paddr = memoryManager.vaddr2paddr(program->pageDirectoryAddress);
    }

    asm_update_cr3(paddr);
}

// TODO
ELFConfig ProgramManager::parseELF(const char* filename) {
    return {};
}

// 传0代表是fork
void load_process(const void *entry)
{
    interruptManager.disableInterrupt();

    PCB *process = programManager.running;
    ProcessStartStack *interruptStack = (ProcessStartStack *)((int)process + PAGE_SIZE - sizeof(ProcessStartStack));
    
    if (entry) {
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

        interruptStack->esp = STACK_TOP - 4;
        interruptStack->ss = programManager.USER_STACK_SELECTOR;
        // 懒初始化用户栈空间
        uint32 stackPage = programManager.running->userVirtual.stackPool.length;
        uint32 addr = memoryManager.allocatePages(AddressPoolType::USER, 1, 
                                         (VPageFlags)(VP_USER | VP_RW), UserSegment::STACK, true);
        (void)memoryManager.allocatePagesLazy(AddressPoolType::USER, stackPage - 1, 
                                                        (VPageFlags)(VP_USER | VP_RW), UserSegment::STACK, true);
        LOG_TRACE("[load_process] entry=0x%x lazy_stack=0x%x pid=%d\n", (uint32)entry, addr, process->pid);
        int *userStack = (int *)interruptStack->esp;
        userStack -= 3;
        userStack[0] = (int)exit;
        userStack[1] = 0;
        userStack[2] = 0;
        interruptStack->esp = (uint32)userStack; 
    } else {
        
        LOG_TRACE("load fork from load_process, pid=%d\n", process->pid);
    }
    // 创建进程,直接lazyAlloc栈, 否则就使用默认COW
    asm_start_process((int)interruptStack);
}


bool ProgramManager::setupCOWPages(const uint32 pgdir, const uint32 paddrStart, 
                const uint32 vaddrStart, const uint32 count, const uint16 owner) {
    // 遍历涉及的PTE和物理页, 同时设置cow
    // pgdir: 页目录表根在kernel中的虚拟地址

    ASSERT(paddrStart % PAGE_SIZE == 0);
    ASSERT(vaddrStart % PAGE_SIZE == 0);
    for (uint32 i = 0; i < count; i++) {
        uint32 paddr = paddrStart + i * PAGE_SIZE;
        uint32 vaddr = vaddrStart + i * PAGE_SIZE;

        uint32 pde_idx = vaddr >> 22;
        uint32* pde = (uint32*)(pgdir + 4 * pde_idx);

        // ptePAdir: 页表根在kernel的物理地址, pteVAdir同理
        uint32 ptePAdir = 0, pteVAdir = 0;

        if (!((*pde) & 0x00000001)) {
            // 从内核物理地址空间中分配一个页表 
            ptePAdir = memoryManager.allocatePhysicalPages(AddressPoolType::KERNEL, 1);
            if (!ptePAdir) {
                rollbackCOWSetup(pgdir, paddrStart, vaddrStart, i, owner);
                return false;
            } else {
                memoryManager.pageinfos[PA2PGI(ptePAdir)].clear();
                memoryManager.pageinfos[PA2PGI(ptePAdir)].incRef();
                memoryManager.pageinfos[PA2PGI(ptePAdir)].setFlag(PG_KERNEL | PG_LOCKED);
            }
            // 使页目录项指向页表, 同时设置权限位
            *pde = ptePAdir | 0x7;
            // 初始化页表
            pteVAdir = memoryManager.mapTemp(AddressPoolType::KERNEL, ptePAdir);
            // 严重错误, 无法恢复
            ASSERT(pteVAdir && pteVAdir % PAGE_SIZE == 0);             
            memset((void*)pteVAdir, 0, PAGE_SIZE);
            memoryManager.unmapTemp(AddressPoolType::KERNEL);
        }

        // 设置其他指向该Physical Page的PTE的COW
        bool res = memoryManager.setCOW(&memoryManager.pageinfos[PA2PGI(paddr)]);

        // 设置自身的COW
        ptePAdir = *pde & PTE_GET_ADDRESS;
        uint32 pte_idx = (vaddr >> 12) & 0x3FF;
        // ptePA: 对应pte项在kernelPA的物理地址, pteTempVA: 基于ptePA临时分配VA
        uint32 ptePA = ptePAdir + (pte_idx << 2);
        uint32 pteTempVA = memoryManager.mapTemp(AddressPoolType::KERNEL, ptePA);   // tmp是临时指向pte的指针
        
        // 若分配失败, 释放
        if (!res || !pteTempVA) {
            if (pteTempVA) {
                // 若临时map成功,需要释放
                memoryManager.unmapTemp(AddressPoolType::KERNEL);
            }
            // 清理前序分配
            rollbackCOWSetup(pgdir, paddrStart, vaddrStart, i, owner);
            return false;
        }
        
        // 设置自身PTE
        *(uint32*)pteTempVA = (paddr | PTE_PRESENT | PTE_USER_ACCESS | PTE_COW) & ~PTE_WRITABLE;
        memoryManager.unmapTemp(AddressPoolType::KERNEL);

        // 插入rmap, 如果当前进程=owner, 必须要传真pte-vaddr(可以利用toPTE得到), 否则传入ANON
        int attach_idx = -1;
        if (programManager.running && programManager.running->pid == owner) {
            attach_idx = memoryManager.rmapManager.attach(&memoryManager.pageinfos[PA2PGI(paddr)],
                                            ptePA, memoryManager.toPTE(vaddr), owner);    
        } else {
            attach_idx = memoryManager.rmapManager.attach(&memoryManager.pageinfos[PA2PGI(paddr)],
                                            ptePA, PTE_ANON_VADDR, owner);                
        }
        LOG_TRACE("[COW] owner=%d vaddr=0x%x paddr=0x%x attach_idx=%d ref=%u\n",
                owner, vaddr, paddr, attach_idx, memoryManager.pageinfos[PA2PGI(paddr)].getRef());
    }
    return true;    
}

void ProgramManager::rollbackCOWSetup(uint32 pgdir, uint32 paddrStart, uint32 vaddrStart, 
                                      uint32 count, uint16 owner) {
    for (uint32 i = 0; i < count; i++) {
        // 关键是计算pte_paddr
        uint32 vaddr = vaddrStart + i * PAGE_SIZE;
        uint32 paddr = paddrStart + i * PAGE_SIZE;

        uint32 pde_idx = vaddr >> 22;
        uint32* pde = (uint32*)(pgdir + 4 * pde_idx);
        uint32 ptePAdir = *pde & PTE_GET_ADDRESS;
        uint32 pte_idx = (vaddr >> 12) & 0x3FF;
        uint32 ptePA = ptePAdir + (pte_idx << 2);
        memoryManager.rmapManager.detach(&memoryManager.pageinfos[PA2PGI(paddr)], 
                                        ptePA, 0, owner);
    }    
}

int ProgramManager::fork()
{
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    // 禁止内核线程调用
    PCB *parent = this->running;
    if (!parent->pageDirectoryAddress)
    {
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    // 创建子进程
    // 在线程创建的基础上初步创建进程的PCB
    int pid = executeThread((ThreadFunction)load_process,
                            (void *)0, "fork child", 0);
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

    // 复制进程的虚拟地址池
    bool res = process->userVirtual.cloneFrom(parent->userVirtual);

    ASSERT(process->userVirtual.stackPool.length);
    if (!res)
    {
        process->status = ProgramStatus::DEAD;
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    // 初始化子进程
    PCB *child = ListItem2PCB(this->allPrograms.back(), tagInAllList);
    bool flag = copyProcess(parent, child);

    if (!flag)
    {
        child->status = ProgramStatus::DEAD;
        interruptManager.setInterruptStatus(status);
        // TODO: 记得回收Process
        return -1;
    }

    interruptManager.setInterruptStatus(status);
    return pid;
}

bool ProgramManager::copyProcess(PCB* parent, PCB* child) 
{
    ASSERT(programManager.running->pid == parent->pid);
    LOG_TRACE("[fork] copyProcess parent=%d child=%d\n", parent->pid, child->pid);
    // 复制PCB
    ProcessStartStack *childpps = (ProcessStartStack*) ((uint32)child + PAGE_SIZE - sizeof(ProcessStartStack));
    ProcessStartStack *parentpps = (ProcessStartStack*) ((uint32)parent + PAGE_SIZE - sizeof(ProcessStartStack));
    memcpy(childpps, parentpps, sizeof(ProcessStartStack));
    childpps->eax = 0;

    child->stack = (int *)childpps - 7;
    child->stack[0] = 0;
    child->stack[1] = 0;
    child->stack[2] = 0;
    child->stack[3] = 0;
    child->stack[4] = (int)asm_start_process;
    child->stack[5] = 0;             // asm_start_process 返回地址
    child->stack[6] = (int)childpps; // asm_start_process 参数

    child->status = ProgramStatus::READY;
    child->parentPid = parent->pid;
    child->priority = parent->priority;
    child->ticks = parent->ticks;
    child->ticksPassedBy = parent->ticksPassedBy;
    strcpy(child->name, parent->name);

    // 子进程页目录表指针(虚拟地址)
    int *childPageDir = (int *)child->pageDirectoryAddress;
    // 父进程页目录表指针(虚拟地址)
    int *parentPageDir = (int *)parent->pageDirectoryAddress;

    // 高位PDE在createPageDirectory时已处理
    // 清除低位PDE
    memset((void*)child->pageDirectoryAddress, 0, 768 * sizeof(uint32));
    // 遍历User空间, 全部COW
    uint32 pgdir = child->pageDirectoryAddress;
    for (uint32 vaddr = USER_VADDR_START; vaddr < USER_VADDR_END; vaddr += PAGE_SIZE) {
        // 只需要保证paddr存在
        uint32* parentPDE = (uint32*)memoryManager.toPDE(vaddr);
        if (!(*parentPDE & PTE_PRESENT)) continue;
        uint32* parentPTE = (uint32*)memoryManager.toPTE(vaddr);
        if (!(*parentPTE & PTE_PRESENT)) continue;

        uint32 paddr = *parentPTE & PTE_GET_ADDRESS;
        LOG_TRACE("[fork] map candidate vaddr=0x%x pte=0x%x paddr=0x%x\n",
                vaddr, *parentPTE, paddr);
        if (!setupCOWPages(pgdir, paddr, vaddr, 1, child->pid)) {
            // 直接放弃, 已分配页表回收, 交给Process相关的事情去做
            for (uint32 vaddr_clear = USER_VADDR_START; vaddr_clear < vaddr; vaddr_clear += PAGE_SIZE) {
                uint32 paddr_clear = *((uint32*)memoryManager.toPTE(vaddr_clear)) & PTE_GET_ADDRESS;
                rollbackCOWSetup(pgdir, paddr_clear, vaddr_clear, 1, child->pid);
            }
            return false;
        }
    }
    return true;
}



void ProgramManager::exit(int ret)
{
    // 关中断
    interruptManager.disableInterrupt();
    
    // 第一步，标记PCB状态为`DEAD`并放入返回值。
    PCB *program = this->running;
    program->retValue = ret;
    program->status = ProgramStatus::DEAD;

    LOG_TRACE("call exit\n");
    // 第二步，如果PCB标识的是进程，则释放进程所占用的物理页、页表、页目录表和虚拟地址池bitmap的空间。
    if (program->pageDirectoryAddress) {
        // 用一个无效地址debug
        program->pageDirectoryAddress = 0xffff;
    }

    // 第三步，立即执行线程/进程调度。
    schedule();
}
