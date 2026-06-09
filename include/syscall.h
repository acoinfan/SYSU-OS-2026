#ifndef SYSCALL_H
#define SYSCALL_H

#include "os_constant.h"
#include "os_type.h"
#include "enum.h"
#include "thread.h"

// 0 个参数的系统调用宏（如 fork, yield 等）
#define USER_ASM_SYSCALL_0(syscall_num, retval) \
    asm volatile( \
        "movl %1, %%eax\n\t" \
        "int $0x80\n\t" \
        "movl %%eax, %0" \
        : "=m"(retval) \
        : "g"((uint32)(syscall_num)) \
        : "eax" \
    )

// 1 个参数的系统调用宏（如 write, exit, pte_dump 等）
#define USER_ASM_SYSCALL_1(syscall_num, arg1, retval) \
    asm volatile( \
        "movl %1, %%eax\n\t" \
        "movl %2, %%ebx\n\t" \
        "int $0x80\n\t" \
        "movl %%eax, %0" \
        : "=m"(retval) \
        : "g"((uint32)(syscall_num)), "g"((uint32)(arg1)) \
        : "eax", "ebx" \
    )

// 2 个参数的系统调用宏（如 move_cursor, waitpid 等）
#define USER_ASM_SYSCALL_2(syscall_num, arg1, arg2, retval) \
    asm volatile( \
        "movl %1, %%eax\n\t" \
        "movl %2, %%ebx\n\t" \
        "movl %3, %%ecx\n\t" \
        "int $0x80\n\t" \
        "movl %%eax, %0" \
        : "=m"(retval) \
        : "g"((uint32)(syscall_num)), "g"((uint32)(arg1)), "g"((uint32)(arg2)) \
        : "eax", "ebx", "ecx" \
    )

class SystemService
{
public:
    SystemService() {}
    void initialize();
    // 设置系统调用，index=系统调用号，function=处理第index个系统调用函数的地址
    bool setSystemCall(int index, int function);
};


// 暂时未启用
int sys_write(int fd, const char* buf, int len);

// 6
int pte_dump(uint32 vaddr);
int syscall_pte_dump(uint32 vaddr);

void pa_dump();
void syscall_pa_dump();

// 第1个系统调用, write
int write(const char *str);
int syscall_write(const char *str);

// 第2个系统调用, fork
int fork();
int syscall_fork();

// 第3个系统调用, exit
void exit(int ret);
void syscall_exit(int ret);

// 第4个系统调用, wait
int wait(int *retval);
int waitpid(int pid, int *retval);
int syscall_wait(int pid, int *retval);

// 第5个系统调用, move cursor
void move_cursor(int i, int j);
void syscall_move_cursor(int i, int j);

uint16 getpid();
uint16 syscall_getpid();

uint16 getppid();
uint16 syscall_getppid();

void yield();
void syscall_yield();

void execveFunc(uint32 func);
void syscall_execveFunc(uint32 func);

static inline uint16 u_getpid() {
    uint32 ret;
    USER_ASM_SYSCALL_0(SYS_GETPID, ret);
    return (uint16)ret;
}

static inline uint16 u_getppid() {
    uint32 ret;
    USER_ASM_SYSCALL_0(SYS_GETPPID, ret);
    return (uint16)ret;
}

static inline int u_fork() {
    int ret;
    USER_ASM_SYSCALL_0(SYS_FORK, ret);
    return ret;
}

static inline void u_exit(int status) {
    int dummy;
    USER_ASM_SYSCALL_1(SYS_EXIT, status, dummy);
}

static inline int u_write(const char* str) {
    int ret;
    USER_ASM_SYSCALL_1(SYS_WRITE, str, ret);
    return ret;
}

static inline int u_waitpid(int pid, int* retval) {
    int ret;
    USER_ASM_SYSCALL_2(SYS_WAIT, pid, retval, ret);
    return ret;
}

static inline int u_wait(int* retval) {
    return u_waitpid(-1, retval);
}

static inline void u_yield() {
    int dummy;
    USER_ASM_SYSCALL_0(SYS_YIELD, dummy);
}

static inline void u_move_cursor(int i, int j) {
    int dummy;
    USER_ASM_SYSCALL_2(SYS_MOVE_CURSOR, i, j, dummy);
}

static inline int u_pte_dump(uint32 vaddr) {
    int ret;
    USER_ASM_SYSCALL_1(SYS_PTE_DUMP, vaddr, ret);
    return ret;
}

static inline void u_pa_dump() {
    int dummy;
    USER_ASM_SYSCALL_0(SYS_PA_DUMP, dummy);
}

static inline void u_execveFunc(uint32 func) {
    int dummy;
    USER_ASM_SYSCALL_1(SYS_EXECFUNC, func, dummy);
}
#endif