#include "fileSys/file_manager.h"
#include "debug.h"
#include "os_modules.h"
#include "stdlib.h"

static bool component_equals(const char* component, int len, const char* target) {
    int i = 0;
    while (i < len && target[i]) {
        if (component[i] != target[i]) {
            return false;
        }
        i++;
    }
    return i == len && target[i] == '\0';
}

static int path_length(const char* path) {
    int len = 0;
    while (path[len]) {
        len++;
        if (len >= MAX_PATH_LENGTH) {
            return -1;
        }
    }
    return len;
}

static void path_pop_component(char* out) {
    int len = strlen(out);
    if (len <= 1) {
        out[0] = '/';
        out[1] = '\0';
        return;
    }

    if (out[len - 1] == '/') {
        len--;
    }

    while (len > 1 && out[len - 1] != '/') {
        len--;
    }

    if (len == 1) {
        out[1] = '\0';
    } else {
        out[len - 1] = '\0';
    }
}

static int path_append_component(char* out, const char* component, int len) {
    if (len <= 0 || component_equals(component, len, ".")) {
        return 0;
    }

    if (component_equals(component, len, "..")) {
        path_pop_component(out);
        return 0;
    }

    int out_len = strlen(out);
    int need_slash = !(out_len == 1 && out[0] == '/');
    if (out_len + need_slash + len >= MAX_PATH_LENGTH) {
        return -1;
    }

    if (need_slash) {
        out[out_len++] = '/';
    }

    for (int i = 0; i < len; i++) {
        out[out_len++] = component[i];
    }
    out[out_len] = '\0';
    return 0;
}

static int path_consume_part(const char* part, char* out) {
    if (!part) {
        return -1;
    }

    int i = 0;
    while (part[i]) {
        while (part[i] == '/') {
            i++;
        }
        if (!part[i]) {
            break;
        }

        int start = i;
        while (part[i] && part[i] != '/') {
            i++;
        }

        int len = i - start;
        if (len >= MAX_PATH_LENGTH) {
            return -1;
        }
        if (path_append_component(out, part + start, len) != 0) {
            return -1;
        }
    }

    return 0;
}

static int path_next_component(const char* path, int* pos, char* name) {
    int i = *pos;
    while (path[i] == '/') {
        i++;
    }

    if (!path[i]) {
        *pos = i;
        name[0] = '\0';
        return 0;
    }

    int len = 0;
    while (path[i] && path[i] != '/') {
        if (len >= MAX_PATH_LENGTH - 1) {
            return -1;
        }
        name[len++] = path[i++];
    }
    name[len] = '\0';
    *pos = i;
    return 1;
}

static bool path_is_mnt_prefix(const char* path) {
    return path[0] == '/' &&
           path[1] == 'm' &&
           path[2] == 'n' &&
           path[3] == 't' &&
           (path[4] == '/' || path[4] == '\0');
}

static FAT12_FS* fat12_from_slot(char* fileSystems, int slot) {
    return (FAT12_FS*)(fileSystems + slot * PAGE_SIZE);
}

void FileManager::initialize(IdeDrive disk, fs_type fs_t) {
    // 清除信息
    memset(this, 0, sizeof(FileManager));
    
    // 初始化bitmap
    openfile_map.initialize(openfile_bitmap, MAX_OPENFILE_COUNT);

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
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();
    int ret = -1;
    FS_info* info = nullptr;
    bool res = false;

    if (total_mount_disk >= MAX_DISK_COUNT) {
        goto done;
    }

    // 清除已有信息
    info = &fs_table[total_mount_disk];
    memset(fileSystems + PAGE_SIZE * total_mount_disk, 0, PAGE_SIZE);
    memset(info, 0, sizeof(FS_info));

    // 初始化info
    info->disk = disk;
    strcpy(info->disk_name, disk_name);
    info->inuse = false;
    info->type = fs_t;

    // 挂载
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
        ret = 0;
    } else {
        ret = -1;
    }

done:
    interruptManager.setInterruptStatus(status);
    return ret;
}

int FileManager::umount(const char* disk_name) {
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();
    int ret = 1;

    // 不要umount root
    for (int i = 1; i < MAX_DISK_COUNT; i++) {
        FS_info* info = &fs_table[i];
        if (strcmp(info->disk_name, disk_name) == 0) {
            if (!info->inuse) {
                ret = -1;  // 并未使用
                goto done;
            } else {
                switch (info->type) {
                    case fs_type::FXT12: {
                        FAT12_FS* fs = (FAT12_FS*)(fileSystems + i * PAGE_SIZE);
                        if (fs->umount()) {
                            ret = 0;   // 成功释放
                        } else {
                            ret = -2;  // 释放了,但是umount失败
                        }
                        goto done;
                        break;
                    }
                    case fs_type::NONE:
                        ret = -3;      // 未识别fs属性
                        goto done;
                        break;                    
                }
            }
        }
    }
done:
    interruptManager.setInterruptStatus(status);
    return ret;   // 未找到对应disk
}

