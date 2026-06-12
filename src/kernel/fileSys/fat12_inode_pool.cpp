#include "fileSys/fat12_inode_pool.h"

#include "os_modules.h"
#include "debug.h"
#include "stdlib.h"
#include "memory.h"


bool fat12_inode_pool::initialize(uint32 total_inode) {
    uint32 inodes_total_bytes = total_inode * sizeof(fat12_inode);
    uint32 bitmap_total_bytes = (total_inode + 7) / 8;
    this->node_count = total_inode;
    this->total_pages = (inodes_total_bytes + bitmap_total_bytes 
                        + PAGE_SIZE - 1) / PAGE_SIZE;
    
    inodes = (fat12_inode*)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, this->total_pages, VP_RW);
    if (!inodes) {
        LOG_ERROR("Fail to initialize inode_pool\n");
        return false;
    }

    bitmap.initialize(((char*)inodes + inodes_total_bytes), node_count);
    return true;
}

void fat12_inode_pool::destroy() {
    ASSERT(inodes != 0);
    ASSERT(total_pages != 0);
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)inodes, total_pages);
}

fat12_inode* fat12_inode_pool::allocate() {
    int idx = bitmap.allocate(1);
    if (idx == -1) return nullptr;
    else {
        fat12_inode* ptr = &inodes[idx];
        memset(ptr, 0, sizeof(fat12_inode));
        return ptr;
    }
}

void fat12_inode_pool::release(uint32 idx) {
    bitmap.release((int)idx, 1);
}

fat12_inode* fat12_inode_pool::get_inode(uint32 start_cluster) {
    for (uint32 i = 0; i < this->node_count; i++) {
        if (inodes[i].start_cluster == start_cluster) {
            return &inodes[i];
        }
    }
    return nullptr;
}