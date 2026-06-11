#include "fileSys/fat12_fs.h"
#include "fileSys/disk_driver.h"
#include "debug.h"
#include "screen.h"
#include "os_modules.h"

bool fat12_cluster_buffer::read_cluster(FAT12_FS* fs) {
    uint32 sector = fs->cluster2sector(cluster_num);
    for (int i = 0; i < fs->bpb.sectors_per_cluster; i++) {
        if (!ide_read_sector(fs->device, sector + i, buf + i * fs->bpb.bytes_per_sector)) {
            return false;
        }
    }
    dirty = false;
    last_used_time = fs->access_time;
    fs->access_time++;
    return true;
}

bool fat12_cluster_buffer::write_cluster(FAT12_FS* fs) {
    if (!dirty) return true;

    uint32 sector = fs->cluster2sector(cluster_num);
    for (int i = 0; i < fs->bpb.sectors_per_cluster; i++) {
        if (!ide_write_sector(fs->device, sector + i, buf + i * fs->bpb.bytes_per_sector)) {
            return false;
        }
    }
    dirty = false;
    last_used_time = fs->access_time;
    fs->access_time++;
    return true;
}

uint32 FAT12_FS::cluster2sector(uint16 cluster) {
    ASSERT(cluster >= 2);
    ASSERT(data_start_sector != 0);
    ASSERT(bpb.sectors_per_cluster != 0);
    return data_start_sector + (cluster - 2) * bpb.sectors_per_cluster;
}

bool FAT12_FS::mount(IdeDrive disk) {
    // 务必保证fat12_BPB没有做padding
    ASSERT(sizeof(fat12_BPB) == 25);

    // 解析bpb
    if (ide_read_sector(disk, 0, this->tmp_sector_buffer)) {
        return false;
    }

    // 初始化FAT BPB
    fat12_BPB* bpb_start = (fat12_BPB*)((uint32)tmp_sector_buffer + 11U);
    memcpy(&bpb, bpb_start, sizeof(fat12_BPB));

    // 必要的属性判断
    if (bpb.bytes_per_sector != 512) {
        kprintf("Invalid Mount: bytes_per_sector shoule be 512 instead %d\n", bpb.bytes_per_sector);
        return false;
    }
    if (bpb.num_fats != 2) {
        kprintf("Invalid Mount: num_fats shoule be 2 instead %d\n", bpb.num_fats);
        return false;        
    }
    if (bpb.sectors_per_cluster == 0 || bpb.sectors_per_cluster > 128) {
        kprintf("Invalid Mount: sectors_per_cluster shoule be less than 128 instead %d\n", bpb.sectors_per_cluster);
        return false;        
    }
    if ((bpb.sectors_per_cluster & (bpb.sectors_per_cluster-1)) != 0) {
        kprintf("Invalid Mount: sectors_per_cluster shoule be power of 2 instead %d\n", bpb.sectors_per_cluster);
        return false;         
    }
    if (bpb.fat_size_16 % 3 != 0) {
        kprintf("Invalid Mount: fat_size_16 should be divisible by 3 instead %d\n", bpb.fat_size_16);
    }

    this->device = disk;
    this->access_time = 0;

    this->root_sector_count = (bpb.root_entry_count * FAT12_ENTRY_BYTES + bpb.bytes_per_sector - 1) / bpb.bytes_per_sector;

    this->fat_start_sector = bpb.reserved_sector_count;
    this->root_start_sector = fat_start_sector + bpb.num_fats * bpb.fat_size_16;
    this->data_start_sector = root_start_sector + root_sector_count;

    this->cluster_size = bpb.bytes_per_sector * bpb.sectors_per_cluster;

    uint32 total_sector = bpb.total_sectors_16 != 0 ? bpb.total_sectors_16 : bpb.total_sectors_32;
    this->cluster_count = (total_sector - data_start_sector) / bpb.sectors_per_cluster + 2;
    ASSERT((total_sector - data_start_sector) % bpb.sectors_per_cluster == 0);

    // 读取fat_table
    if (!read_fat_table()) {
        return false;
    }

    // 初始化缓存(对于其中缓存的分配,做lazyAlloc)
    if (!init_cache_pool()) {
        return false;
    }

    // 初始化inode_pool
    inode_pool.initialize(FAT12_MAX_INODES);
}

bool FAT12_FS::init_cache_pool() {
    // 初始化缓存(对于其中缓存的分配,做lazyAlloc)
    ASSERT(this->cluster_size != 0);
    memset(cache_pool, 0, sizeof(fat12_cluster_buffer) * FAT12_MAX_CACHE);
    uint32 page_count = (this->cluster_size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint16 i = 0; i < FAT12_MAX_CACHE; i++) {
        cache_pool[i].buf = (char*)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, page_count, VP_RW);
        if (cache_pool[i].buf == 0) {
            destroy_cache_pool();
            return false;
        }
    }    
    return true;
}

void FAT12_FS::destroy_cache_pool() {
    ASSERT(this->cluster_size != 0);
    uint32 page_count = (this->cluster_size + PAGE_SIZE - 1) / PAGE_SIZE;
    // 由于是连续分配, 失败直接返回即可
    for (uint16 i = 0; i < FAT12_MAX_CACHE; i++) {
        if (cache_pool[i].buf != 0) {
            memoryManager.releasePages(AddressPoolType::KERNEL, (int)cache_pool[i].buf, page_count);
        } else {
            break;
        }
    }
}

