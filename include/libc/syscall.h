#ifndef SYSCALL_H
#define SYSCALL_H

#include "os_type.h"

#define FSEEK_SET 0
#define FSEEK_CUR 1
#define FSEEK_END 2

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
int write(int fd, const void* buf, int size);

int pte_dump(uint32 vaddr);

void execveFunc(uint32 func_addr);

int execve(const char* path);

uint32 expandHeap(uint32 pageCount);

int close(int fd);

// 2 Args
int waitpid(int pid, int* retval);

void move_cursor(int row, int col);

int open(const char* path, int flags);
int read(int fd, void* buf, int size);
int fdread(int fd, void* buf, int size);
int fdwrite(int fd, void* buf, int size);
int fdappend(int fd, void* buf, int size);
int create_file(const char* path, int flags);
int remove_file(const char* path);
int fseek(int fd, int bias, int whence);
void sync();
int mkdir(const char* path);
int rmdir(const char* path);
int fd_dump(int fd);
int chdir(const char* path);
int getcwd(char* buf, int size);

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
