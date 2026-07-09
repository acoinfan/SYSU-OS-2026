#include "fileSys/fat12_entry.h"
#include "stdlib.h"
#include "screen.h"
#include "fileSys/fat12_fs.h"
#include "debug.h"

bool fat12_entry_buf::read(void* fat12_entry) {
    uint8 attr = *(uint8*) (fat12_entry + 11);
    
    // LFN
    if (attr == 0x0F) {
        status = SKIP_LFN;
        return false;

    // SFN
    } else {
        fat12_sfn_entry* entry = (fat12_sfn_entry*)fat12_entry;
        // 末尾条目
        if (entry->name[0] == 0x0) {
            status = END_OF_DIR;
            return false;
        }
        // 已删除条目
        if ((uint8)entry->name[0] == 0xE5) {
            status = DELETE;
            return false;
        }
        // 前面是LFN条目, 不接受这个SFN
        if (status == SKIP_LFN) {
            status = SKIP_SFN;
            return false;
        }
        // 读取信息
        this->normalized_entry.start_cluster = entry->first_cluster_low;
        this->normalized_entry.attr = entry->attr;
        this->normalized_entry.create_time_tenths = entry->create_time_tenths;
        this->normalized_entry.create_time = entry->create_time;
        this->normalized_entry.create_date = entry->create_date;
        this->normalized_entry.last_access_date = entry->last_access_date;
        this->normalized_entry.write_time = entry->write_time;
        this->normalized_entry.write_date = entry->write_date;
        this->normalized_entry.file_size = entry->file_size;

        // 处理名称
        tail = 0; 
        // 处理前导名
        for (int i = 0; i < 8 && entry->name[i] != ' '; i++) {
            normalized_entry.name[tail++] = to_lower(entry->name[i]);
        }

        // 处理后缀名
        if (!(entry->attr & fat12_attr::DIRECTORY)) {
            // 无后缀名文件:
            if (entry->ext[0] != ' ') {
                normalized_entry.name[tail++] = '.';
                for (int i = 0; i < 3 && entry->ext[i] != ' '; i++) {
                    normalized_entry.name[tail++] = to_lower(entry->ext[i]);
                }
            }
        }

        normalized_entry.name[tail] = '\0';
        tail = 0; // 下次解析可用
        status = VALID;
        return true;
    }
}

bool fat12_entry_buf::write(void* fat12_entry, bool setDelete) {
    fat12_sfn_entry* entry = (fat12_sfn_entry*)fat12_entry;
    if (setDelete) {
        entry->name[0] = (uint8)0xE5;
        return true;
    }

    // 设置padding
    memset(entry, 0, sizeof(fat12_sfn_entry));
    memset(entry->name, ' ', 8);
    memset(entry->ext, ' ', 3);

    // 设置名字
    if (this->normalized_entry.attr & fat12_attr::DIRECTORY) {
        for (int i = 0; i < 8 && this->normalized_entry.name[i]; i++) {
            entry->name[i] = to_upper(this->normalized_entry.name[i]);
        }   
    } else {
        // 找到末尾的.
        int dot_idx = -1;
        for (int i = 0; i < FAT12_MAX_FILENAME && this->normalized_entry.name[i]; i++) {
            if (this->normalized_entry.name[i] == '.') dot_idx = i;
        }

        for (int i = 0; i < 8 && i < dot_idx; i++) {
            entry->name[i] = to_upper(this->normalized_entry.name[i]);
        }
        // 没有找到. 视为无后缀名文件
        if (dot_idx != -1) {
            for (int i = 0; i < 3 && this->normalized_entry.name[i + dot_idx + 1]; i++) {
                entry->ext[i] = to_upper(this->normalized_entry.name[i + dot_idx + 1]);
            }
        }
    }
    
    // 设置其它参数
    entry->first_cluster_low = this->normalized_entry.start_cluster;
    entry->attr = this->normalized_entry.attr;
    entry->create_time_tenths = this->normalized_entry.create_time_tenths;
    entry->create_time = this->normalized_entry.create_time;
    entry->create_date = this->normalized_entry.create_date;
    entry->last_access_date = this->normalized_entry.last_access_date;
    entry->write_time = this->normalized_entry.write_time;
    entry->write_date = this->normalized_entry.write_date;
    entry->file_size = this->normalized_entry.file_size;
    status = WRITE;
    return true;
}