bool FAT12_FS::read_fat_table() {
    uint16 total_fat_sectors = bpb.fat_size_16;
    uint16 fat1_start_sector = bpb.reserved_sector_count;
    uint32 idx = 0;

    bool res = true;
    // 遍历fat表, 每次读取三个sector
    for (uint16 i = 0; i < total_fat_sectors; i += 3) {
        // 复制到tmp_sector_buffer
        res &= ide_read_sector(device, i + fat1_start_sector, tmp_sector_buffer);
        res &= ide_read_sector(device, i + 1 + fat1_start_sector, 
                        (void*)((uint32)tmp_sector_buffer + FAT12_SECTOR_SIZE));
        res &= ide_read_sector(device, i + 2+ fat1_start_sector,
                        (void*)((uint32)tmp_sector_buffer + FAT12_SECTOR_SIZE * 2));
        
        if (!res) {
            return false;
        }
        
        // 读取512 * 3 / 1.5 = 1024个项
        for (uint16 j = 0; j < 1024; j++, idx++) {
            uint32 offset = j + j/2; // 12bit -> 1.5B
            uint16 value;
            if ((idx & 1) == 0) {
                value = tmp_sector_buffer[offset] | ((tmp_sector_buffer[offset+1] & 0x0F) << 8);
            } else {
                value = (tmp_sector_buffer[offset] >> 4) | (tmp_sector_buffer[offset+1] << 4);
            }
            fat_table[idx] = value;
        }
    }
    return res;
}

bool FAT12_FS::flush_fat_table() {
    uint16 total_fat_sectors = bpb.fat_size_16;
    uint16 fat1_start_sector = bpb.reserved_sector_count;
    uint32 idx = 0;

    bool res = true;
    // 遍历fat表, 每次读取三个sector
    for (uint16 i = 0; i < total_fat_sectors; i += 3) {
        
        // 写回512 * 3 / 1.5 = 1024个项
        for (uint16 j = 0; j < 1024; j++, idx++) {
            uint32 offset = j + j/2; // 12bit -> 1.5B
            uint16 value = fat_table[idx];
            if ((idx & 1) == 0) {
                tmp_sector_buffer[offset] = value & 0xFF;                      // 低8bit
                tmp_sector_buffer[offset+1] = (tmp_sector_buffer[offset+1] & 0xF0) | ((value >> 8) & 0x0F); // 高4bit
            } else {
                tmp_sector_buffer[offset] = (tmp_sector_buffer[offset] & 0x0F) | ((value << 4) & 0xF0);  // 高4bit
                tmp_sector_buffer[offset+1] = (value >> 4) & 0xFF; // 低8bit
            }
        }

        // 写回到disk
        res &= ide_write_sector(device, i + fat1_start_sector, tmp_sector_buffer);
        res &= ide_write_sector(device, i + 1 + fat1_start_sector, 
                        (void*)((uint32)tmp_sector_buffer + FAT12_SECTOR_SIZE));
        res &= ide_write_sector(device, i + 2+ fat1_start_sector,
                        (void*)((uint32)tmp_sector_buffer + FAT12_SECTOR_SIZE * 2));
        
        if (!res) {
            return false;
        }
    }
    return res;    
}

int FAT12_FS::find_cache(uint16 cluster) {
    for (int i = 0; i < FAT12_MAX_CACHE; i++) {
        if (cache_pool[i].cluster_num == cluster) {
            return i;
        }
    }
    return -1;
}

fat12_cluster_buffer* FAT12_FS::get_cache(uint16 cluster) {
    ASSERT(cluster != 0);
    int idx = find_cache(cluster);

    // cache中找到
    if (idx != -1) {
        return &cache_pool[idx];
    }

    // 未在cache中, 尝试找到一个空的(ref = 0)
    int victim = 0;
    uint32 last_access_time = 0xFFFFFFFF;
    for (int i = 0; i < FAT12_MAX_CACHE; i++) {
        if (cache_pool[i].refcount == 0) {
            return &cache_pool[i];
        } 
        if (cache_pool[i].last_used_time < last_access_time) {
            victim = i;
        }
    }

    // flush victim 到磁盘
    if (cache_pool[victim].dirty) {
        if (!cache_pool[victim].write_cluster(this)) {
            PANIC("FAT12_sys: inconsistent write_cluster in cluster %d\n", cache_pool[victim].cluster_num);
        }
    }

    // 更新cache_pool信息
    cache_pool[victim].cluster_num = cluster;
    cache_pool[victim].refcount = 1;
    cache_pool[victim].dirty = false;
    cache_pool[victim].last_used_time = this->access_time;

    // 读取
    if (!cache_pool[victim].read_cluster(this)) {
        PANIC("FAT12_sys: inconsistent read_cluster in cluster %d\n", cache_pool[victim].cluster_num);
    }

    return &cache_pool[victim];
}


uint16 FAT12_FS::allocate_cluster() {
    for (uint16 idx = 2; idx < cluster_count; idx++) {
        if (fat_table[idx] == 0) {
            return idx;
        }
    }
    return 0;
}

void FAT12_FS::free_cluster(uint16 idx) {
    fat_table[idx] = 0;
}

bool FAT12_FS::lookup(fat12_inode* dir, const char* name, fat12_inode* out) {

}