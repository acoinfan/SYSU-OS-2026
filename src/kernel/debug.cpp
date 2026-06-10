#include "debug.h"
#include "screen.h"
#include "asm_utils.h"

void assert_fail(
    const char *expr,
    const char *file,
    int line,
    const char *func
)
{
    kprintf("\n");
    kprintf("================================\n");
    kprintf("ASSERT FAILED\n");
    kprintf("expr : %s\n",expr);
    kprintf("file : %s\n",file);
    kprintf("line : %d\n",line);
    kprintf("func : %s\n",func);
    kprintf("================================\n");

    asm_halt();

    while(1);
}