int FileManager::normalizePath(const char* cwd, const char* path, char* out) {
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();
    int ret = 0;

    if (!path || !out) {
        ret = -1;
        goto done;
    }
    if (path_length(path) < 0) {
        ret = -1;
        goto done;
    }
    if (cwd && path_length(cwd) < 0) {
        ret = -1;
        goto done;
    }

    out[0] = '/';
    out[1] = '\0';

    if (path[0] == '/') {
        ret = path_consume_part(path, out);
        goto done;
    }

    if (cwd && cwd[0]) {
        if (cwd[0] != '/') {
            ret = -1;
            goto done;
        }
        if (path_consume_part(cwd, out) != 0) {
            ret = -1;
            goto done;
        }
    }

    ret = path_consume_part(path, out);

done:
    interruptManager.setInterruptStatus(status);
    return ret;
}

int FileManager::normalizePath(const char* path, char* out) {
    const char* cwd = "/";
    if (programManager.running && programManager.running->fs_info.cwd_path[0]) {
        cwd = programManager.running->fs_info.cwd_path;
    }

    return normalizePath(cwd, path, out);
}

int FileManager::splitPath(const char* path, char* parent, char* name) {
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();
    int ret = 0;
    char normalized[MAX_PATH_LENGTH];
    int len = 0;
    int slash = 0;
    int name_len = 0;

    if (!path || !parent || !name) {
        ret = -1;
        goto done;
    }

    if (normalizePath(path, normalized) != 0) {
        ret = -1;
        goto done;
    }

    len = strlen(normalized);
    if (len <= 1) {
        ret = -1;
        goto done;
    }

    slash = len - 1;
    while (slash > 0 && normalized[slash] != '/') {
        slash--;
    }

    name_len = len - slash - 1;
    if (name_len <= 0 || name_len >= MAX_PATH_LENGTH) {
        ret = -1;
        goto done;
    }

    for (int i = 0; i < name_len; i++) {
        name[i] = normalized[slash + 1 + i];
    }
    name[name_len] = '\0';

    if (slash == 0) {
        parent[0] = '/';
        parent[1] = '\0';
        goto done;
    }

    for (int i = 0; i < slash; i++) {
        parent[i] = normalized[i];
    }
    parent[slash] = '\0';

done:
    interruptManager.setInterruptStatus(status);
    return ret;
}

int FileManager::allocate_openfile() {
    return openfile_map.allocate(1);
}
void FileManager::release_openfile(int idx) {
    memset(&openfile_table[idx], 0, sizeof(OpenFile));
    openfile_map.release(idx, 1);
}

OpenFile* FileManager::get_openfile(int idx) {
    return &openfile_table[idx];
}

OpenFile* FileManager::lookup(const char* path) {
    char normalized[MAX_PATH_LENGTH];
    char component[MAX_PATH_LENGTH];
    int fs_idx = 0;
    int pos = 1;
    void* node = nullptr;
    int openfile_idx = -1;
    OpenFile* file = nullptr;
    bool reused_openfile = false;

    if (normalizePath(path, normalized) != 0) {
        return nullptr;
    }

    if (path_is_mnt_prefix(normalized)) {
        pos = 5;
        int component_status = path_next_component(normalized, &pos, component);
        if (component_status <= 0) {
            return nullptr;
        }

        fs_idx = -1;
        for (int i = 1; i < MAX_DISK_COUNT; i++) {
            if (fs_table[i].inuse && strcmp(fs_table[i].disk_name, component) == 0) {
                fs_idx = i;
                break;
            }
        }

        if (fs_idx == -1) {
            return nullptr;
        }

        while (normalized[pos] == '/') {
            pos++;
        }
    }

    switch (fs_table[fs_idx].type) {
        case fs_type::FXT12: {
            FAT12_FS* fs = fat12_from_slot(fileSystems, fs_idx);
            fat12_inode* current = nullptr;

            while (true) {
                int component_status = path_next_component(normalized, &pos, component);
                if (component_status < 0) {
                    if (current) {
                        fs->release_inode(current);
                    }
                    return nullptr;
                }
                if (component_status == 0) {
                    node = current;
                    break;
                }

                fat12_inode* next = fs->lookup(current, component);
                if (current) {
                    fs->release_inode(current);
                }
                if (!next) {
                    return nullptr;
                }
                current = next;
            }
            break;
        }
        default:
            return nullptr;
    }

    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    for (int i = 0; i < MAX_OPENFILE_COUNT; i++) {
        if (openfile_map.get(i) &&
            openfile_table[i].type == fs_table[fs_idx].type &&
            openfile_table[i].node == node) {
            file = &openfile_table[i];
            reused_openfile = true;
            break;
        }
    }

    if (!file) {
        openfile_idx = allocate_openfile();
    }

    if (openfile_idx != -1) {
        file = &openfile_table[openfile_idx];
        file->node = node;
        file->type = fs_table[fs_idx].type;
        file->refcount = 0;
        file->attr = 0;
    }
    interruptManager.setInterruptStatus(status);

    if (!file) {
        switch (fs_table[fs_idx].type) {
            case fs_type::FXT12:
                if (node) {
                    fat12_from_slot(fileSystems, fs_idx)->release_inode((fat12_inode*)node);
                }
                break;
            default:
                break;
        }
        return nullptr;
    }

    if (reused_openfile) {
        switch (fs_table[fs_idx].type) {
            case fs_type::FXT12:
                if (node) {
                    fat12_from_slot(fileSystems, fs_idx)->release_inode((fat12_inode*)node);
                }
                break;
            default:
                break;
        }
    }

    return file;
}

