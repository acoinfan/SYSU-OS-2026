#ifndef VFS_H
#define VFS_H

#include "os_type.h"

#define VFS_NAME_MAX 256

enum VfsEntryType : uint8 {
    VFS_ENTRY_UNKNOWN = 0,
    VFS_ENTRY_FILE = 1,
    VFS_ENTRY_DIR = 2
};

struct LsEntry {
    char name[VFS_NAME_MAX];
    uint8 type;
    uint8 attr;
    uint16 start_cluster;
    uint32 size;
};

#endif
