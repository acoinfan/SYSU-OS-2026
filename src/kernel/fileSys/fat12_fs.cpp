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
    memset(this, 0, sizeof(FAT12_FS));

    // 解析bpb
    if (!ide_read_sector(disk, 0, this->tmp_sector_buffer)) {
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
    if ((bpb.fat_size_16 * bpb.bytes_per_sector )% 3 != 0) {
        kprintf("Invalid Mount: fat_size_16 * bytes_per_sector should be divisible by 3 instead %d\n", bpb.fat_size_16 * bpb.bytes_per_sector);
    }

    this->device = disk;
    this->access_time = 0;

    this->root_entries = bpb.root_entry_count;
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
    this->fat_table_dirty = false;

    // 初始化缓存(对于其中缓存的分配,做lazyAlloc)
    if (!init_cache_pool()) {
        return false;
    }

    // 初始化inode_pool
    inodepool.initialize(FAT12_MAX_INODES);

    // 初始化root_dir
    init_root_dir();
    
    // debug: 打印root_dir信息
    dump_root_dir();
    return true;
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
    // 遍历fat表, 每次读取三个sector, 最后余下的单独读取
    uint16 first_part = total_fat_sectors - total_fat_sectors % 3;
    for (uint16 i = 0; i < first_part; i += 3) {
        // 复制到tmp_sector_buffer
        res &= ide_read_sector(device, i + fat1_start_sector, tmp_sector_buffer);
        res &= ide_read_sector(device, i + 1 + fat1_start_sector, 
                        (void*)((uint32)tmp_sector_buffer + FAT12_SECTOR_SIZE));
        res &= ide_read_sector(device, i + 2+ fat1_start_sector,
                        (void*)((uint32)tmp_sector_buffer + FAT12_SECTOR_SIZE * 2));
        
        if (!res) {
            return false;
        }
        
        // 读取bytes_per_sector * 3 / 1.5个项
        for (uint16 j = 0; j < bpb.bytes_per_sector / 2; j++, idx++) {
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

    uint16 entries_per_sector = bpb.bytes_per_sector * 8 / 12;
    for (uint16 i = first_part; i < total_fat_sectors; i++) {
        res &= ide_read_sector(device, i + fat1_start_sector, tmp_sector_buffer);
        for (uint16 j = 0; j < entries_per_sector; j++, idx++) {
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
    // 不允许读取非法簇
    if (cluster < 2 || cluster >= 0xFF0) return nullptr;
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
            cache_pool[i].cluster_num = cluster;
            cache_pool[i].refcount = 1;
            cache_pool[i].dirty = false;
            cache_pool[i].last_used_time = this->access_time;

            if (!cache_pool[i].read_cluster(this)) {
                PANIC("FAT12_fs: read cluster %u failed\n", cluster);
            }
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

fat12_inode* FAT12_FS::lookup(fat12_inode* dir, const char* name) {
    // 判断是否是根目录
    fat12_entry_buf entry_buf;
    fat12_inode* res = nullptr;
    if (!dir) {
        for (uint32 idx = 0; idx < root_entries; idx++) {
            fat12_normalized_entry* entry = &root_dir[idx];
            if (entry->name[0] == '\0') continue;   // 跳过空条目
            if ((uint8)entry->name[0] == 0xE5) continue;   // 跳过已删除条目
            if (entry->attr & 0x08) continue;       // 跳过卷标
            else if (strcmp(name, entry->name) == 0) {
                uint16 start_cluster = entry->start_cluster;
                res = inodepool.get_inode(start_cluster);
                if (res) {
                    res->refcount++;
                } else {
                    res = inodepool.allocate();
                    if (!res) {
                        kprintf("inode Pool is Full\n");
                        return nullptr;
                    }
                    res->attr = entry->attr;
                    res->fat12_fs = this;
                    res->parent_dir_start_cluster = 0;
                    res->refcount = 1;
                    res->size = entry->file_size;
                    res->start_cluster = start_cluster;
                }
                return res;
            }
        }
        return nullptr;
    } else {
        // 不是对应文件系统, 返回
        if (dir->fat12_fs != this) {
            return nullptr;
        }
        uint16 cnt_cluster = dir->start_cluster;
        // 遍历有效簇
        while (cnt_cluster >= 0x2 && cnt_cluster <= 0xFF7) {
            fat12_cluster_buffer* cache = get_cache(cnt_cluster);
            uint16 total_entries = cluster_size / FAT12_ENTRY_BYTES;
            entry_buf.reset();
            for (uint16 i = 0; i < total_entries; i++) {
                if (entry_buf.read(cache->buf + i * FAT12_ENTRY_BYTES)) {
                    if (strcmp(entry_buf.normalized_entry.name, name) == 0) {
                        fat12_normalized_entry* entry = &entry_buf.normalized_entry;
                        uint16 start_cluster = entry->start_cluster;
                        res = inodepool.get_inode(start_cluster);
                        if (res) {
                            res->refcount++;
                        } else {
                            res = inodepool.allocate();
                            if (!res) {
                                kprintf("inode Pool is Full\n");
                                return nullptr;
                            }
                            res->attr = entry->attr;
                            res->fat12_fs = this;
                            res->parent_dir_start_cluster = 0;
                            res->refcount = 1;
                            res->size = entry->file_size;
                            res->start_cluster = start_cluster;
                        }
                        return res;
                    }
                } else if (entry_buf.status == fat12_entry_buf::fat12_entry_buf_status::END_OF_DIR){
                    return nullptr;
                }

            }
            cnt_cluster = fat_table[cnt_cluster];
        }
        return nullptr;
    }
}

bool FAT12_FS::init_root_dir() {
    fat12_entry_buf entry_buf;
    // 初始化root_table
    uint32 root_bytes = this->root_entries * sizeof(fat12_normalized_entry);
    uint32 root_pages = (root_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    this->root_dir = (fat12_normalized_entry*)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, root_pages, VP_RW);
    if (!this->root_dir) {
        // TODO 记得释放
        return false;
    }

    memset(this->root_dir, 0, root_bytes);

    // 构建root_table
    uint32 end_sector = this->root_sector_count + this->root_start_sector;
    uint32 entry_idx = 0;
    uint32 entry_per_sector = bpb.bytes_per_sector / FAT12_ENTRY_BYTES;
    
    bool finish = false;
    entry_buf.reset();
    for (uint32 sector_idx = this->root_start_sector; sector_idx < end_sector && !finish; sector_idx++) {
        ide_read_sector(device, sector_idx, tmp_sector_buffer);
        // 遍历sector内所有的entry
        for (uint32 i = 0; i < entry_per_sector; i++) {
            if (entry_buf.read(tmp_sector_buffer + FAT12_ENTRY_BYTES * i)) {
                if (entry_idx < this->root_entries) {
                    memcpy(&root_dir[entry_idx], &entry_buf.normalized_entry, sizeof(fat12_normalized_entry));
                    entry_idx++;
                }
            } else if (entry_buf.status == fat12_entry_buf::fat12_entry_buf_status::END_OF_DIR) {
                finish = true;
            }
        }
    }
    this->root_dir_dirty = false;
    return true;
}

void FAT12_FS::dump_root_dir() {
    kprintf("========== ROOT DIRECTORY ==========\n");
    for (uint32 i = 0; i < root_entries; ++i) {
        fat12_normalized_entry* entry = &root_dir[i];
        if (entry->name[0] == '\0') continue;
        if ((uint8)entry->name[0] == 0xE5) continue;

        unsigned int idx   = static_cast<unsigned int>(i);
        unsigned int clust = static_cast<unsigned int>(entry->start_cluster);
        unsigned int size  = static_cast<unsigned int>(entry->file_size);
        unsigned int attr  = static_cast<unsigned int>(entry->attr);
        unsigned int cdate = static_cast<unsigned int>(entry->create_date);
        unsigned int ctime = static_cast<unsigned int>(entry->create_time);
        unsigned int wdate = static_cast<unsigned int>(entry->write_date);
        unsigned int wtime = static_cast<unsigned int>(entry->write_time);

        kprintf(
            "[%03u] %-20s cluster=%u size=%u attr=0x%02x "
            "ctime=%04x/%04x wtime=%04x/%04x\n",
            idx,
            entry->name,
            clust,
            size,
            attr,
            cdate,
            ctime,
            wdate,
            wtime
        );
    }

    kprintf("====================================\n");
}

int FAT12_FS::read(fat12_inode* node, void* buf, int size, int offset) {
    // 参数预检查
    // node
    if (!node || node->start_cluster < 2 || node->fat12_fs != this) 
        return 0;

    // offset:
    if (offset >= node->size) return 0;     // EOF
    else if (offset < 0)      return -1;    // Invalid Offest

    // size:
    if (size <= 0)            return 0;     // Not read
    else if (size + offset > node->size) {
        size = node->size - offset;         // file size limit
    }
    
    // is File:
    if ((node->attr & fat12_attr::DIRECTORY) || (node->attr & fat12_attr::VOLUME_ID)) {
        return -1;                          // Not file
    }

    // 读取内容, 先找到对应位置
    uint16 cnt_cluster = node->start_cluster;
    while (offset >= cluster_size) {
        if (cnt_cluster < 2 || cnt_cluster > 0xFF8) return 0;  // 超出簇链
        cnt_cluster = fat_table[cnt_cluster];
        offset -= cluster_size;
    }

    uint32 read_size = 0;
    // 先读完该簇剩下内容
    // size < 余下内容
    fat12_cluster_buffer* cluster_buf = get_cache(cnt_cluster);
    if (!cluster_buf) return -1;
    uint32 left = cluster_size - offset;
    if (size < left) {
        memcpy(buf, &cluster_buf->buf[offset], size);
        read_size += size;
        return read_size;
    } else {
        memcpy(buf, &cluster_buf->buf[offset], left);
        size -= left;
        read_size += left;
    }

    cnt_cluster = fat_table[cnt_cluster];

    // 连cluster读
    while (size > 0 && cnt_cluster >= 2 && cnt_cluster < 0xFF8) {
        cluster_buf = get_cache(cnt_cluster);
        if (!cluster_buf) return read_size;
        uint16 chunk = min(size, cluster_size);
        memcpy((char*)buf + read_size, cluster_buf->buf, chunk);
        size -= chunk;
        read_size += chunk;
        cnt_cluster = fat_table[cnt_cluster];
    }
    return read_size;
}

int FAT12_FS::write(fat12_inode* node, const void* buf, int size, int offset) {
    // 参数预检查
    // node
    if (!node || node->start_cluster < 2 || node->fat12_fs != this) 
        return 0;

    // offset:
    if (offset >= node->size) return 0;     // EOF
    else if (offset < 0)      return -1;    // Invalid Offest

    // size:
    if (size <= 0)            return 0;     // Not read
    else if (size + offset > node->size) {
        size = node->size - offset;         // file size limit
    }
    
    // is File:
    if ((node->attr & fat12_attr::DIRECTORY) || (node->attr & fat12_attr::VOLUME_ID)) {
        return -1;                          // Not file
    }    

    // 写入内容, 先找到对应位置
    uint16 cnt_cluster = node->start_cluster;
    while (offset >= cluster_size) {
        if (cnt_cluster < 2 || cnt_cluster > 0xFF8) return 0;  // 超出簇链
        cnt_cluster = fat_table[cnt_cluster];
        offset -= cluster_size;
    }

    uint32 write_size = 0;
    // 先写入该簇剩下内容
    // size < 余下内容
    fat12_cluster_buffer* cluster_buf = get_cache(cnt_cluster);
    if (!cluster_buf) return -1;
    uint32 left = cluster_size - offset;
    if (size < left) {
        memcpy(&cluster_buf->buf[offset], buf, size);
        cluster_buf->dirty = true;
        write_size += size;
        return write_size;
    } else {
        memcpy(&cluster_buf->buf[offset], buf, left);
        cluster_buf->dirty = true;
        size -= left;
        write_size += left;
    }

    cnt_cluster = fat_table[cnt_cluster];

    // 连cluster写
    while (size > 0 && cnt_cluster >= 2 && cnt_cluster < 0xFF8) {
        cluster_buf = get_cache(cnt_cluster);
        if (!cluster_buf) return write_size;
        uint16 chunk = min(size, cluster_size);
        memcpy(cluster_buf->buf, (char*)buf + write_size, chunk);
        cluster_buf->dirty = true;
        size -= chunk;
        write_size += chunk;
        cnt_cluster = fat_table[cnt_cluster];
    }
    return write_size;
}

bool FAT12_FS::find_dir_entry(fat12_inode* target, fat12_entry_location* location) {
    // 不是对应文件系统, 返回
    if (!target || target->fat12_fs != this) {
        return false;
    }

    // 判断是否是根目录
    fat12_entry_buf entry_buf;
    if (target->parent_dir_start_cluster == 0) {
        for (uint32 idx = 0; idx < root_entries; idx++) {
            fat12_normalized_entry* entry = &root_dir[idx];
            if (entry->name[0] == '\0') continue;   // 跳过空条目
            if ((uint8)entry->name[0] == 0xE5) continue;   // 跳过已删除条目
            if (entry->attr & 0x08) continue;       // 跳过卷标
            else if (entry->start_cluster == target->start_cluster) {
                location->current_cluster = 0;
                location->entry_index_in_cluster = 0;
                location->is_root = true;
                location->parent_cluster = 0;
                location->root_index = idx;
                return true;
            }
        }
        return false;
    } else {
        uint16 cnt_cluster = target->parent_dir_start_cluster;
        // 遍历有效簇
        while (cnt_cluster >= 0x2 && cnt_cluster <= 0xFF7) {
            fat12_cluster_buffer* cache = get_cache(cnt_cluster);
            uint16 total_entries = cluster_size / FAT12_ENTRY_BYTES;
            entry_buf.reset();
            for (uint16 i = 0; i < total_entries; i++) {
                if (entry_buf.read(cache->buf + i * FAT12_ENTRY_BYTES)) {
                    if (entry_buf.normalized_entry.start_cluster == target->start_cluster) {
                        location->current_cluster = cnt_cluster;
                        location->entry_index_in_cluster = i;
                        location->is_root = false;
                        location->parent_cluster = target->parent_dir_start_cluster;
                        location->root_index = 0;
                        return true;
                    }
                } else if (entry_buf.status == fat12_entry_buf::fat12_entry_buf_status::END_OF_DIR){
                    return false;
                }

            }
            cnt_cluster = fat_table[cnt_cluster];
        }
        return false;
    }
}

int FAT12_FS::find_free_entry(fat12_inode* dir, fat12_entry_location* location) {
    // 不是对应文件系统, 返回
    if (dir && dir->fat12_fs != this) {
        return -1;
    }    

    // 判断是否是根目录
    fat12_entry_buf entry_buf;
    if (!dir) {
        for (uint32 idx = 0; idx < root_entries; idx++) {
            fat12_normalized_entry* entry = &root_dir[idx];
            if (entry->attr & 0x08) continue;       // 跳过卷标
            if (entry->name[0] == '\0' || (uint8)entry->name[0] == 0xE5) {
                location->current_cluster = 0;
                location->entry_index_in_cluster = 0;
                location->is_root = true;
                location->parent_cluster = 0;
                location->root_index = idx;
                return 0;
            }
        }
        return 2;
    } else {
        uint16 cnt_cluster = dir->start_cluster, prev_cluster = dir->start_cluster;
        // 遍历有效簇
        while (cnt_cluster >= 0x2 && cnt_cluster <= 0xFF7) {
            fat12_cluster_buffer* cache = get_cache(cnt_cluster);
            uint16 total_entries = cluster_size / FAT12_ENTRY_BYTES;
            entry_buf.reset();
            for (uint16 i = 0; i < total_entries; i++) {
                entry_buf.read(cache->buf + i * FAT12_ENTRY_BYTES);
                switch (entry_buf.status) {
                    case fat12_entry_buf::fat12_entry_buf_status::DELETE: 
                    case fat12_entry_buf::fat12_entry_buf_status::END_OF_DIR: {
                        location->current_cluster = cnt_cluster;
                        location->entry_index_in_cluster = i;
                        location->is_root = false;
                        location->parent_cluster = dir->start_cluster;
                        location->root_index = 0;
                        return 0;
                    }
                }
            }
            prev_cluster = cnt_cluster;
            cnt_cluster = fat_table[cnt_cluster];
        }
        // 需要扩容, 为方便起见, location->curent_cluster保存最后一个cluster编号
        location->current_cluster = prev_cluster;
        return 1;
    }
}

bool FAT12_FS::write_dir_entry(const fat12_entry_location& location, const fat12_normalized_entry& entry) {
    if (location.is_root) {
        if (location.root_index >= root_entries) return false;
        fat12_normalized_entry* dst_entry = &root_dir[location.root_index];
        root_dir_dirty = true;
        dst_entry->copy(entry);
    } else {
        if (location.entry_index_in_cluster >= cluster_size / FAT12_ENTRY_BYTES) return false;
        if (location.current_cluster < 2 || location.current_cluster > 0xFF7) return false; 
        fat12_cluster_buffer* cache = get_cache(location.current_cluster);
        fat12_entry_buf buf;
        buf.normalized_entry.copy(entry);
        if (!buf.write(cache->buf + location.entry_index_in_cluster * sizeof(fat12_sfn_entry), false)) {
            return false;
        }
        cache->dirty = true;
    }
    return true;
}

bool FAT12_FS::delete_dir_entry(const fat12_entry_location& location) {
    if (location.is_root) {
        if (location.root_index >= root_entries) return false;
        fat12_normalized_entry* dst_entry = &root_dir[location.root_index];
        root_dir_dirty = true;
        dst_entry->name[0] = (uint8)0xE5;
    } else {
        if (location.entry_index_in_cluster >= cluster_size / FAT12_ENTRY_BYTES) return false;
        if (location.current_cluster < 2 || location.current_cluster > 0xFF7) return false; 
        fat12_cluster_buffer* cache = get_cache(location.current_cluster);
        fat12_entry_buf buf;
        if (!buf.write(cache->buf + location.entry_index_in_cluster * sizeof(fat12_sfn_entry), true)) {
            return false;
        }
        cache->dirty = true;        
    }
    return true;
}

bool FAT12_FS::create_file(fat12_inode* dir, const char* name, uint8 attr) {
    if ((attr & fat12_attr::VOLUME_ID) || (attr & fat12_attr::DIRECTORY)) return false;
    // 先设置对应信息
    fat12_inode* file = lookup(dir, name);
    // 已存在文件, 无需创建
    if (file) {
        release_inode(file);
        return false;
    }

    // 判断是否存在位置插入entry
    fat12_entry_location location;
    uint16 prev_cluster, new_cluster;
    int res = find_free_entry(dir, &location); 
    switch (res) {
        case -1: return false;
        case 2: return false;
        case 1: {
            // 分配一个新cluster
            new_cluster = allocate_cluster();
            if (new_cluster == 0) return false;

            prev_cluster = location.current_cluster;
            // 若case = 1, 则location.current_cluster会存放最后一个cluster地址
            fat_table[prev_cluster] = new_cluster;
            fat_table[new_cluster] = 0xFFF;
            fat_table_dirty = true;
            
            // 清空新簇
            fat12_cluster_buffer* cache = get_cache(new_cluster);
            memset(cache->buf, 0, cluster_size);
            cache->dirty = true;

            // 统一把信息写入location
            location.current_cluster = new_cluster;
            location.entry_index_in_cluster = 0;
            location.is_root = false;
            location.parent_cluster = dir ? dir->start_cluster : 0;
            location.root_index = 0;
            break;
        }
    }

    // 创建文件, 但不分配簇(初始化为0)
    fat12_normalized_entry entry;
    memset(&entry, 0, sizeof(fat12_normalized_entry));
    entry.attr = attr;
    entry.file_size = 0;
    strcpy(entry.name, name);
    entry.start_cluster = 0;

    if (!write_dir_entry(location, entry)) {
        // 回滚前面的分配
        if (res == 1) {
            // 修改新分配的表
            fat_table[new_cluster] = 0;
            // 修改原本的表
            fat_table[prev_cluster] = 0xFFF;
        }
        return false;
    }

    return true;
}

bool FAT12_FS::remove(fat12_inode* dir, const char* name) {
    fat12_inode* file = lookup(dir, name);
    if (!file) return false;                    // 不存在该文件
    else if (file->refcount != 1){
        // 还有更多地方已占用该文件
        release_inode(file);
        return false;
    }
        

    fat12_entry_location location;
    if (!find_dir_entry(file, &location)) {
        // 无效的file
        release_inode(file);
        return false;
    }

    if (!delete_dir_entry(location)) {
        // 清理失败
        release_inode(file);
        return false;
    }

    // 清理fat表
    uint16 cnt_cluster = file->start_cluster;
    while (cnt_cluster >= 2 && cnt_cluster <= 0xFF7) {
        uint16 next = fat_table[cnt_cluster];
        free_cluster(cnt_cluster);
        cnt_cluster = next;
        fat_table_dirty = true;
    }
    release_inode(file);
    return true;
}

bool FAT12_FS::create_directory(fat12_inode* dir, const char* name) {
    uint8 attr = fat12_attr::DIRECTORY;
    // 先设置对应信息
    fat12_inode* new_dir = lookup(dir, name);
    // 已存在对应文件夹, 无需创建, 直接返回
    if (new_dir) {
        release_inode(new_dir);
        return false;
    }

    // 判断是否存在位置插入entry
    fat12_entry_location location;
    uint16 new_cluster;
    uint16 prev_cluster;
    int res = find_free_entry(dir, &location); 
    switch (res) {
        case -1: return false;
        case 2: return false;
        case 1: {
            // 分配一个新cluster
            new_cluster = allocate_cluster();
            if (new_cluster == 0) return false;

            prev_cluster = location.current_cluster;
            // 若case = 1, 则location.current_cluster会存放最后一个cluster地址
            fat_table[prev_cluster] = new_cluster;
            fat_table[new_cluster] = 0xFFF;
            fat_table_dirty = true;
            
            // 清空新簇
            fat12_cluster_buffer* cache = get_cache(new_cluster);
            memset(cache->buf, 0, cluster_size);
            cache->dirty = true;

            // 统一把信息写入location
            location.current_cluster = new_cluster;
            location.entry_index_in_cluster = 0;
            location.is_root = false;
            location.parent_cluster = dir ? dir->start_cluster : 0;
            location.root_index = 0;
            break;
        }
    }


    // 给文件夹第一个cluster
    uint16 start_cluster = allocate_cluster();
    if (start_cluster == 0) {
        // 回滚前面的分配
        if (res == 1) {
            // 修改新分配的表
            fat_table[new_cluster] = 0;
            // 修改原本的表
            fat_table[prev_cluster] = 0xFFF;
        }
        return false;
    }
    fat_table[start_cluster] = 0xFFF;
    

    // 创建文件夹
    fat12_normalized_entry entry;
    memset(&entry, 0, sizeof(fat12_normalized_entry));
    entry.attr = attr;
    entry.file_size = 0;
    strcpy(entry.name, name);
    entry.start_cluster = start_cluster;

    if (!write_dir_entry(location, entry)) {
        // 回滚前面的分配
        if (res == 1) {
            // 修改新分配的表
            fat_table[new_cluster] = 0;
            // 修改原本的表
            fat_table[prev_cluster] = 0xFFF;
        }
        free_cluster(start_cluster);
        return false;
    }

    // 清空对应cluster
    fat12_cluster_buffer* dir_cache = get_cache(start_cluster);
    memset(dir_cache->buf, 0, cluster_size);
    dir_cache->dirty = true;

    return true;
}

bool FAT12_FS::remove_directory(fat12_inode* dir, const char* name) {
    fat12_inode* target = lookup(dir, name);

    if (!target) return false;                                  // invalid dir
    if (!(target->attr & fat12_attr::DIRECTORY)) return false;  // not a dir
    if (is_dir_empty(target) != 1)               return false;  // not empty

    if (target->refcount != 1){
        // 还有更多地方已占用该文件夹
        release_inode(target);
        return false;
    }
        

    fat12_entry_location location;
    if (!find_dir_entry(target, &location)) {
        // 无效的文件夹
        release_inode(target);
        return false;
    }

    if (!delete_dir_entry(location)) {
        // 清理失败
        release_inode(target);
        return false;
    }

    // 清理fat表
    uint16 cnt_cluster = target->start_cluster;
    while (cnt_cluster >= 2 && cnt_cluster <= 0xFF7) {
        uint16 next = fat_table[cnt_cluster];
        free_cluster(cnt_cluster);
        cnt_cluster = next;
        fat_table_dirty = true;
    }
    release_inode(target);
    return true;
}


void FAT12_FS::release_inode(fat12_inode* node) {
    if (node && node->fat12_fs == this) {
        node->refcount--;
        if (node->refcount <= 0) {
            inodepool.release(node);
        }
    }
}

int FAT12_FS::is_dir_empty(fat12_inode* dir) {
    if (dir && dir->fat12_fs != this) return -1;
    if (dir && !(dir->attr & fat12_attr::DIRECTORY)) return -1;

    fat12_dir_iter iter;
    if (!iter.init(this, dir)) return -1;

    return iter.next() ? 0 : 1;
}