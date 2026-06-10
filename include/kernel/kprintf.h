#ifndef __PRINTF_H__
#define __PRINTF_H__

#define BUF_SIZE 32
#define MAX_NUM_LENGTH 128
#define DEFAULT_WIDTH 1
#define false 0
#define true 1

#include "screen.h"
#include "os_type.h"
#include "asm_utils.h"
#include "os_modules.h"
#include "stdarg.h"
#include "stdlib.h"

#if defined(__i386__) || defined(_M_IX86)
#define PLATFORM_32BIT
#elif defined(__x86_64__) || defined(_M_X64)
#define PLATFORM_64BIT
#endif

struct Format {
    unsigned width = DEFAULT_WIDTH;
    int left_align = false;
    int zero_pad = false;
    int valid = true;
    enum {
        LEN_INT = 0,
        LEN_LONG,
        LEN_LLONG
    } length = LEN_INT;
    char specifier; // d,i,u,x
};

struct Buffer {
    char str[BUF_SIZE + 1];
    int idx = 0;
};


inline int put_string(const char* const string);
int flush_buffer(Buffer* buf);
void init_buffer(Buffer* buf);
int add_char_to_buffer(Buffer* buf, char c);
int add_str_to_buffer(Buffer* buf, const char* const str);
inline int add_pad_to_buffer(Buffer* buf, char pad, int num);
Format analyse_format(const char *const fmt, int* idx);
int varg_to_buffer(Buffer* buf, Format format, va_list* ap);

#ifdef PLATFORM_64BIT
char* sign_decify(long long number);
char* unsign_decify(unsigned long long number);
char* hexify(unsigned long long number);
#else 
char* sign_decify(long number);
char* unsign_decify(unsigned long number);
char* hexify(unsigned long number);
#endif

int printf(const char *const fmt, ...);
void test();

#endif