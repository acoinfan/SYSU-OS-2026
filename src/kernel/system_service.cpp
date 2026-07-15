#include "system_service.h"
#include "interrupt.h"
#include "stdlib.h"
#include "asm_utils.h"
#include "os_modules.h"
#include "screen.h"
#include "thread.h"
#include "debug.h"
#include "enum.h"
#include "os_type.h"

int system_call_table[MAX_SYSTEM_CALL];

void SystemService::initialize()
{
    memset((char *)system_call_table, 0, sizeof(int) * MAX_SYSTEM_CALL);
    // 代码段选择子默认是DPL=0的平坦模式代码段选择子，DPL=3，否则用户态程序无法使用该中断描述符
    interruptManager.setInterruptDescriptor(0x80, (uint32)asm_system_call_handler, 3);
    setSystemCall(SYS_GETPID, (int)syscall_getpid);
    setSystemCall(SYS_GETPPID, (int)syscall_getppid);
    setSystemCall(SYS_FORK, (int)syscall_fork);
    setSystemCall(SYS_EXIT, (int)syscall_exit);
    setSystemCall(SYS_WAIT, (int)syscall_wait);
    setSystemCall(SYS_YIELD, (int)syscall_yield);
    setSystemCall(SYS_WRITE, (int)syscall_write);
    setSystemCall(SYS_READ, (int)syscall_read);
    setSystemCall(SYS_OPEN, (int)syscall_open);
    setSystemCall(SYS_CLOSE, (int)syscall_close);
    setSystemCall(SYS_FDREAD, (int)syscall_fdread);
    setSystemCall(SYS_FDWRITE, (int)syscall_fdwrite);
    setSystemCall(SYS_FDAPPEND, (int)syscall_fdappend);
    setSystemCall(SYS_CREATE_FILE, (int)syscall_create_file);
    setSystemCall(SYS_REMOVE_FILE, (int)syscall_remove_file);
    setSystemCall(SYS_FSEEK, (int)syscall_fseek);
    setSystemCall(SYS_SYNC, (int)syscall_sync);
    setSystemCall(SYS_MKDIR, (int)syscall_mkdir);
    setSystemCall(SYS_RMDIR, (int)syscall_rmdir);
    setSystemCall(SYS_FD_DUMP, (int)syscall_fd_dump);
    setSystemCall(SYS_CHDIR, (int)syscall_chdir);
    setSystemCall(SYS_GETCWD, (int)syscall_getcwd);
    setSystemCall(SYS_LS, (int)syscall_ls);
    setSystemCall(SYS_MOVE_CURSOR, (int)syscall_move_cursor);
    setSystemCall(SYS_PTE_DUMP, (int)syscall_pte_dump);
    setSystemCall(SYS_PA_DUMP, (int)syscall_pa_dump);
    setSystemCall(SYS_EXEC, (int)syscall_execve);
    setSystemCall(SYS_EXEC_ARGV, (int)syscall_execveArgv);
    setSystemCall(SYS_EXECFUNC, (int)syscall_execveFunc);
    setSystemCall(SYS_EXPANDHEAP, (int)syscall_expandHeap);
}

bool SystemService::setSystemCall(int index, int function)
{
    system_call_table[index] = function;
    return true;
}


int sys_write(int fd, const char* buf, int len)
{
    if (!buf || len <= 0) return -1;

    for (int i = 0; i < len; ++i) {
        screen.print(buf[i]);  // 直接复用内核的输出
    }
    return len;
}

int k_pte_dump(uint32 vaddr) {
    return asm_system_call(SYS_PTE_DUMP, vaddr);
}

void k_pa_dump() {
    asm_system_call(SYS_PA_DUMP);
}

uint16 k_getpid() {
    return asm_system_call(SYS_GETPID);
}

uint16 k_getppid() {
    return asm_system_call(SYS_GETPPID);
}

int k_fork() {
    return asm_system_call(SYS_FORK);
}

int k_write(const char *str) {
    if (!str) {
        return -1;
    }
    return asm_system_call(SYS_WRITE, 1, (int)str, strlen(str));
}

void k_exit(int ret) {
    asm_system_call(SYS_EXIT, ret);
}

int k_wait(int *retval) {
    return k_waitpid(-1, retval);
}

int k_waitpid(int pid, int *retval) {
    return asm_system_call(SYS_WAIT, pid, (int)retval);
}

void k_move_cursor(int i, int j) {
    asm_system_call(SYS_MOVE_CURSOR, i, j);
}

void k_yield() {
    asm_system_call(SYS_YIELD);
}

void k_execveFunc(uint32 func) {
    asm_system_call(SYS_EXECFUNC, func);
}

