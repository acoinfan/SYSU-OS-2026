#ifndef SYSCALL_H
#define SYSCALL_H

#include "os_type.h"

// 调用
// 0 Args
uint16 getpid();

uint16 getppid();

int fork();

void yield();

void pa_dump();

// 1 Args
void exit(int code);

int write(const char* buffer);

int pte_dump(uint32 vaddr);

void execveFunc(uint32 func_addr);

// 2 Args
int waitpid(int pid, int* retval);

void move_cursor(int row, int col);

// 复用waitpid
int wait(int* retval);

// 接口
static inline uint32 _syscall0(uint32 syscall_num) {
    uint32 ret;
    asm volatile(
        "movl %1, %%eax; int $0x80; movl %%eax, %0"
        : "=m"(ret)
        : "g"(syscall_num)
        : "eax"
    );
    return ret;
}

static inline uint32 _syscall1(uint32 syscall_num, uint32 arg1) {
    uint32 ret;
    asm volatile(
        "movl %1, %%eax; movl %2, %%ebx; int $0x80; movl %%eax, %0"
        : "=m"(ret)
        : "g"(syscall_num), "g"(arg1)
        : "eax", "ebx"
    );
    return ret;
}

static inline uint32 _syscall2(uint32 syscall_num, uint32 arg1, uint32 arg2) {
    uint32 ret;
    asm volatile(
        "movl %1, %%eax; movl %2, %%ebx; movl %3, %%ecx; int $0x80; movl %%eax, %0"
        : "=m"(ret)
        : "g"(syscall_num), "g"(arg1), "g"(arg2)
        : "eax", "ebx", "ecx"
    );
    return ret;
}

static inline uint32 _syscall3(uint32 syscall_num, uint32 arg1, uint32 arg2, uint32 arg3) {
    uint32 ret;
    asm volatile(
        "movl %1, %%eax; movl %2, %%ebx; movl %3, %%ecx; movl %4, %%edx; int $0x80; movl %%eax, %0"
        : "=m"(ret)
        : "g"(syscall_num), "g"(arg1), "g"(arg2), "g"(arg3)
        : "eax", "ebx", "ecx", "edx"
    );
    return ret;
}


#endif