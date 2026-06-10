#include "stdio.h"
#include "vsprintf.h"
#include "stdarg.h"
#include "syscall.h"

int printf(const char* const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsprintf_callback(printf_buf_puts, fmt, ap);
    va_end(ap);
    return ret;
}

int putchar(char ch) {
    char str[2] = {};
    str[0] = ch;
    int res = write(str);
    if (res) return 1;
    else return 0;
}

int puts(const char* str) {
    return write(str);
}

void printf_buf_puts(char c) {
    static char buf[65];
    static int idx = 0;
    buf[idx++] = c;
    if (idx >= 64 || c == '\n') {
        buf[idx] = '\0';
        write(buf);
        idx = 0;
    } 
}