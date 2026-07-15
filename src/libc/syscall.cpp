#include "syscall.h"
#include "os_type.h"
#include "enum.h"
#include "stdlib.h"

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
    if (!buffer) {
        return -1;
    }
    return write(1, buffer, strlen(buffer));
}

int pte_dump(uint32 vaddr) {
    return (int)_syscall1(SYS_PTE_DUMP, vaddr);
}

void execveFunc(uint32 func_addr) {
    _syscall1(SYS_EXECFUNC, func_addr);
}

int execve(const char* path) {
    return (int)_syscall1(SYS_EXEC, (uint32)path);
}

uint32 expandHeap(uint32 pageCount) {
    return _syscall1(SYS_EXPANDHEAP, pageCount);
}

int close(int fd) {
    return (int)_syscall1(SYS_CLOSE, (uint32)fd);
}

// 2 Args
int waitpid(int pid, int* retval) {
    return (int)_syscall2(SYS_WAIT, (uint32)pid, (uint32)retval);
}

void move_cursor(int row, int col) {
    _syscall2(SYS_MOVE_CURSOR, (uint32)row, (uint32)col);
}

int open(const char* path, int flags) {
    return (int)_syscall2(SYS_OPEN, (uint32)path, (uint32)flags);
}

int read(int fd, void* buf, int size) {
    return (int)_syscall3(SYS_READ, (uint32)fd, (uint32)buf, (uint32)size);
}

int write(int fd, const void* buf, int size) {
    return (int)_syscall3(SYS_WRITE, (uint32)fd, (uint32)buf, (uint32)size);
}

int fdread(int fd, void* buf, int size) {
    return (int)_syscall3(SYS_FDREAD, (uint32)fd, (uint32)buf, (uint32)size);
}

int fdwrite(int fd, void* buf, int size) {
    return write(fd, buf, size);
}

int fdappend(int fd, void* buf, int size) {
    return (int)_syscall3(SYS_FDAPPEND, (uint32)fd, (uint32)buf, (uint32)size);
}

int create_file(const char* path, int flags) {
    return (int)_syscall2(SYS_CREATE_FILE, (uint32)path, (uint32)flags);
}

int remove_file(const char* path) {
    return (int)_syscall1(SYS_REMOVE_FILE, (uint32)path);
}

int fseek(int fd, int bias, int whence) {
    return (int)_syscall3(SYS_FSEEK, (uint32)fd, (uint32)bias, (uint32)whence);
}

void sync() {
    _syscall0(SYS_SYNC);
}

int mkdir(const char* path) {
    return (int)_syscall1(SYS_MKDIR, (uint32)path);
}

int rmdir(const char* path) {
    return (int)_syscall1(SYS_RMDIR, (uint32)path);
}

int fd_dump(int fd) {
    return (int)_syscall1(SYS_FD_DUMP, (uint32)fd);
}

// 对原有函数的复用
int wait(int* retval) { 
    return waitpid(-1, retval); 
}
