#include "vsprintf.h"
#include "stdlib.h"

static int out_str(putc_callback_t out, const char* str) {
    int counter = 0;
    for (int i = 0; str[i]; ++i) {
        out(str[i]);
        counter++;
    }
    return counter;
}

static int out_pad(putc_callback_t out, char pad, int num) {
    for (int i = 0; i < num; ++i) {
        out(pad);
    }
    return num;
}

Format analyse_format(const char *const fmt, int* idx) {
    ++(*idx);
    Format format;
    while (format.valid) {
        switch (fmt[*idx]) {
            case 'd':
            case 'i':
            case 'u':
            case 'x':
            case 'c':
            case 's':
            case 'p':
                format.specifier = fmt[*idx];
                return format;
            case '0':
                if (!format.zero_pad)
                    format.zero_pad = true;
                else
                    format.valid = false;
                break;
            case '-':
                if (!format.left_align)
                    format.left_align = true;
                else
                    format.valid = false;
                break;
            case 'l':
                if (format.length == format.LEN_INT)
                    format.length = format.LEN_LONG;
                #ifdef PLATFORM_64BIT
                    else if (format.length == format.LEN_LONG)
                        format.length = format.LEN_LLONG;
                #endif
                else
                    format.valid = false;
                break;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': {
                if (format.width != DEFAULT_WIDTH) {
                    format.valid = false;
                    break;
                }
                int sum = 0;
                while (fmt[*idx] >= '0' && fmt[*idx] <= '9') {
                    sum *= 10;
                    sum += (fmt[*idx] - '0');
                    (*idx)++;
                }
                (*idx)--;
                format.width = sum;
                break;
            }
            default:
                format.valid = 0;
                return format;
        }
        (*idx)++;
    }
    return format;
}

int varg_to_callback(putc_callback_t out, Format format, va_list* ap) {
    int counter = 0;
    switch (format.specifier) {
        case 'p': {
            void* ptr = va_arg(*ap, void*);
#ifdef PLATFORM_64BIT
            char* str = hexify((unsigned long long)ptr);
#else
            char* str = hexify((unsigned long)ptr);
#endif
            counter += out_str(out, "0x");
            counter += out_str(out, str);
            break;
        }
        case 's': {
            char* str = va_arg(*ap, char*);
            counter += out_str(out, str);
            break;
        }
        case 'c': {
            char c = va_arg(*ap, int);
            out(c);
            counter++;
            break;
        }
        case 'd':
        case 'i': {
#ifdef PLATFORM_64BIT
            long long num;
#else
            long num;
#endif
            switch (format.length) {
                case format.LEN_INT:
                    num = va_arg(*ap, int);
                    break;
                case format.LEN_LONG:
                    num = va_arg(*ap, long);
                    break;
#ifdef PLATFORM_64BIT
                case format.LEN_LLONG:
                    num = va_arg(*ap, long long);
                    break;
#endif
                default:
                    break;
            }
            char* str = sign_decify(num);
            unsigned length = strlen(str);
            if (length < format.width) {
                if (format.left_align) {
                    counter += out_str(out, str);
                    counter += out_pad(out, ' ', format.width - length);
                } else {
                    int negative = str[0] == '-';
                    if (format.zero_pad && negative) {
                        out('-');
                        counter += out_pad(out, '0', format.width - length);
                        counter += out_str(out, str + 1);
                    } else if (format.zero_pad && !negative) {
                        counter += out_pad(out, '0', format.width - length);
                        counter += out_str(out, str);
                    } else if (!format.zero_pad && negative) {
                        counter += out_pad(out, ' ', format.width - length);
                        out('-');
                        counter += out_str(out, str + 1);
                    } else {
                        counter += out_pad(out, ' ', format.width - length);
                        counter += out_str(out, str);
                    }
                }
            } else {
                counter += out_str(out, str);
            }
            break;
        }
        case 'u': {
#ifdef PLATFORM_64BIT
            unsigned long long num;
#else
            unsigned long num;
#endif
            switch (format.length) {
                case format.LEN_INT:
                    num = va_arg(*ap, unsigned);
                    break;
                case format.LEN_LONG:
                    num = va_arg(*ap, unsigned long);
                    break;
#ifdef PLATFORM_64BIT
                case format.LEN_LLONG:
                    num = va_arg(*ap, unsigned long long);
                    break;
#endif
                default:
                    break;
            }
            char* str = unsign_decify(num);
            unsigned length = strlen(str);
            if (length < format.width) {
                if (format.left_align) {
                    counter += out_str(out, str);
                    counter += out_pad(out, ' ', format.width - length);
                } else if (format.zero_pad) {
                    counter += out_pad(out, '0', format.width - length);
                    counter += out_str(out, str);
                } else {
                    counter += out_pad(out, ' ', format.width - length);
                    counter += out_str(out, str);
                }
            } else {
                counter += out_str(out, str);
            }
            break;
        }
        case 'x': {
#ifdef PLATFORM_64BIT
            unsigned long long num;
#else
            unsigned long num;
#endif
            switch (format.length) {
                case format.LEN_INT:
                    num = va_arg(*ap, unsigned);
                    break;
                case format.LEN_LONG:
                    num = va_arg(*ap, unsigned long);
                    break;
#ifdef PLATFORM_64BIT
                case format.LEN_LLONG:
                    num = va_arg(*ap, unsigned long long);
                    break;
#endif
                default:
                    break;
            }
            char* str = hexify(num);
            unsigned length = strlen(str);
            if (length < format.width) {
                if (format.left_align) {
                    counter += out_str(out, str);
                    counter += out_pad(out, ' ', format.width - length);
                } else if (format.zero_pad) {
                    counter += out_pad(out, '0', format.width - length);
                    counter += out_str(out, str);
                } else {
                    counter += out_pad(out, ' ', format.width - length);
                    counter += out_str(out, str);
                }
            } else {
                counter += out_str(out, str);
            }
            break;
        }
    }
    return counter;
}

