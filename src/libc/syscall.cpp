#include "syscall.h"
#include "os_type.h"
#include "enum.h"

// 调用
// 0 Args
uint16 getpid() {
    return (uint16)_syscall0(SYS_GETPID);
}

uint16 getppid() {
    return (uint16)_syscall0(SYS_GETPPID);
}

int fork() {
    return (int)_syscall0(SYS_FORK);
}

void yield() {
    _syscall0(SYS_YIELD);
}

void pa_dump() {
    _syscall0(SYS_PA_DUMP);
}

// 1 Args
void exit(int code) {
    _syscall1(SYS_EXIT, (uint32)code);
}

int write(const char* buffer) {
    return (int)_syscall1(SYS_WRITE, (uint32)buffer);
}

int pte_dump(uint32 vaddr) {
    return (int)_syscall1(SYS_PTE_DUMP, vaddr);
}

void execveFunc(uint32 func_addr) {
    _syscall1(SYS_EXECFUNC, func_addr);
}

uint32 expandHeap(uint32 pageCount) {
    return _syscall1(SYS_EXPANDHEAP, pageCount);
}

// 2 Args
int waitpid(int pid, int* retval) {
    return (int)_syscall2(SYS_WAIT, (uint32)pid, (uint32)retval);
}

void move_cursor(int row, int col) {
    _syscall2(SYS_MOVE_CURSOR, (uint32)row, (uint32)col);
}

// 对原有函数的复用
int wait(int* retval) { 
    return waitpid(-1, retval); 
}