#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "os_type.h"

#define KEYBOARD_BUFFER_SIZE 256

class KeyboardManager {
public:
    char buffer[KEYBOARD_BUFFER_SIZE];
    int head;
    int tail;
    int count;
    bool shift;

public:
    KeyboardManager();
    void initialize();
    void handleScancode(uint8 scancode);
    int read(char* buf, int size);

private:
    void push(char c);
    bool pop(char* c);
    char decode(uint8 scancode);
};

extern "C" void c_keyboard_interrupt(uint8 scancode);

#endif