void fat12_entry_buf::reset() {
    memset(this, 0, sizeof(fat12_entry_buf));
    status = FREE;
    tail = 0;
    checksum = 0;
}

void fat12_inode::dump() {
    // test_log_printf("size = %d\n"
    //     "start_cluster = %d\n"
    //     "parent_dir_start_cluster = %d\n"
    //     "attr = %x\n"
    //     "refCount = %d\n",
    //     size, start_cluster, parent_dir_start_cluster, attr, refcount);
}

bool fat12_dir_iter::init(FAT12_FS* fs, fat12_inode* dir) {
    if (!fs) return false;

    if (dir && dir->fat12_fs != fs) return false;
    if (dir && (!(dir->attr & fat12_attr::DIRECTORY))) return false;
    memset(this, 0, sizeof(fat12_dir_iter));

    this->fs = fs;
    this->dir = dir;
    this->current.reset();

    if (dir) {
        this->current_cluster = dir->start_cluster;
    }
    return true;
}

bool fat12_dir_iter::next() {
    // 根目录
    if (!dir) {
        while (root_index < fs->root_entries) {
            const fat12_normalized_entry& entry = fs->root_dir[root_index++];
            global_idx++;

            if (entry.name[0] == '\0') {
                return false;   // 根目录结束
            }
            if (entry.attr & fat12_attr::VOLUME_ID) {
                continue;       // 跳过卷标
            }

            current.reset();
            current.normalized_entry = entry;
            current.status = fat12_entry_buf::VALID;
            count++;
            return true;
        }
        return false;
    }

    // 子目录
    while (current_cluster >= 2 && current_cluster < 0xFF8) {
        fat12_cluster_buffer* cache = fs->get_cache(current_cluster);
        if (!cache) return false;

        uint16 total_entries = fs->cluster_size / FAT12_ENTRY_BYTES;

        while (entry_index_in_cluster < total_entries) {
            uint32 raw_index = entry_index_in_cluster++;
            global_idx++;

            // 注意：current 不能每次都 reset，
            // 因为 fat12_entry_buf::read 需要靠 status 跳过 LFN 序列
            if (current.read(cache->buf + raw_index * FAT12_ENTRY_BYTES)) {
                if (current.status != fat12_entry_buf::VALID) {
                    continue;
                }

                if (current.normalized_entry.attr & fat12_attr::VOLUME_ID) {
                    continue;
                }

                count++;
                return true;
            } else {
                if (current.status == fat12_entry_buf::END_OF_DIR) {
                    return false;
                }
                // DELETE / SKIP_LFN / SKIP_SFN 都继续扫
            }
        }

        current_cluster = fs->fat_table[current_cluster];
        entry_index_in_cluster = 0;
    }

    return false;
}

const fat12_normalized_entry& fat12_dir_iter::get() const {
    return this->current.normalized_entry;
}

void fat12_normalized_entry::dump() const {
    kprintf("name = %s\n", name);
    kprintf("size = %u\n", file_size);
    kprintf("start_cluster = %u\n", start_cluster);
    kprintf("attr = 0x%x\n", attr);
    kprintf("create_time = %04x, create_date = %04x\n", create_time, create_date);
    kprintf("write_time = %04x, write_date = %04x\n", write_time, write_date);
}

void fat12_normalized_entry::copy(const fat12_normalized_entry& other) {
    this->attr = other.attr;
    this->create_date = other.create_date;
    this->create_time = other.create_time;
    this->create_time_tenths = other.create_time_tenths;
    this->file_size = other.file_size;
    this->last_access_date = other.last_access_date;
    strcpy(this->name, other.name);
    this->start_cluster = other.start_cluster;
    this->write_date = other.write_date;
    this->write_time = other.write_time;
}

bool fat12_entry_location::is_same(const fat12_entry_location& other) const {
    if (this->is_root) {
        return other.is_root && this->root_index == other.root_index;
    } else {
        return (!other.is_root) && (this->current_cluster == other.current_cluster)
                && (this->entry_index_in_cluster == other.entry_index_in_cluster);
    }
}