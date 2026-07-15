#include "scheduler.h"
#include "stdlib.h"
#include "interrupt.h"
#include "asm_utils.h"
#include "screen.h"
#include "thread.h"
#include "os_modules.h"
#include "system_service.h"
#include "debug.h"

namespace {

constexpr uint32 EI_NIDENT = 16;
constexpr uint8 EI_CLASS = 4;
constexpr uint8 EI_DATA = 5;
constexpr uint8 ELFCLASS32 = 1;
constexpr uint8 ELFDATA2LSB = 1;
constexpr uint16 ET_EXEC = 2;
constexpr uint16 EM_386 = 3;
constexpr uint32 EV_CURRENT = 1;
constexpr uint32 PT_LOAD = 1;
constexpr uint32 PF_X = 1;
constexpr uint32 PF_W = 2;

struct Elf32_Ehdr {
    uint8 e_ident[EI_NIDENT];
    uint16 e_type;
    uint16 e_machine;
    uint32 e_version;
    uint32 e_entry;
    uint32 e_phoff;
    uint32 e_shoff;
    uint32 e_flags;
    uint16 e_ehsize;
    uint16 e_phentsize;
    uint16 e_phnum;
    uint16 e_shentsize;
    uint16 e_shnum;
    uint16 e_shstrndx;
} __attribute__((packed));

struct Elf32_Phdr {
    uint32 p_type;
    uint32 p_offset;
    uint32 p_vaddr;
    uint32 p_paddr;
    uint32 p_filesz;
    uint32 p_memsz;
    uint32 p_flags;
    uint32 p_align;
} __attribute__((packed));

static uint32 align_down(uint32 value, uint32 align) {
    return value & ~(align - 1);
}

static uint32 align_up(uint32 value, uint32 align) {
    return (value + align - 1) & ~(align - 1);
}

static bool add_elf_segment(ELFConfig& elfConf, UserSegment userSegment, uint32 vaddr,
                            uint32 file_vaddr, uint32 memsz, uint32 filesz,
                            uint32 offset, VPageFlags flags) {
    if (memsz == 0) {
        return true;
    }
    if (elfConf.segment_count >= ELF_SEGMENT_AMOUNT) {
        LOG_ERROR("ELF has too many loadable segments");
        return false;
    }

    ElfSegment& seg = elfConf.segments[elfConf.segment_count++];
    seg.userSegment = userSegment;
    seg.vaddr = vaddr;
    seg.file_vaddr = file_vaddr;
    seg.memsz = memsz;
    seg.filesz = filesz;
    seg.offset = offset;
    seg.flags = flags;
    return true;
}

static void update_boundary(Boundary& boundary, uint32 start, uint32 end) {
    if (boundary.start > boundary.end) {
        boundary.start = start;
        boundary.end = end;
        return;
    }
    boundary.start = min(boundary.start, start);
    boundary.end = max(boundary.end, end);
}

static bool map_process_page(PCB* process, uint32 vaddr, uint32 paddr, VPageFlags flags) {
    uint32 pgdir = process->pageDirectoryAddress;
    uint32 pde_idx = vaddr >> 22;
    uint32* pde = (uint32*)(pgdir + pde_idx * sizeof(uint32));
    uint32 pte_table_paddr = 0;

    if (!((*pde) & PDE_PRESENT)) {
        pte_table_paddr = memoryManager.allocatePageTable();
        if (!pte_table_paddr) {
            return false;
        }

        *pde = pte_table_paddr | PDE_USER;
        uint32 pte_table_vaddr = memoryManager.mapTemp(AddressPoolType::KERNEL, pte_table_paddr);
        if (!pte_table_vaddr) {
            return false;
        }
        memset((void*)pte_table_vaddr, 0, PAGE_SIZE);
        memoryManager.unmapTemp(AddressPoolType::KERNEL);
    }

    pte_table_paddr = *pde & PTE_GET_ADDRESS;
    uint32 pte_idx = (vaddr >> 12) & 0x3ff;
    uint32 pte_paddr = pte_table_paddr + pte_idx * sizeof(uint32);
    uint32 pte_vaddr = memoryManager.mapTemp(AddressPoolType::KERNEL, pte_paddr);
    if (!pte_vaddr) {
        return false;
    }

    if (*(uint32*)pte_vaddr & PTE_PRESENT) {
        memoryManager.unmapTemp(AddressPoolType::KERNEL);
        return false;
    }

    *(uint32*)pte_vaddr = paddr | vPageFlags2PTE(flags) | PTE_PRESENT;
    memoryManager.PDEinc((uint32)pde);
    memoryManager.unmapTemp(AddressPoolType::KERNEL);

    int attach_idx = memoryManager.rmapManager.attach(&memoryManager.pageinfos[PA2PGI(paddr)],
                                                      pte_paddr, PTE_ANON_VADDR, process->pid);
    if (attach_idx == -1) {
        pte_vaddr = memoryManager.mapTemp(AddressPoolType::KERNEL, pte_paddr);
        if (pte_vaddr) {
            *(uint32*)pte_vaddr = 0;
            memoryManager.unmapTemp(AddressPoolType::KERNEL);
        }
        memoryManager.PDEdec((uint32)pde);
        return false;
    }
    return true;
}

static bool release_process_page(PCB* process, uint32 vaddr) {
    uint32 pgdir = process->pageDirectoryAddress;
    uint32 pde_idx = vaddr >> 22;
    uint32* pde = (uint32*)(pgdir + pde_idx * sizeof(uint32));
    if (!((*pde) & PDE_PRESENT)) {
        return false;
    }

    uint32 pte_table_paddr = *pde & PTE_GET_ADDRESS;
    uint32 pte_idx = (vaddr >> 12) & 0x3ff;
    uint32 pte_paddr = pte_table_paddr + pte_idx * sizeof(uint32);
    uint32 pte_vaddr = memoryManager.mapTemp(AddressPoolType::KERNEL, pte_paddr);
    if (!pte_vaddr) {
        return false;
    }

    uint32 pte_value = *(uint32*)pte_vaddr;
    if (!(pte_value & PTE_PRESENT)) {
        memoryManager.unmapTemp(AddressPoolType::KERNEL);
        return false;
    }

    uint32 paddr = pte_value & PTE_GET_ADDRESS;
    *(uint32*)pte_vaddr = 0;
    memoryManager.unmapTemp(AddressPoolType::KERNEL);
    memoryManager.rmapManager.detach(&memoryManager.pageinfos[PA2PGI(paddr)],
                                     pte_paddr, 0, process->pid);
    memoryManager.releasePhysicalPages(AddressPoolType::USER, paddr, 1);
    memoryManager.PDEdec((uint32)pde);
    return true;
}

static void rollback_loaded_segments(PCB* process, const ELFConfig& elfConf) {
    for (uint32 i = 0; i < elfConf.segment_count; i++) {
        const ElfSegment& seg = elfConf.segments[i];
        uint32 page_begin = align_down(seg.vaddr, PAGE_SIZE);
        uint32 page_end = align_up(seg.vaddr + seg.memsz, PAGE_SIZE);
        for (uint32 vaddr = page_begin; vaddr < page_end; vaddr += PAGE_SIZE) {
            release_process_page(process, vaddr);
        }
    }
}

static bool load_segment_page(int fd, const ElfSegment& seg, uint32 page_vaddr,
                              uint32 page_paddr, void* io_buffer) {
    if (!io_buffer) {
        return false;
    }

    memset(io_buffer, 0, PAGE_SIZE);

    uint32 seg_file_begin = seg.file_vaddr;
    uint32 seg_file_end = seg.file_vaddr + seg.filesz;
    uint32 page_begin = page_vaddr;
    uint32 page_end = page_vaddr + PAGE_SIZE;
    uint32 copy_begin = max(page_begin, seg_file_begin);
    uint32 copy_end = min(page_end, seg_file_end);

    if (copy_begin < copy_end) {
        uint32 chunk = copy_end - copy_begin;
        if (fileManager.fseek(fd, seg.offset + copy_begin - seg.file_vaddr, 0) < 0) {
            return false;
        }
        int ret = fileManager.read(fd, (void*)((uint32)io_buffer + copy_begin - page_begin), chunk);
        if (ret != (int)chunk) {
            return false;
        }
    }

    uint32 page_kaddr = memoryManager.mapTemp(AddressPoolType::KERNEL, page_paddr);
    if (!page_kaddr) {
        return false;
    }

    memcpy((void*)page_kaddr, io_buffer, PAGE_SIZE);
    memoryManager.unmapTemp(AddressPoolType::KERNEL);
    return true;
}

} // namespace

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

