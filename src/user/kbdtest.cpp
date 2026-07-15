#include "stdio.h"
#include "stdlib.h"
#include "syscall.h"

static int strip_newline(char* s, int n)
{
    if (n > 0 && s[n - 1] == '\n') {
        s[n - 1] = '\0';
        return n - 1;
    }
    s[n] = '\0';
    return n;
}

static int append_log(const char* text)
{
    const char* path = "/kbdlog";
    int ret = -1;
    int fd = open(path, 0);
    printf("[kbdtest] log open -> %d\n", fd);
    if (fd < 0) {
        int create_ret = create_file(path, 0);
        printf("[kbdtest] log create -> %d\n", create_ret);
        fd = open(path, 0);
        printf("[kbdtest] log reopen -> %d\n", fd);
    }
    if (fd >= 3) {
        ret = fdappend(fd, (void*)text, strlen(text));
        printf("[kbdtest] log append %d -> %d\n", strlen(text), ret);
        int close_ret = close(fd);
        printf("[kbdtest] log close -> %d\n", close_ret);
    } else {
        printf("[kbdtest][fail] log fd invalid\n");
    }
    return ret;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    char buf[64];
    memset(buf, 0, sizeof(buf));

    printf("[kbdtest] begin\n");
    printf("[kbdtest] type a line and press Enter:\n");

    int n = read(0, buf, sizeof(buf) - 1);
    if (n < 0) {
        printf("[kbdtest][fail] read stdin -> %d\n", n);
        return 1;
    }

    int text_len = strip_newline(buf, n);
    printf("[kbdtest] read bytes = %d\n", n);
    printf("[kbdtest] text bytes = %d\n", text_len);
    printf("[kbdtest] text = %s\n", buf);

    int log_ret = 0;
    log_ret += append_log("[kbdtest] captured: ");
    log_ret += append_log(buf);
    log_ret += append_log("\n");
    printf("[kbdtest] sync begin\n");
    sync();
    printf("[kbdtest] sync end\n");

    printf("[kbdtest] wrote /kbdlog total append = %d\n", log_ret);
    printf("[kbdtest] return %d\n", text_len);
    return text_len;
}
