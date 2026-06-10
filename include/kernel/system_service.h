#ifndef SYSTEM_SERVICE_H
#define SYSTEM_SERVICE_H

#include "os_constant.h"
#include "os_type.h"
#include "enum.h"
#include "thread.h"

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
int k_pte_dump(uint32 vaddr);
int syscall_pte_dump(uint32 vaddr);

void k_pa_dump();
void syscall_pa_dump();

// 第1个系统调用, write
int k_write(const char *str);
int syscall_write(const char *str);

// 第2个系统调用, fork
int k_fork();
int syscall_fork();

// 第3个系统调用, exit
void k_exit(int ret);
void syscall_exit(int ret);

// 第4个系统调用, wait
int k_wait(int *retval);
int k_waitpid(int pid, int *retval);
int syscall_wait(int pid, int *retval);

// 第5个系统调用, move cursor
void k_move_cursor(int i, int j);
void syscall_move_cursor(int i, int j);

uint16 k_getpid();
uint16 syscall_getpid();

uint16 k_getppid();
uint16 syscall_getppid();

void k_yield();
void syscall_yield();

void k_execveFunc(uint32 func);
void syscall_execveFunc(uint32 func);

#endif