#ifndef DEBUG_H
#define DEBUG_H

#include "screen.h"
#define DEBUG_LEVEL 0  // 0: 关闭所有非必要调试; 1: 只保留报错; 2: 开启INFO; 3: 详细追踪


#define PANIC(fmt, ...) do { \
    asm volatile ("cli"); \
    kprintf("[PANIC] [%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    while(1) { asm volatile ("hlt"); } \
    } while(0);

#if DEBUG_LEVEL >= 3
    #define LOG_TRACE(fmt, ...) kprintf("[TRACE] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_TRACE(fmt, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= 2
    #define LOG_INFO(fmt, ...) kprintf("[INFO] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_INFO(fmt, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= 1
    #define LOG_ERROR(fmt, ...) kprintf("[ERROR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define LOG_ERROR(fmt, ...) ((void)0)
#endif

void assert_fail(
    const char *expr,
    const char *file,
    int line,
    const char *func
);

#define ASSERT(expr)                                      \
do {                                                      \
    if(!(expr))                                           \
    {                                                     \
        assert_fail(                                      \
            #expr,                                        \
            __FILE__,                                     \
            __LINE__,                                     \
            __func__                                      \
        );                                                \
    }                                                     \
} while(0)



#endif