#include "debug.h"
#include "screen.h"
#include "asm_utils.h"
#include "stdarg.h"
#include "vsprintf.h"
#include "stdlib.h"
#include "os_modules.h"
#include "os_constant.h"

#define DEBUG_LOG_SIZE (64 * 1024)

static char* debug_log_buffer = nullptr;
static uint32 debug_log_length = 0;

void debug_log_init() {
    if (debug_log_buffer) {
        return;
    }

    debug_log_buffer = (char*)memoryManager.allocatePagesLazy(
        AddressPoolType::KERNEL, DEBUG_LOG_SIZE / PAGE_SIZE, VP_RW);
    if (debug_log_buffer) {
        memset(debug_log_buffer, 0, DEBUG_LOG_SIZE);
    }
    debug_log_length = 0;
}

void debug_log_clear() {
    if (!debug_log_buffer) {
        debug_log_init();
    }
    if (!debug_log_buffer) {
        debug_log_length = 0;
        return;
    }
    memset(debug_log_buffer, 0, DEBUG_LOG_SIZE);
    debug_log_length = 0;
}

void debug_log_append(char c) {
    if (!debug_log_buffer) return;
    if (debug_log_length + 1 >= DEBUG_LOG_SIZE) return;
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
    debug_log_append(c);
    kputc(c);
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