int k_execve(const char* filename) {
    return asm_system_call(SYS_EXEC, (int)filename);
}

// ====== syscall function ======
uint16 syscall_getpid() {
    return programManager.getpid();
}

uint16 syscall_getppid() {
    return programManager.getppid();
}

int syscall_write(int fd, void* buf, int size) {
    return fileManager.write(fd, buf, size);
}

int syscall_fork() {
    return programManager.fork();
}

void syscall_exit(int ret) {
    programManager.exit(ret);
}

int syscall_wait(int pid, int *retval) {
    return programManager.waitpid(pid, retval);
}

void syscall_move_cursor(int i, int j) {
    screen.moveCursor(i, j);
}

void syscall_yield() {
    programManager.schedule();
}

void syscall_execveFunc(uint32 func) {
    programManager.execve((char*)func, nullptr, nullptr, 1);
}

int syscall_execve(const char* filename) {
    return programManager.execve(filename, nullptr, nullptr, 0);
}

int syscall_execveArgv(const char* filename, char* const argv[]) {
    return programManager.execve(filename, argv, nullptr, 0);
}

int syscall_pte_dump(uint32 vaddr)
{
    PCB* cur = programManager.running;
    if (!cur) return -1;

    // 计算 PDE 指针（通过内核映射）
    uint32* pde = (uint32*)memoryManager.toPDE(vaddr);
    uint32 pde_val = *pde;

    LOG_TRACE("[sys_dump_pte] pid=%d vaddr=0x%x PDE=0x%x\n",
           cur->pid, vaddr, pde_val);

    if (!(pde_val & PTE_PRESENT)) {
        LOG_ERROR("  PDE not present.\n");
        return 0;
    }

    // 透过 PDE 得到页表物理地址
    uint32 pte_pa_base = pde_val & PTE_GET_ADDRESS;
    // 临时映射这张页表
    uint32 tmp = memoryManager.mapTemp(AddressPoolType::KERNEL, pte_pa_base);
    if (!tmp) {
        LOG_ERROR("  mapTemp failed.\n");
        return -1;
    }
    uint32 pte_idx = (vaddr >> 12) & 0x3FF;
    uint32* pte = (uint32*)(tmp + (pte_idx << 2));

    LOG_TRACE("  PTE[%u]=0x%x\n", pte_idx, *pte);
    uint32 paddr = *pte & PTE_GET_ADDRESS;
    memoryManager.pageinfos[PA2PGI(paddr)].dump();
    memoryManager.unmapTemp(AddressPoolType::KERNEL);
    return 0;
}

void syscall_pa_dump() {
    kprintf("Kernel Physical Avail Pages: %d\n", memoryManager.kernelPhysical.dump());
    kprintf("User Physical Avail Pages: %d\n", memoryManager.userPhysical.dump());    
}

uint32 syscall_expandHeap(uint32 pageCount) {
    return memoryManager.allocatePagesLazy(AddressPoolType::USER, pageCount, 
                                    (VPageFlags)(VP_USER | VP_RW), UserSegment::HEAP);
}

int syscall_open(const char* path, int flags) {
    return fileManager.open(path, flags);
}

int syscall_close(int fd) {
    return fileManager.close(fd);
}

int syscall_read(int fd, void* buf, int size) {
    return fileManager.read(fd, buf, size);
}

int syscall_fdread(int fd, void* buf, int size) {
    return fileManager.read(fd, buf, size);
}

int syscall_fdwrite(int fd, void* buf, int size) {
    return fileManager.write(fd, buf, size);
}

int syscall_fdappend(int fd, void* buf, int size) {
    return fileManager.append(fd, buf, size);
}

int syscall_create_file(const char* path, int flags) {
    return fileManager.create_file(path, flags);
}

int syscall_remove_file(const char* path) {
    return fileManager.remove_file(path);
}

int syscall_fseek(int fd, int bias, int whence) {
    return fileManager.fseek(fd, bias, whence);
}

void syscall_sync() {
    fileManager.sync_all();
}

int syscall_mkdir(const char* path) {
    return fileManager.mkdir(path);
}

int syscall_rmdir(const char* path) {
    return fileManager.rmdir(path);
}

int syscall_fd_dump(int fd) {
    return fileManager.dump_fd(fd);
}

int syscall_chdir(const char* path) {
    return fileManager.chdir(path);
}

int syscall_getcwd(char* buf, int size) {
    return fileManager.getcwd(buf, size);
}

int syscall_ls(const char* path, LsEntry* entries, int max_entries) {
    return fileManager.ls(path, entries, max_entries);
}
