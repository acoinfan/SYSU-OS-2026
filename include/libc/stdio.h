#ifndef STDIO_H
#define STDIO_H
int printf(const char* const fmt, ...);

int putchar(char ch);
int puts(const char* str);

void printf_buf_puts(char c);
#endif