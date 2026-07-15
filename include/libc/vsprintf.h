#ifndef VSPRINTF_H
#define VSPRINTF_H

#define DEFAULT_WIDTH 1
#define MAX_NUM_LENGTH 128

#include "os_type.h"
#include "stdarg.h"

#if defined(__i386__) || defined(_M_IX86)
#define PLATFORM_32BIT
#elif defined(__x86_64__) || defined(_M_X64)
#define PLATFORM_64BIT
#endif

typedef void (*putc_callback_t)(char c);

struct Format {
    unsigned width;
    bool left_align;
    bool zero_pad;
    bool valid;
    enum {
        LEN_INT = 0,
        LEN_LONG,
        LEN_LLONG
    } length;
    char specifier;
};

void analyse_format(const char *const fmt, int* idx, Format* format);

#ifdef PLATFORM_64BIT
char* sign_decify(long long number, char* buf);
char* unsign_decify(unsigned long long number, char* buf);
char* hexify(unsigned long long number, char* buf);
#else
char* sign_decify(long number, char* buf);
char* unsign_decify(unsigned long number, char* buf);
char* hexify(unsigned long number, char* buf);
#endif

int varg_to_callback(putc_callback_t out, const Format* format, va_list* ap);
int vsprintf_callback(putc_callback_t out, const char* fmt, va_list ap);

#endif
