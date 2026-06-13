#include "debug.h"
#include "screen.h"
#include "asm_utils.h"
#include "stdarg.h"
#include "vsprintf.h"
#include "stdlib.h"

static char debug_log_buffer[64 * 1024];
static uint32 debug_log_length = 0;

void debug_log_clear() {
    memset(debug_log_buffer, 0, sizeof(debug_log_buffer));
    debug_log_length = 0;
}

void debug_log_append(char c) {
    if (debug_log_length + 1 >= sizeof(debug_log_buffer)) return;
    debug_log_buffer[debug_log_length++] = c;
    debug_log_buffer[debug_log_length] = '\0';
}

const char* debug_log_data() {
    return debug_log_buffer;
}

uint32 debug_log_size() {
    return debug_log_length;
}

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

static void test_log_putc(char c) {
    kputc(c);
    debug_log_append(c);
}

int test_log_printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int len = vsprintf_callback(test_log_putc, fmt, ap);
    va_end(ap);

    return len;
}

void debug_log_write(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsprintf_callback(kputc, fmt, ap);
    va_end(ap);
}
