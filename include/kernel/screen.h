#ifndef SCREEN_H
#define SCREEN_H

#include "os_type.h"

class SCREEN
{
private:
    uint8 *screen;

public:
    SCREEN();
    // 初始化函数
    void initialize();
    // 打印字符c，颜色color到位置(x,y)
    void print(uint x, uint y, uint8 c, uint8 color);
    // 打印字符c，颜色color到光标位置
    void print(uint8 c, uint8 color);
    // 打印字符c，颜色默认到光标位置
    void print(uint8 c);
    // 打印字符串，颜色默认
    int print(const char *const str);
    // 移动光标到一维位置
    void moveCursor(uint position);
    // 移动光标到二维位置
    void moveCursor(uint x, uint y);
    // 获取光标位置
    uint getCursor();
    // 删除光标前一个字符
    void backspace();

private:
    // 滚屏
    void rollUp();
};

int kprintf(const char *const fmt, ...);
void kputc(char c);
#endif
