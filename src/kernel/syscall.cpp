#include "syscall.h"
#include "interrupt.h"
#include "stdlib.h"
#include "asm_utils.h"
#include "os_modules.h"
#include "stdio.h"

int system_call_table[MAX_SYSTEM_CALL];