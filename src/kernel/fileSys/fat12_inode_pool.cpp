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

    memset(inodes, 0, inodes_total_bytes + bitmap_total_bytes);

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

void fat12_inode_pool::release(fat12_inode* ptr) {
    ASSERT(ptr != nullptr);
    int idx = ((uint32)ptr - (uint32)inodes) / sizeof(fat12_inode);
    bitmap.release((int)idx, 1);
}

fat12_inode* fat12_inode_pool::get_inode(uint32 start_cluster) {
    if (start_cluster == 0) return nullptr;
    for (uint32 i = 0; i < this->node_count; i++) {
        if (bitmap.get(i) && inodes[i].start_cluster == start_cluster) {
            return &inodes[i];
        }
    }
    return nullptr;
}

fat12_inode* fat12_inode_pool::get_inode(const fat12_entry_location& location) {
    for (uint32 i = 0; i < this->node_count; i++) {
        if (bitmap.get(i) && inodes[i].start_cluster == 0 && inodes[i].location.is_same(location))
            return &inodes[i];
    }
    return nullptr;
}
