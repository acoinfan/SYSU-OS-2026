#include "fileSys/fat12_entry.h"
#include "stdlib.h"
#include "screen.h"

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
        if (entry->name[0] == 0xE5) {
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
        entry->name[0] = 0xE5;
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
    kprintf("size = %d\n"
        "start_cluster = %d\n"
        "parent_dir_start_cluster = %d\n"
        "attr = %x\n"
        "refCount = %d\n",
        size, start_cluster, parent_dir_start_cluster, attr, refcount);
}