int FileManager::open(const char* path, int flags) {
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();
    int fd = -1;
    OpenFile* openfile = nullptr;

    PCB* process = programManager.running;
    if (!process) {
        goto done;
    }

    openfile = lookup(path);
    if (!openfile) {
        goto done;
    }

    for (int i = 0; i < MAX_FD_COUNT; i++) {
        if (!process->fd_table[i].openfile) {
            fd = i;
            process->fd_table[i].openfile = openfile;
            process->fd_table[i].offset = 0;
            openfile->refcount++;
            break;
        }
    }

    if (fd == -1 && openfile->refcount == 0) {
        int openfile_idx = ((uint32)openfile - (uint32)openfile_table) / sizeof(OpenFile);
        void* node = openfile->node;
        fs_type type = openfile->type;
        switch (type) {
            case fs_type::FXT12:
                if (node) {
                    FAT12_FS* fs = (FAT12_FS*)((fat12_inode*)node)->fat12_fs;
                    fs->release_inode((fat12_inode*)node);
                }
                break;
            default:
                break;
        }
        release_openfile(openfile_idx);
    }

done:
    interruptManager.setInterruptStatus(status);
    return fd;
}

int FileManager::close(int fd) {
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();
    int ret = -1;
    File* file = nullptr;
    OpenFile* openfile = nullptr;

    PCB* process = programManager.running;
    if (!process || fd < 0 || fd >= MAX_FD_COUNT) {
        goto done;
    }

    file = &process->fd_table[fd];
    openfile = file->openfile;
    if (!openfile) {
        goto done;
    }

    file->openfile = nullptr;
    file->offset = 0;
    openfile->refcount--;

    if (openfile->refcount <= 0) {
        int openfile_idx = ((uint32)openfile - (uint32)openfile_table) / sizeof(OpenFile);
        void* node = openfile->node;
        fs_type type = openfile->type;

        switch (type) {
            case fs_type::FXT12:
                if (node) {
                    FAT12_FS* fs = (FAT12_FS*)((fat12_inode*)node)->fat12_fs;
                    fs->release_inode((fat12_inode*)node);
                }
                break;
            default:
                break;
        }

        release_openfile(openfile_idx);
    }

    ret = 0;

done:
    interruptManager.setInterruptStatus(status);
    return ret;
}

int FileManager::read(int fd, void* buf, int size) {
    PCB* process = programManager.running;
    if (!process || fd < 0 || fd >= MAX_FD_COUNT || !buf || size <= 0) {
        return -1;
    }

    File* file = &process->fd_table[fd];
    OpenFile* openfile = file->openfile;
    if (!openfile) {
        return -1;
    }

    int ret = -1;
    switch (openfile->type) {
        case fs_type::FXT12: {
            FAT12_FS* fs = (FAT12_FS*)((fat12_inode*)openfile->node)->fat12_fs;
            ret = fs->read((fat12_inode*)openfile->node, buf, size, file->offset);
            break;
        }
        default:
            return -1;
    }

    if (ret > 0) {
        file->offset += ret;
    }
    return ret;
}

int FileManager::write(int fd, void* buf, int size) {
    PCB* process = programManager.running;
    if (!process || fd < 0 || fd >= MAX_FD_COUNT || !buf || size <= 0) {
        return -1;
    }

    File* file = &process->fd_table[fd];
    OpenFile* openfile = file->openfile;
    if (!openfile) {
        return -1;
    }

    int ret = -1;
    switch (openfile->type) {
        case fs_type::FXT12: {
            FAT12_FS* fs = (FAT12_FS*)((fat12_inode*)openfile->node)->fat12_fs;
            ret = fs->write((fat12_inode*)openfile->node, buf, size, file->offset);
            break;
        }
        default:
            return -1;
    }

    if (ret > 0) {
        file->offset += ret;
    }
    return ret;
}
