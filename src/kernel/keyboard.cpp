#include "keyboard.h"
#include "interrupt.h"
#include "os_modules.h"
#include "program.h"
#include "screen.h"
#include "asm_utils.h"

static const char keymap[128] = {
    0,  0,  '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,  '\\',
    'z','x','c','v','b','n','m',',','.','/', 0,  '*', 0,  ' ',
};

static const char shift_keymap[128] = {
    0,  0,  '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0,  '|',
    'Z','X','C','V','B','N','M','<','>','?', 0,  '*', 0,  ' ',
};

KeyboardManager::KeyboardManager() {
    initialize();
}

void KeyboardManager::initialize() {
    head = 0;
    tail = 0;
    count = 0;
    shift = false;
}

void KeyboardManager::push(char c) {
    if (count >= KEYBOARD_BUFFER_SIZE) {
        return;
    }
    buffer[tail] = c;
    tail = (tail + 1) % KEYBOARD_BUFFER_SIZE;
    count++;
}

bool KeyboardManager::pop(char* c) {
    if (count <= 0) {
        return false;
    }
    *c = buffer[head];
    head = (head + 1) % KEYBOARD_BUFFER_SIZE;
    count--;
    return true;
}

char KeyboardManager::decode(uint8 scancode) {
    if (scancode >= sizeof(keymap)) {
        return 0;
    }
    return shift ? shift_keymap[scancode] : keymap[scancode];
}

void KeyboardManager::handleScancode(uint8 scancode) {
    bool released = scancode & 0x80;
    uint8 code = scancode & 0x7f;

    if (code == 0x2a || code == 0x36) {
        shift = !released;
        return;
    }
    if (released) {
        return;
    }

    char c = decode(code);
    if (c) {
        push(c);
    }
}

int KeyboardManager::read(char* buf, int size) {
    if (!buf || size <= 0) {
        return -1;
    }

    int n = 0;
    while (n < size) {
        bool status = interruptManager.getInterruptStatus();
        interruptManager.disableInterrupt();

        char c = 0;
        bool ok = pop(&c);
        interruptManager.setInterruptStatus(status);

        if (!ok) {
            if (programManager.running) {
                programManager.schedule();
            } else {
                asm_halt();
            }
            continue;
        }

        if (c == '\b') {
            if (n > 0) {
                n--;
                screen.backspace();
            }
            continue;
        }

        buf[n++] = c;
        screen.print(c);
        if (c == '\n') {
            break;
        }
    }
    return n;
}

extern "C" void c_keyboard_interrupt(uint8 scancode) {
    keyboardManager.handleScancode(scancode);
}
