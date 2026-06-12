#ifndef FILE_TYPE_H
#define FILE_TYPE_H

#include "os_type.h"
#include "enum.h"

struct File {
    void *node;
    fs_type type;
    int refcount;
    int pos;
    int flags;    
};


#endif