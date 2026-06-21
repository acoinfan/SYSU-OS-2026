#include "fileSys/file_manager.h"
#include "debug.h"

void FileManager::initialize(IdeDrive disk, fs_type fs_t) {
    // 清除信息
    memset(this, 0, sizeof(FileManager));

    // 挂载root盘
    fs_table[0].disk = disk;
    fs_table[0].disk_name[0] = '/';
    fs_table[0].inuse = true;
    fs_table[0].type = fs_t;
    total_mount_disk = 1;

    // mount
    switch (fs_table[0].type) {
        case fs_type::FXT12: {
            FAT12_FS* root_fs = (FAT12_FS*)fileSystems;
            root_fs->mount(disk);
            break;
        }
        case fs_type::NONE:
            PANIC("Invalid initialize of fileSystem\n");
    }

    return;
}

// TODO: 不允许相同disk_name做挂载
int FileManager::mount(const char* disk_name, IdeDrive disk, fs_type fs_t) {
    // 清除已有信息
    FS_info* info = &fs_table[total_mount_disk];
    memset(fileSystems + PAGE_SIZE * total_mount_disk, 0, PAGE_SIZE);
    memset(info, 0, sizeof(FS_info));

    // 初始化info
    info->disk = disk;
    strcpy(info->disk_name, disk_name);
    info->inuse = false;
    info->type = fs_t;

    // 挂载
    bool res = false;
    switch (fs_table[total_mount_disk].type) {
        case fs_type::FXT12: {
            FAT12_FS* fs = (FAT12_FS*)(fileSystems + total_mount_disk * PAGE_SIZE);
            res = fs->mount(info->disk);
            break;
        }
        case fs_type::NONE:
            break;
    }

    // 设置信息
    if (res) {
        info->inuse = true;
        total_mount_disk++;
        return 0;
    } else {
        return -1;
    }
}

int FileManager::umount(const char* disk_name) {
    // 不要umount root
    for (int i = 1; i < MAX_DISK_COUNT; i++) {
        FS_info* info = &fs_table[i];
        if (strcmp(info->disk_name, disk_name) == 0) {
            if (!info->inuse) {
                return -1;  // 并未使用
            } else {
                switch (info->type) {
                    case fs_type::FXT12: {
                        FAT12_FS* fs = (FAT12_FS*)(fileSystems + i * PAGE_SIZE);
                        if (fs->umount()) {
                            return 0;   // 成功释放
                        } else {
                            return -2;  // 释放了,但是umount失败
                        }
                        break;
                    }
                    case fs_type::NONE:
                        return -3;      // 未识别fs属性
                        break;                    
                }
            }
        }
    }
    return 1;   // 未找到对应disk
}