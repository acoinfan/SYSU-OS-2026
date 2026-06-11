#ifndef VSPRINTF_H
#define VSPRINTF_H

#define DEFAULT_WIDTH 1
#define MAX_NUM_LENGTH 128
#define false 0
#define true 1

#include "os_type.h"
#include "stdarg.h"

#if defined(__i386__) || defined(_M_IX86)
#define PLATFORM_32BIT
#elif defined(__x86_64__) || defined(_M_X64)
#define PLATFORM_64BIT
#endif

typedef void (*putc_callback_t)(char c);

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
    char specifier;
};

Format analyse_format(const char *const fmt, int* idx);

#ifdef PLATFORM_64BIT
char* sign_decify(long long number);
char* unsign_decify(unsigned long long number);
char* hexify(unsigned long long number);
#else
char* sign_decify(long number);
char* unsign_decify(unsigned long number);
char* hexify(unsigned long number);
#endif

int varg_to_callback(putc_callback_t out, Format format, va_list* ap);
int vsprintf_callback(putc_callback_t out, const char* fmt, va_list ap);

#endif