#ifndef ADDRESS_POOL_H
#define ADDRESS_POOL_H

#include "bitmap.h"
#include "os_type.h"
#include "buddy.h"
#include "os_constant.h"

struct PA {};
struct VA {};

template <typename T>
class AddressPool;

template <>
class AddressPool<VA>
{
public:
    BitMap resources;
    int startAddress;

public:
    AddressPool() {}

    // 初始化地址池
    void initialize(char *bitmap, const int length, const int startAddress)
    {
        resources.initialize(bitmap, length);
        this->startAddress = startAddress;
    }

    // 从地址池中分配count个连续页，成功则返回第一个页的地址，失败则返回-1
    int allocate(const int count)
    {
        int start = resources.allocate(count);
        return (start == -1) ? -1 : (start * PAGE_SIZE + startAddress);
    }

    // 释放若干页的空间
    void release(const int address, const int amount)
    {
        resources.release((address - startAddress) / PAGE_SIZE, amount);
    }
};

template <>
class AddressPool<PA>
{
public:
    Buddy resources;
    int startAddress;

public:
    AddressPool() {}

    // 初始化地址池
    void initialize(char *bitmap, const int length, const int startAddress, char* freeBitMap, FreeNode* freeNodes)
    {
        resources.initialize(bitmap, length, freeBitMap, freeNodes);
        this->startAddress = startAddress;
    }

    // 从地址池中分配count个连续页，成功则返回第一个页的地址，失败则返回-1
    int allocate(const int count)
    {
        int start = resources.allocate(count);
        return (start == -1) ? -1 : (start * PAGE_SIZE + startAddress);
    }

    // 释放若干页的空间
    void release(const int address, const int amount)
    {
        resources.release((address - startAddress) / PAGE_SIZE, amount);
    }
};


#endif