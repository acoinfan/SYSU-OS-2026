#ifndef OS_MODULES_H
#define OS_MODULES_H

#include "interrupt.h"
#include "screen.h"
#include "program.h"
#include "memory.h"
#include "tss.h"
#include "fileSys/file_manager.h"

extern InterruptManager interruptManager;
extern SCREEN screen;
extern ProgramManager programManager;
extern MemoryManager memoryManager;
extern TSS tss;
extern FileManager fileManager;

#endif
