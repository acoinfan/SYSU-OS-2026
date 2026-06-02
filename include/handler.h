#ifndef HANDLER_H
#define HANDLER_H

#include "os_type.h"
#include "enum.h"

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