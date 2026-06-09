#ifndef USER_H
#define USER_H

#include "os_type.h"
#include "enum.h"

// 1. 生成 0 个参数的系统调用
#define DEFINE_USER_STUB_0(syscall_num, func_name, ret_type) \
    static inline ret_type u_##func_name() { \
        uint32 ret; \
        asm volatile("movl %1, %%eax; int $0x80; movl %%eax, %0" : "=m"(ret) : "g"((uint32)(syscall_num)) : "eax"); \
        return (ret_type)ret; \
    }

// 2. 生成 1 个参数的系统调用
#define DEFINE_USER_STUB_1(syscall_num, func_name, ret_type, arg1_type) \
    static inline ret_type u_##func_name(arg1_type arg1) { \
        uint32 ret; \
        asm volatile("movl %1, %%eax; movl %2, %%ebx; int $0x80; movl %%eax, %0" : "=m"(ret) : "g"((uint32)(syscall_num)), "g"((uint32)(arg1)) : "eax", "ebx"); \
        return (ret_type)ret; \
    }

// 3. 生成 2 个参数的系统调用
#define DEFINE_USER_STUB_2(syscall_num, func_name, ret_type, arg1_type, arg2_type) \
    static inline ret_type u_##func_name(arg1_type arg1, arg2_type arg2) { \
        uint32 ret; \
        asm volatile("movl %1, %%eax; movl %2, %%ebx; movl %3, %%ecx; int $0x80; movl %%eax, %0" : "=m"(ret) : "g"((uint32)(syscall_num)), "g"((uint32)(arg1)), "g"((uint32)(arg2)) : "eax", "ebx", "ecx"); \
        return (ret_type)ret; \
    }

DEFINE_USER_STUB_0(SYS_GETPID,       getpid,       uint16)
DEFINE_USER_STUB_0(SYS_GETPPID,      getppid,      uint16)
DEFINE_USER_STUB_0(SYS_FORK,         fork,         int)
DEFINE_USER_STUB_1(SYS_EXIT,         exit,         void,   int)
DEFINE_USER_STUB_1(SYS_WRITE,        write,        int,    const char*)
DEFINE_USER_STUB_2(SYS_WAIT,         waitpid,      int,    int, int*)
DEFINE_USER_STUB_0(SYS_YIELD,        yield,        void)
DEFINE_USER_STUB_2(SYS_MOVE_CURSOR,  move_cursor,  void,   int, int)
DEFINE_USER_STUB_1(SYS_PTE_DUMP,     pte_dump,     int,    uint32)
DEFINE_USER_STUB_0(SYS_PA_DUMP,      pa_dump,      void)
DEFINE_USER_STUB_1(SYS_EXECFUNC,     execveFunc,   void,   uint32)

// 特殊的 wait(retval) 顺风车，单独补一行纯 C 代码即可
static inline int u_wait(int* retval) { return u_waitpid(-1, retval); }

// =================================================================
// 🎭 影子劫持：让上层代码继续伪装成 Linux 标准调用
// =================================================================
#define getpid       u_getpid
#define getppid      u_getppid
#define fork         u_fork
#define exit         u_exit
#define write        u_write
#define wait         u_wait
#define waitpid      u_waitpid
#define yield        u_yield
#define move_cursor  u_move_cursor
#define pte_dump     u_pte_dump
#define pa_dump      u_pa_dump
#define execveFunc   u_execveFunc

#endif