#ifndef SYSCALL_H
#define SYSCALL_H

#include "os_constant.h"
#include "os_type.h"

class SystemService
{
public:
    SystemService() {}
    void initialize();
    // 设置系统调用，index=系统调用号，function=处理第index个系统调用函数的地址
    bool setSystemCall(int index, int function);
};

// 第0个系统调用
int syscall_0(int first, int second, int third, int forth, int fifth);

// 1
int sys_write(int fd, const char* buf, int len);

// 2
int sys_dump_pte(uint32 vaddr);
#endif