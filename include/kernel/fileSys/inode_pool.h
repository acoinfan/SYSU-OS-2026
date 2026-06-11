#ifndef INODE_POOL_H
#define INODE_POOL_H

#include "bitmap.h"
#include "os_modules.h"
#include "debug.h"
#include "stdlib.h"

template<typename T>
class inode_pool {
    BitMap bitmap;
    T* inodes;
    uint32 node_count, total_pages;

public:
    bool initialize(uint32 total_inode) {
        uint32 inodes_total_bytes = total_inode * sizeof(T);
        uint32 bitmap_total_bytes = (total_inode + 7) / 8;
        this->node_count = total_inode;
        this->total_pages = (inodes_total_bytes + bitmap_total_bytes 
                            + PAGE_SIZE - 1) / PAGE_SIZE;
        
        inodes = memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, this->total_pages, VP_RW);
        if (!inodes) {
            LOG_ERROR("Fail to initialize inode_pool\n");
            return false;
        }

        bitmap.initialize(((char*)inodes + inodes_total_bytes), node_count);
        return true;
    }

    void destroy() {
        ASSERT(inodes != 0);
        ASSERT(total_pages != 0);
        memoryManager.releasePages(AddressPoolType::KERNEL, inodes, total_pages);
    }

    T* allocate() {
        int idx = bitmap.allocate(1);
        if (idx == -1) return nullptr;
        else {
            T* ptr = &inodes[idx];
            memset(ptr, 0, sizeof(T));
            return ptr;
        }
    }

    void release(uint32 idx) {
        bitmap.release((int)idx, 1);
    }

    // 传入nullptr则视作根目录(0)
    // TODO 更好的文件名处理
    T* get_inode(T* dir, const char* name) {
        uint16 parent_cluster = dir ? dir->start_cluster : 0;
        for (uint32 idx = 0; idx < node_count; idx++) {
            if (bitmap.get(idx) &&
                inodes[idx].parent_dir_start_cluster == parent_cluster &&
                strcmp(inodes[idx].name, name) == 0) {
                return &inodes[idx];
            }
        }
    }

    // TODO
    T* get_inode(uint32 start_cluster);
};
#endif