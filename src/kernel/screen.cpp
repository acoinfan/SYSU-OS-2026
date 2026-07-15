#include "screen.h"
#include "os_type.h"
#include "asm_utils.h"
#include "os_modules.h"
#include "stdarg.h"
#include "stdlib.h"
#include "vsprintf.h"

SCREEN::SCREEN()
{
    initialize();
}

void SCREEN::initialize()
{
    screen = (uint8 *)SCREEN_MEMORY;
}

void SCREEN::print(uint x, uint y, uint8 c, uint8 color)
{

    if (x >= 25 || y >= 80)
    {
        return;
    }

    uint pos = x * 80 + y;
    screen[2 * pos] = c;
    screen[2 * pos + 1] = color;
}

void SCREEN::print(uint8 c, uint8 color)
{
    if (c == '\n')
    {
        uint row = getCursor() / 80;
        if (row == 24)
        {
            rollUp();
        }
        else
        {
            ++row;
        }
        moveCursor(row * 80);
        return;
    }

    if (c == '\b')
    {
        backspace();
        return;
    }

    uint cursor = getCursor();
    screen[2 * cursor] = c;
    screen[2 * cursor + 1] = color;
    cursor++;
    if (cursor == 25 * 80)
    {
        rollUp();
        cursor = 24 * 80;
    }
    moveCursor(cursor);
}

void SCREEN::print(uint8 c)
{
    print(c, 0x07);
}

void SCREEN::moveCursor(uint position)
{
    if (position >= 80 * 25)
    {
        return;
    }

    uint8 temp;

    // 处理高8位
    temp = (position >> 8) & 0xff;
    asm_out_port(0x3d4, 0x0e);
    asm_out_port(0x3d5, temp);

    // 处理低8位
    temp = position & 0xff;
    asm_out_port(0x3d4, 0x0f);
    asm_out_port(0x3d5, temp);
}

uint SCREEN::getCursor()
{
    uint pos;
    uint8 temp;

    pos = 0;
    temp = 0;
    // 处理高8位
    asm_out_port(0x3d4, 0x0e);
    asm_in_port(0x3d5, &temp);
    pos = ((uint)temp) << 8;

    // 处理低8位
    asm_out_port(0x3d4, 0x0f);
    asm_in_port(0x3d5, &temp);
    pos = pos | ((uint)temp);

    return pos;
}

void SCREEN::moveCursor(uint x, uint y)
{
    if (x >= 25 || y >= 80)
    {
        return;
    }

    moveCursor(x * 80 + y);
}

void SCREEN::backspace()
{
    uint cursor = getCursor();
    if (cursor == 0)
    {
        return;
    }

    cursor--;
    screen[2 * cursor] = ' ';
    screen[2 * cursor + 1] = 0x07;
    moveCursor(cursor);
}

void SCREEN::rollUp()
{
    uint length;
    length = 25 * 80;
    for (uint i = 80; i < length; ++i)
    {
        screen[2 * (i - 80)] = screen[2 * i];
        screen[2 * (i - 80) + 1] = screen[2 * i + 1];
    }

    for (uint i = 24 * 80; i < length; ++i)
    {
        screen[2 * i] = ' ';
        screen[2 * i + 1] = 0x07;
    }
}

int SCREEN::print(const char *const str)
{
    int i = 0;

    for (i = 0; str[i]; ++i)
    {
        print(str[i]);
    }

    return i;
}

int kprintf(const char *const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsprintf_callback(kputc, fmt, ap);
    va_end(ap);
    return ret;
}

void kputc(char c) {
    screen.print((uint8)c);
}
