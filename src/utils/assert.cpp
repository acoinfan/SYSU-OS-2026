#include "assert.h"
#include "stdio.h"
#include "asm_utils.h"

void assert_fail(
    const char *expr,
    const char *file,
    int line,
    const char *func
)
{
    printf("\n");
    printf("================================\n");
    printf("ASSERT FAILED\n");
    printf("expr : %s\n",expr);
    printf("file : %s\n",file);
    printf("line : %d\n",line);
    printf("func : %s\n",func);
    printf("================================\n");

    asm_halt();

    while(1);
}