#ifdef PLATFORM_64BIT
char* sign_decify(long long number) {
    static char buf[MAX_NUM_LENGTH + 1];
    int idx = MAX_NUM_LENGTH;
    buf[idx] = '\0';
    bool negative = false;
    if (number < 0) {
        negative = true;
        number = -number;
    }
    if (number == 0) {
        buf[--idx] = '0';
    }
    while (number > 0) {
        buf[--idx] = '0' + (number % 10);
        number /= 10;
    }
    if (negative) {
        buf[--idx] = '-';
    }
    return &buf[idx];
}

char* unsign_decify(unsigned long long number) {
    static char buf[MAX_NUM_LENGTH + 1];
    int idx = MAX_NUM_LENGTH;
    buf[idx] = '\0';
    if (number == 0) {
        buf[--idx] = '0';
    }
    while (number > 0) {
        buf[--idx] = '0' + (number % 10);
        number /= 10;
    }
    return &buf[idx];
}

char* hexify(unsigned long long number) {
    static char buf[MAX_NUM_LENGTH + 1];
    static const char table[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    int idx = MAX_NUM_LENGTH;
    buf[idx] = '\0';
    if (number == 0) {
        buf[--idx] = '0';
    }
    while (number > 0) {
        buf[--idx] = table[number % 16];
        number /= 16;
    }
    return &buf[idx];
}
#else
char* sign_decify(long number) {
    static char buf[MAX_NUM_LENGTH + 1];
    int idx = MAX_NUM_LENGTH;
    buf[idx] = '\0';
    bool negative = false;
    if (number < 0) {
        negative = true;
        number = -number;
    }
    if (number == 0) {
        buf[--idx] = '0';
    }
    while (number > 0) {
        buf[--idx] = '0' + (number % 10);
        number /= 10;
    }
    if (negative) {
        buf[--idx] = '-';
    }
    return &buf[idx];
}

char* unsign_decify(unsigned long number) {
    static char buf[MAX_NUM_LENGTH + 1];
    int idx = MAX_NUM_LENGTH;
    buf[idx] = '\0';
    if (number == 0) {
        buf[--idx] = '0';
    }
    while (number > 0) {
        buf[--idx] = '0' + (number % 10);
        number /= 10;
    }
    return &buf[idx];
}

char* hexify(unsigned long number) {
    static char buf[MAX_NUM_LENGTH + 1];
    static const char table[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    int idx = MAX_NUM_LENGTH;
    buf[idx] = '\0';
    if (number == 0) {
        buf[--idx] = '0';
    }
    while (number > 0) {
        buf[--idx] = table[number % 16];
        number /= 16;
    }
    return &buf[idx];
}
#endif

int vsprintf_callback(putc_callback_t out, const char* fmt, va_list ap) {
    int counter = 0;
    for (int i = 0; fmt[i]; ++i) {
        if (fmt[i] == '%' && fmt[i + 1] == '%') {
            out('%');
            counter++;
            i++;
        } else if (fmt[i] == '%' && fmt[i + 1] != '%') {
            Format format = analyse_format(fmt, &i);
            if (!format.valid) {
                return -1;
            }
            counter += varg_to_callback(out, format, &ap);
        } else {
            out(fmt[i]);
            counter++;
        }
    }
    return counter;
}
