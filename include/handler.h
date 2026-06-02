#ifndef HANDLER_H
#define HANDLER_H

#include "os_type.h"

enum struct FaultType : uint8 {
    DEMAND_ZERO = 0,
    STACK_GROWTH,
    HEAP_GROWTH,
    SWAP_IN,
    COPY_ON_WRITE,
    FILE_BACKED,
    PERMISSION_VIOLATION,
    KERNEL_RESERVED,
    INVALID_ADDRESS,
    OUT_OF_MEMORY,
    PAGE_TABLE_BROKEN,
    UNKNOWN
};

struct PageFaultInfo {
    FaultType faultType;
    uint32 addr;
    uint32* PDEptr;
    uint32* PTEptr;
};

void handle_kernel_page_fault(const PageFaultInfo& info);
bool handle_user_page_fault(const PageFaultInfo& info);
int out_of_memory(enum AddressPoolType type, const int count);

#endif