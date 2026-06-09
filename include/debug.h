#ifndef DEBUG_H
#define DEBUG_H

#include "printf.h"
#define DEBUG_LEVEL 2  // 0: 关闭所有非必要调试; 1: 只保留报错; 2: 开启INFO; 3: 详细追踪


#define PANIC(fmt, ...) do { \
    asm volatile ("cli"); \
    printf("[PANIC] [%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    while(1) { asm volatile ("hlt"); } \
    } while(0);

#if DEBUG_LEVEL >= 3
    #define LOG_TRACE(fmt, ...) printf("[TRACE] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_TRACE(fmt, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= 2
    #define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_INFO(fmt, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= 1
    #define LOG_ERROR(fmt, ...) printf("[ERROR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define LOG_ERROR(fmt, ...) ((void)0)
#endif

#endif