int ProgramManager::executeThread(ThreadFunction function, void *parameter, const char *name, int priority, bool needEnqueue)
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

    thread->status = ProgramStatus::CREATED;
    thread->priority = priority;
    thread->ticks = priority * 10;
    thread->ticksPassedBy = 0;
    thread->pid = ((int)thread - (int)PCB_SET) / PCB_SIZE;
    thread->parentPid = programManager.running ? programManager.running->pid: 0;
    fileManager.init_process_fs(thread);

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
    if (needEnqueue) {
        thread->status = ProgramStatus::READY;
        switch (sType) {
            case SchedulerType::RR:
                rrScheduler.enqueue(thread);
                break;
            case SchedulerType::FIFS:
                fifsScheduler.enqueue(thread);
                break;
        }
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
    
    bool isDEAD = false;
    if (running->status == ProgramStatus::DEAD)
    {
        isDEAD = true;
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

    // 事实上, 系统在正常情况下, 由于存在pid0和pid1, 这个必然成功
    if (next)
    {
        LOG_TRACE("switch pid%d to pid%d\n", cur->pid, next->pid);
        next->status = ProgramStatus::RUNNING;
        if (!isDEAD) {
            cur->status = ProgramStatus::READY;
            switch (sType) {
                case SchedulerType::RR:
                    // TODO: 更新对应的值
                    rrScheduler.enqueue(cur);
                    break;
                case SchedulerType::FIFS:
                    fifsScheduler.enqueue(cur);
                    break;
            }
        }
        running = next;
        activateProgramPage(next);

        // execve 拷贝
        if (next->needExecveReload) {
            uint32 total_stack_size = sizeof(ProcessStartStack) + sizeof(ThreadStartStack);
            ASSERT(total_stack_size < (PCB_SIZE - 1024));

            next->stack = (int *)((uint32)next + PCB_SIZE - total_stack_size);

            ThreadStartStack* threadStartStack = (ThreadStartStack*)next->stack;
            memset(threadStartStack, 0, total_stack_size);
            threadStartStack->edi = 0;
            threadStartStack->esi = 0;
            threadStartStack->ebx = 0;
            threadStartStack->ebp = 0;
            threadStartStack->function_entry = (int)load_process;
            threadStartStack->return_address = (int)program_exit;
            threadStartStack->parameter = (int)next->entry;
            next->entry = nullptr;
            next->needExecveReload = false;
        }
        asm_switch_thread(cur, next);
    } else {
        // 不允许DEAD进程不切换, 事实上除了只有pid=0时, 其他情况不应该走到该分支
        if (isDEAD) {
            PANIC("Invalid Schedule");
        }
    }

    interruptManager.setInterruptStatus(status);
}

void program_exit()
{
    PCB *thread = programManager.running;
    fileManager.release_process_fs(thread);
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
    fileManager.release_process_fs(program);
    int index = ((int)program - (int)PCB_SET) / PCB_SIZE;
    PCB_SET_STATUS[index] = false;
    this->allPrograms.erase(&(program->tagInAllList));
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
        LOG_TRACE("  seg[%d]: type=%d vaddr=0x%x file_vaddr=0x%x memsz=%u filesz=%u offset=%u flags=0x%x",
               i, (int)seg.userSegment, seg.vaddr, seg.file_vaddr, seg.memsz, seg.filesz, seg.offset, (uint32)seg.flags);
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

int ProgramManager::executeProcess(const char *filename, int priority, int mode, char** argv, char** envp) 
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
    if (!elfConf.entry) {
        interruptManager.setInterruptStatus(status);
        return -1;
    }
    dumpELFConfig(elfConf);

    // 在线程创建的基础上初步创建进程的PCB
    int pid = executeThread((ThreadFunction)load_process,
                            (void*)elfConf.entry, filename, priority, false);

    if (pid == -1)
    {
        if (elfConf.source_fd >= 0) {
            fileManager.close(elfConf.source_fd);
        }
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    // 找到刚刚创建的PCB
    PCB *process = ListItem2PCB(allPrograms.back(), tagInAllList);

    // 创建进程的页目录表
    process->pageDirectoryAddress = createProcessPageDirectory(process->pid);
    if (!process->pageDirectoryAddress)
    {
        process->status = ProgramStatus::DEAD;
        if (elfConf.source_fd >= 0) {
            fileManager.close(elfConf.source_fd);
        }
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    // 创建进程的虚拟地址池
    bool res = createUserVirtualPool(process, elfConf, mode);
    LOG_TRACE("create res: %d, pid: %d\n", res, pid);

    if (!res)
    {
        process->status = ProgramStatus::DEAD;
        if (elfConf.source_fd >= 0) {
            fileManager.close(elfConf.source_fd);
        }
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    if (elfConf.source_fd >= 0) {
        fileManager.close(elfConf.source_fd);
    }

    // 最后加入队列
    process->status = ProgramStatus::READY;
    switch (sType) {
        case SchedulerType::RR:
            rrScheduler.enqueue(process);
            break;
        case SchedulerType::FIFS:
            fifsScheduler.enqueue(process);
            break;
    }
    
    interruptManager.setInterruptStatus(status);

    return pid;
}

int ProgramManager::createProcessPageDirectory(uint32 owner)
{
    // 从内核地址池中分配一页存储用户进程的页目录表
    int vaddr = memoryManager.allocatePageDirTable(owner);
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
    ((int *)vaddr)[1023] = memoryManager.vaddr2paddr(vaddr) | PDE_RESERVE;
    
    return vaddr;
}

bool ProgramManager::createUserVirtualPool(PCB *process, const ELFConfig& elfConf, int mode) {
    // PCB* saved = programManager.running;
    // // 预先进入对应页表做设置
    // activateProgramPage(process);
    // 计算BitMap Node等大小, 并预先分配对应KernelVA
    SegBoundary segBoundary = {};
    segBoundary.text.start = elfConf.heap_begin;
    segBoundary.text.end = elfConf.heap_begin - 1;
    segBoundary.data.start = elfConf.heap_begin;
    segBoundary.data.end = elfConf.heap_begin - 1;
    segBoundary.bss.start = elfConf.heap_begin;
    segBoundary.bss.end = elfConf.heap_begin - 1;

    // 固定Segment分配和初始化
    for (int i = 0; i < elfConf.segment_count; i++) {
        switch (elfConf.segments[i].userSegment) {
            case UserSegment::TEXT: {
                update_boundary(segBoundary.text, elfConf.segments[i].vaddr,
                                elfConf.segments[i].vaddr + elfConf.segments[i].memsz - 1);
                break;
            }
            case UserSegment::DATA: {
                update_boundary(segBoundary.data, elfConf.segments[i].vaddr,
                                elfConf.segments[i].vaddr + elfConf.segments[i].memsz - 1);
                break;                
            }
            case UserSegment::BSS: {
                update_boundary(segBoundary.bss, elfConf.segments[i].vaddr,
                                elfConf.segments[i].vaddr + elfConf.segments[i].memsz - 1);
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
        bool initRes = process->userVirtual.initialize(segBoundary, heapConf, stackConf,
                                    mmapConf, TLSConf, process->pid);
        if (!initRes) return false;

        void* io_buffer = (void*)memoryManager.allocatePages(AddressPoolType::KERNEL, 1, VP_RW);
        if (!io_buffer) {
            process->userVirtual.destroy();
            return false;
        }

        for (uint32 i = 0; i < elfConf.segment_count; i++) {
            const ElfSegment& seg = elfConf.segments[i];
            uint32 page_begin = align_down(seg.vaddr, PAGE_SIZE);
            uint32 page_end = align_up(seg.vaddr + seg.memsz, PAGE_SIZE);
            for (uint32 vaddr = page_begin; vaddr < page_end; vaddr += PAGE_SIZE) {
                uint32 paddr = memoryManager.allocatePhysicalPages(AddressPoolType::USER, 1);
                if (!paddr) {
                    rollback_loaded_segments(process, elfConf);
                    process->userVirtual.destroy();
                    memoryManager.releasePages(AddressPoolType::KERNEL, (uint32)io_buffer, 1);
                    return false;
                }

                if (!load_segment_page(elfConf.source_fd, seg, vaddr, paddr, io_buffer)) {
                    memoryManager.releasePhysicalPages(AddressPoolType::USER, paddr, 1);
                    rollback_loaded_segments(process, elfConf);
                    process->userVirtual.destroy();
                    memoryManager.releasePages(AddressPoolType::KERNEL, (uint32)io_buffer, 1);
                    return false;
                }

                if (!map_process_page(process, vaddr, paddr, seg.flags)) {
                    memoryManager.releasePhysicalPages(AddressPoolType::USER, paddr, 1);
                    rollback_loaded_segments(process, elfConf);
                    process->userVirtual.destroy();
                    memoryManager.releasePages(AddressPoolType::KERNEL, (uint32)io_buffer, 1);
                    return false;
                }
            }
        }
        memoryManager.releasePages(AddressPoolType::KERNEL, (uint32)io_buffer, 1);
        return true;
    } else if (mode == 1) {
        // LOAD FUNC
        // initialize userVA
        bool initRes = process->userVirtual.initialize(segBoundary, heapConf, stackConf,
                                    mmapConf, TLSConf, process->pid);
        if (!initRes) return false;

        // copy on write: .text
        uint32 pgdir = process->pageDirectoryAddress;
        uint32 vaddrStart = segBoundary.text.start;
        // 注意,传入的是entry在内核的虚拟地址, 需要转化为物理地址
        uint32 paddrStart = memoryManager.vaddr2paddr(elfConf.entry_in_kernel & ~0xfff);
        uint32 total_bytes = (segBoundary.text.end + 1 - segBoundary.text.start);
        ASSERT(total_bytes % PAGE_SIZE == 0);
        uint32 total_pages = total_bytes / PAGE_SIZE;
        // 遍历涉及的PTE和物理页, 同时设置cow
        bool res = setupCOWPages(pgdir, paddrStart, vaddrStart, total_pages, owner);
        if (!res) {
            // 回收userVirtual
            process->userVirtual.destroy();
        }
        return res;
    } else {
        ASSERT(0);
        asm_halt();
    }
    return false;
}

// 注意, 这里使用的是左闭右开定义
void ProgramManager::func2ELF(ELFConfig& elfConf, const void* entry) {
    memset(&elfConf, 0, sizeof(ELFConfig));
    elfConf.entry = (((uint32)entry) & 0xfff) + USER_VADDR_START;
    elfConf.entry_in_kernel = (uint32)entry;
    elfConf.source_fd = -1;
    
    elfConf.segment_count = 3;   // 仅TEXT, 只读, 5页大小
    elfConf.segments[0].userSegment = UserSegment::TEXT;
    elfConf.segments[0].filesz = PAGE_SIZE * 5;
    elfConf.segments[0].flags = VP_USER;
    elfConf.segments[0].memsz = PAGE_SIZE * 5;
    elfConf.segments[0].offset = 0;
    elfConf.segments[0].vaddr = USER_VADDR_START;
    elfConf.segments[0].file_vaddr = USER_VADDR_START;

    elfConf.segments[1].userSegment = UserSegment::DATA;
    elfConf.segments[1].filesz = 0;
    elfConf.segments[1].flags = (VPageFlags)(VP_USER | VP_RW);
    elfConf.segments[1].memsz = 0;
    elfConf.segments[1].offset = 0;
    elfConf.segments[1].vaddr = USER_VADDR_START + elfConf.segments[0].memsz;
    elfConf.segments[1].file_vaddr = elfConf.segments[1].vaddr;

    elfConf.segments[2].userSegment = UserSegment::BSS;
    elfConf.segments[2].filesz = 0;
    elfConf.segments[2].flags = (VPageFlags)(VP_USER | VP_RW);
    elfConf.segments[2].memsz = 0;
    elfConf.segments[2].offset = 0;
    elfConf.segments[2].vaddr = USER_VADDR_START + elfConf.segments[0].memsz;
    elfConf.segments[2].file_vaddr = elfConf.segments[2].vaddr;

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
    ELFConfig elfConf;
    memset(&elfConf, 0, sizeof(ELFConfig));
    elfConf.source_fd = -1;

    int fd = fileManager.open(filename, 0);
    if (fd < 0) {
        LOG_ERROR("parseELF: cannot open %s", filename);
        return elfConf;
    }

    Elf32_Ehdr ehdr;
    if (fileManager.read(fd, &ehdr, sizeof(ehdr)) != (int)sizeof(ehdr)) {
        LOG_ERROR("parseELF: cannot read ELF header");
        fileManager.close(fd);
        return elfConf;
    }

    if (ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F' ||
        ehdr.e_ident[EI_CLASS] != ELFCLASS32 ||
        ehdr.e_ident[EI_DATA] != ELFDATA2LSB ||
        ehdr.e_type != ET_EXEC ||
        ehdr.e_machine != EM_386 ||
        ehdr.e_version != EV_CURRENT ||
        ehdr.e_phentsize != sizeof(Elf32_Phdr) ||
        ehdr.e_phnum == 0) {
        LOG_ERROR("parseELF: invalid i386 ELF");
        fileManager.close(fd);
        return elfConf;
    }

    elfConf.entry = ehdr.e_entry;
    elfConf.entry_in_kernel = 0;
    elfConf.source_fd = fd;

    uint32 max_static_end = USER_VADDR_START;
    for (uint16 i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr phdr;
        uint32 phoff = ehdr.e_phoff + i * sizeof(Elf32_Phdr);
        if (fileManager.fseek(fd, phoff, 0) < 0 ||
            fileManager.read(fd, &phdr, sizeof(phdr)) != (int)sizeof(phdr)) {
            LOG_ERROR("parseELF: cannot read program header");
            fileManager.close(fd);
            elfConf.source_fd = -1;
            elfConf.entry = 0;
            return elfConf;
        }

        if (phdr.p_type != PT_LOAD) {
            continue;
        }
        if (phdr.p_memsz == 0) {
            continue;
        }
        if (phdr.p_filesz > phdr.p_memsz ||
            phdr.p_vaddr < USER_VADDR_START ||
            phdr.p_vaddr + phdr.p_memsz < phdr.p_vaddr ||
            phdr.p_vaddr + phdr.p_memsz >= STACK_TOP) {
            LOG_ERROR("parseELF: invalid load segment");
            fileManager.close(fd);
            elfConf.source_fd = -1;
            elfConf.entry = 0;
            return elfConf;
        }

        uint32 file_end = phdr.p_vaddr + phdr.p_filesz;
        uint32 mem_end = phdr.p_vaddr + phdr.p_memsz;
        VPageFlags flags = (VPageFlags)(VP_USER | ((phdr.p_flags & PF_W) ? VP_RW : 0));

        if (phdr.p_flags & PF_X) {
            uint32 text_begin = align_down(phdr.p_vaddr, PAGE_SIZE);
            uint32 text_end = align_up(mem_end, PAGE_SIZE);
            if (!add_elf_segment(elfConf, UserSegment::TEXT, text_begin,
                                 phdr.p_vaddr, text_end - text_begin,
                                 phdr.p_filesz, phdr.p_offset, flags)) {
                fileManager.close(fd);
                elfConf.source_fd = -1;
                elfConf.entry = 0;
                return elfConf;
            }
            max_static_end = max(max_static_end, text_end);
            continue;
        }

        uint32 data_begin = align_down(phdr.p_vaddr, PAGE_SIZE);
        uint32 data_end = phdr.p_filesz ? align_up(file_end, PAGE_SIZE) : data_begin;
        if (phdr.p_filesz && data_end > data_begin) {
            if (!add_elf_segment(elfConf, UserSegment::DATA, data_begin,
                                 phdr.p_vaddr, data_end - data_begin,
                                 phdr.p_filesz, phdr.p_offset, flags)) {
                fileManager.close(fd);
                elfConf.source_fd = -1;
                elfConf.entry = 0;
                return elfConf;
            }
            max_static_end = max(max_static_end, data_end);
        }

        uint32 bss_begin = data_end;
        uint32 bss_end = align_up(mem_end, PAGE_SIZE);
        if (bss_begin < bss_end) {
            if (!add_elf_segment(elfConf, UserSegment::BSS, bss_begin,
                                 bss_begin, bss_end - bss_begin, 0, 0,
                                 (VPageFlags)(VP_USER | VP_RW))) {
                fileManager.close(fd);
                elfConf.source_fd = -1;
                elfConf.entry = 0;
                return elfConf;
            }
            max_static_end = max(max_static_end, bss_end);
        }
    }

    if (elfConf.segment_count == 0 ||
        elfConf.entry < USER_VADDR_START ||
        elfConf.entry >= max_static_end) {
        LOG_ERROR("parseELF: no loadable segment or invalid entry");
        fileManager.close(fd);
        elfConf.source_fd = -1;
        elfConf.entry = 0;
        return elfConf;
    }

    elfConf.stack_top = STACK_TOP;
    elfConf.stack_size = STACK_SIZE;
    elfConf.stack_begin = elfConf.stack_top - elfConf.stack_size;
    elfConf.stack_pages = STACK_PREALLOC_PAGE;

    uint32 cnt_end = align_up(max_static_end, PAGE_SIZE);
    elfConf.heap_begin = cnt_end;
    elfConf.heap_size = HEAP_SIZE;
    cnt_end += elfConf.heap_size;

    elfConf.tls_begin = cnt_end;
    elfConf.tls_size = MAX_THREAD_AMOUNT * PAGE_SIZE;
    cnt_end += elfConf.tls_size;

    elfConf.mmap_begin = cnt_end;
    elfConf.mmap_size = elfConf.stack_begin - elfConf.mmap_begin;
    if (elfConf.mmap_begin >= elfConf.stack_begin || elfConf.mmap_size % PAGE_SIZE != 0) {
        LOG_ERROR("parseELF: user layout overflow");
        fileManager.close(fd);
        elfConf.source_fd = -1;
        elfConf.entry = 0;
        return elfConf;
    }

    return elfConf;
}

// 0 for fork
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
        userStack[0] = (int)k_exit;
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

        if (!((*pde) & PDE_PRESENT)) {
            // 从内核物理地址空间中分配一个页表 
            ptePAdir = memoryManager.allocatePageTable();
            if (!ptePAdir) {
                rollbackCOWSetup(pgdir, paddrStart, vaddrStart, i, owner);
                return false;
            } 
            // 使页目录项指向页表, 同时设置权限位
            *pde = ptePAdir | PDE_USER;
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
        
        // 设置自身PTE, 同时增加PDE count
        *(uint32*)pteTempVA = (paddr | PTE_PRESENT | PTE_USER_ACCESS | PTE_COW) & ~PTE_WRITABLE;
        LOG_TRACE("update PTE at vaddr = 0x%x\n", vaddr);
        memoryManager.PDEinc((uint32)pde);
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
                            (void *)0, "fork child", 0, true);
    if (pid == -1)
    {
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    // 找到刚刚创建的PCB
    PCB *process = ListItem2PCB(allPrograms.back(), tagInAllList);
    process->needFork = true;

    // 创建进程的页目录表
    process->pageDirectoryAddress = createProcessPageDirectory(process->pid);

    if (!process->pageDirectoryAddress)
    {
        process->status = ProgramStatus::DEAD;
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    // 复制进程的虚拟地址池
    bool res = process->userVirtual.cloneFrom(parent->userVirtual, process->pid);

    ASSERT(process->userVirtual.stackPool.length);
    if (!res)
    {
        process->status = ProgramStatus::DEAD;
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    // 初始化子进程
    bool flag = copyProcess(parent, process);

    if (!flag)
    {
        process->status = ProgramStatus::DEAD;
        interruptManager.setInterruptStatus(status);
        // 回收UserVirtual
        process->userVirtual.destroy();
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
    fileManager.fork_process_fs(child, parent);
    return true;
}



void ProgramManager::exit(int ret)
{
    // 关中断
    interruptManager.disableInterrupt();
    
    // 第一步，标记PCB状态为`DEAD`并放入返回值。
    PCB *program = this->running;
    program->retValue = ret;
    fileManager.release_process_fs(program);
    program->status = ProgramStatus::DEAD;

    LOG_TRACE("call exit\n");
    // 第二步，如果PCB标识的是进程，则释放进程所占用的物理页、页表、页目录表和虚拟地址池bitmap的空间。
    if (program->pageDirectoryAddress) {
        // 释放页表和物理页
        memoryManager.destroyUserVAPool(&programManager.running->userVirtual, program->pageDirectoryAddress);
        // const UserVAddressPool& userVAPool = programManager.running->userVirtual;
        // memoryManager.releasePages(AddressPoolType::USER, userVAPool.segBoundary.text.start, 
        //                             (userVAPool.segBoundary.text.end - userVAPool.segBoundary.text.start + 1) / PAGE_SIZE,
        //                             UserSegment::TEXT);
        // memoryManager.releasePages(AddressPoolType::USER, userVAPool.segBoundary.data.start, 
        //                             (userVAPool.segBoundary.data.end - userVAPool.segBoundary.data.start + 1) / PAGE_SIZE,
        //                             UserSegment::DATA);
        // memoryManager.releasePages(AddressPoolType::USER, userVAPool.segBoundary.bss.start, 
        //                             (userVAPool.segBoundary.bss.end - userVAPool.segBoundary.bss.start + 1) / PAGE_SIZE,
        //                             UserSegment::BSS);                                         
        // memoryManager.releasePages(AddressPoolType::USER, userVAPool.heapPool.startAddress, 
        //                             userVAPool.heapPool.length, UserSegment::HEAP);
        // memoryManager.releasePages(AddressPoolType::USER, userVAPool.stackPool.startAddress, 
        //                             userVAPool.stackPool.length, UserSegment::STACK);
        // memoryManager.releasePages(AddressPoolType::USER, userVAPool.mmapPool.startAddress, 
        //                             userVAPool.mmapPool.length, UserSegment::MMAP);    
        // memoryManager.releasePages(AddressPoolType::USER, userVAPool.TLSPool.startAddress, 
        //                             userVAPool.TLSPool.length, UserSegment::TLS);

        // for (uint32 vaddr = 0; vaddr <= USER_VADDR_END; vaddr += PAGE_SIZE * PAGE_SIZE) {
        //     uint32 PDEptr = memoryManager.toPDE(vaddr);
        //     if (*(uint32*)PDEptr & PDE_PRESENT) {
        //         memoryManager.releasePageTable(PDEptr);
        //     }
        // }
        // // 释放页目录表
        // memoryManager.releasePageDirTable(program->pageDirectoryAddress);
        
        // // 释放BitMap
        // programManager.running->userVirtual.destroy();
    }

    // 第三步，处理孤儿进程
    ListItem* item = this->allPrograms.head.next;
    PCB* child = nullptr;
    // 查找子进程, 托付给pid 1
    while (item)
    {
        child = ListItem2PCB(item, tagInAllList);
        if (child != running && child->parentPid == this->running->pid)
        {
            child->parentPid = 1;
        }
        item = item->next;
    }


    // DEBUG: 空间占用
    LOG_INFO("At Exit: pid = %d", program->pid);
    LOG_INFO("Kernel Physical Avail Pages: %d", memoryManager.kernelPhysical.dump());
    LOG_INFO("User Physical Avail Pages: %d", memoryManager.userPhysical.dump());
    LOG_INFO("Return Val: %d", program->retValue);
    // 第四步，立即执行线程/进程调度。
    schedule();

    PANIC("Zombie resumed");
}

int ProgramManager::waitpid(int pid, int* retval) {
    PCB *zombie, *pcb;
    ListItem *item;
    bool interrupt, hasChild;

    while (true) {
        interrupt = interruptManager.getInterruptStatus();
        interruptManager.disableInterrupt();

        item = this->allPrograms.head.next;

        // 查找子进程
        hasChild = false;
        zombie = nullptr;
        pcb = nullptr;
        while (item) {
            pcb = ListItem2PCB(item, tagInAllList);
            // 找到子进程
            if (pcb->parentPid == this->running->pid && (pid == -1 || pcb->pid == pid)) {
                hasChild = true;
                // 子进程pid相同
                if (pcb->status == ProgramStatus::DEAD) {
                    zombie = pcb;
                    break;
                }
            }
            item = item->next;
        }


        if (!hasChild) {
            // 未找到对应子进程, 返回-1
            interruptManager.setInterruptStatus(interrupt);
            return -1;            
        } 

        // 找到对应子进程
        if (!zombie) {
            // 找到不可返回子进程, 阻塞, 接着扫描
            interruptManager.setInterruptStatus(interrupt);
            schedule();
            continue;
        }


        // 找到可返回子进程, 回收并设置retVal  
        if (retval) {
            *retval = zombie->retValue;
        }

        int childPid = zombie->pid;
        releasePCB(zombie);
        interruptManager.setInterruptStatus(interrupt);
        return childPid;                
    }
}

uint16 ProgramManager::getpid() const {
    return this->running->pid;
}

uint16 ProgramManager::getppid() const {
    return this->running->parentPid;
}

int ProgramManager::execve(const char *filename, char *const argv[], char *const envp[], int mode) {
    bool status = interruptManager.getInterruptStatus();
    int priority = 1;
    interruptManager.disableInterrupt();

    ELFConfig elfConf;
    uint32 pageDirAddr = 0;
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
    if (!elfConf.entry) {
        interruptManager.setInterruptStatus(status);
        return -1;
    }
    dumpELFConfig(elfConf);

    // 使用现有进程PCB
    PCB *process = programManager.running;
    fileManager.exec_process_fs(process);

    
    // 清除原有的页目录表, 页表, 虚拟地址池, 同时切换为内核页表
    ASSERT(process->pageDirectoryAddress);
    memoryManager.destroyUserVAPool(&process->userVirtual, process->pageDirectoryAddress);
    process->pageDirectoryAddress = 0;

    // 创建进程的页目录表
    pageDirAddr = createProcessPageDirectory(process->pid);
    if (!pageDirAddr)
    {
        if (elfConf.source_fd >= 0) {
            fileManager.close(elfConf.source_fd);
        }
        process->status = ProgramStatus::DEAD;
        interruptManager.setInterruptStatus(status);
        return -1;
    }
    process->pageDirectoryAddress = pageDirAddr;
    
    

    // 创建进程的虚拟地址池, 同时初始化部分段内容, 设置COW
    bool res = createUserVirtualPool(process, elfConf, mode);
    LOG_TRACE("create res: %d, pid: %d\n", res, process->pid);

    if (!res)
    {
        if (elfConf.source_fd >= 0) {
            fileManager.close(elfConf.source_fd);
        }
        process->status = ProgramStatus::DEAD;
        interruptManager.setInterruptStatus(status);
        programManager.schedule(); 

        // 理论上 schedule() 换走后就永远不会回来了
        while(1) { asm_halt(); }
    }

    if (elfConf.source_fd >= 0) {
        fileManager.close(elfConf.source_fd);
    }

    // 切换页表
    asm_update_cr3(memoryManager.vaddr2paddr(pageDirAddr));

    // 伪造顶部栈
    uint32 stackPage = programManager.running->userVirtual.stackPool.length;
    uint32 addr = memoryManager.allocatePages(AddressPoolType::USER, 1, 
                                        (VPageFlags)(VP_USER | VP_RW), UserSegment::STACK, true);
    (void)memoryManager.allocatePagesLazy(AddressPoolType::USER, stackPage - 1, 
                                                    (VPageFlags)(VP_USER | VP_RW), UserSegment::STACK, true);
    LOG_TRACE("[load_process] entry=0x%x lazy_stack=0x%x pid=%d\n", (uint32)elfConf.entry, addr, process->pid);

    // 顶部空间设置(returnVal和 TODO: argc, argv)
    uint32 user_esp = STACK_TOP - 4; 
    user_esp -= 12;                  
    int *userStack = (int *)user_esp;
    userStack[0] = (int)k_exit;        // 对应 esp
    userStack[1] = 0;                // 对应 esp + 4 (argc)
    userStack[2] = 0;                // 对应 esp + 8 (argv)

    // 找到当前进程内核栈顶应该放置 ProcessStartStack 的位置
    ProcessStartStack *interruptStack = (ProcessStartStack *)((int)process + PAGE_SIZE - sizeof(ProcessStartStack));

    // 擦除并初始化通用寄存器
    memset(interruptStack, 0, sizeof(ProcessStartStack));

    // 填充段选择子 (用户态平坦模型)
    interruptStack->fs = programManager.USER_DATA_SELECTOR;
    interruptStack->es = programManager.USER_DATA_SELECTOR;
    interruptStack->ds = programManager.USER_DATA_SELECTOR;

    // 极其关键的四大核心寄存器：
    interruptStack->eip = elfConf.entry;          // 新程序的入口点！(来自解析出的 ELF 头)
    interruptStack->cs = programManager.USER_CODE_SELECTOR;
    interruptStack->eflags = (0 << 12) | (1 << 9) | (1 << 1); 

    interruptStack->esp = user_esp;             
    interruptStack->ss = programManager.USER_STACK_SELECTOR;

    // 恢复interrupt
    interruptManager.setInterruptStatus(status);

    // 弹出栈
    asm_start_process((int)interruptStack);

    // SHOULD NEVER REACH HERE
    return process->pid;
}
