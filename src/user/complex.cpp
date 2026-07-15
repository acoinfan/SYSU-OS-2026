#include "stdio.h"
#include "stdlib.h"
#include "syscall.h"

static int failures = 0;

static void expect_eq(const char* name, int got, int expected)
{
    if (got == expected) {
        printf("[complex_elf][ok] %s = %d\n", name, got);
    } else {
        printf("[complex_elf][fail] %s got %d expected %d\n", name, got, expected);
        failures++;
    }
}

static void expect_str(const char* name, const char* got, const char* expected)
{
    if (strcmp(got, expected) == 0) {
        printf("[complex_elf][ok] %s = %s\n", name, got);
    } else {
        printf("[complex_elf][fail] %s got %s expected %s\n", name, got, expected);
        failures++;
    }
}

static void test_memory()
{
    printf("[complex_elf] memory begin\n");

    char* p = (char*)malloc(128);
    if (!p) {
        printf("[complex_elf][fail] malloc returned null\n");
        failures++;
        return;
    }

    for (int i = 0; i < 127; i++) {
        p[i] = 'A' + (i % 26);
    }
    p[127] = '\0';

    char tmp[16];
    memcpy(tmp, p + 26, 5);
    tmp[5] = '\0';
    expect_str("memcpy slice", tmp, "ABCDE");

    free(p);
    printf("[complex_elf] memory end\n");
}

static void test_stdio()
{
    printf("[complex_elf] stdio begin pid=%d ppid=%d\n", getpid(), getppid());
    write(1, "[complex_elf] stdout fd write\n", 30);
    write(2, "[complex_elf] stderr fd write\n", 30);
    expect_eq("write stdin", write(0, "x", 1), -1);
    printf("[complex_elf] stdio end\n");
}

static void test_file()
{
    printf("[complex_elf] file begin\n");

    const char* path = "/complex.txt";
    remove_file(path);

    expect_eq("create", create_file(path, 0), 0);
    int fd = open(path, 0);
    if (fd < 3) {
        printf("[complex_elf][fail] open returned %d\n", fd);
        failures++;
        return;
    }
    printf("[complex_elf][ok] open fd = %d\n", fd);

    char hello[] = "hello";
    char tail[] = "+tail";
    expect_eq("write file", write(fd, hello, 5), 5);
    expect_eq("append file", fdappend(fd, tail, 5), 5);
    expect_eq("seek set", fseek(fd, 0, FSEEK_SET), 0);

    char buf[16];
    memset(buf, 0, sizeof(buf));
    expect_eq("read file", read(fd, buf, 10), 10);
    expect_str("file data", buf, "hello+tail");

    expect_eq("seek cur back", fseek(fd, -4, FSEEK_CUR), 6);
    memset(buf, 0, sizeof(buf));
    expect_eq("read tail", read(fd, buf, 4), 4);
    expect_str("tail data", buf, "tail");

    expect_eq("append stdout", fdappend(1, tail, 5), -1);
    expect_eq("close", close(fd), 0);
    expect_eq("remove", remove_file(path), 0);
    expect_eq("open removed", open(path, 0), -1);

    printf("[complex_elf] file end\n");
}

static void test_fork()
{
    printf("[complex_elf] fork begin\n");

    int pid = fork();
    if (pid < 0) {
        printf("[complex_elf][fail] fork returned %d\n", pid);
        failures++;
        return;
    }

    if (pid == 0) {
        printf("[complex_elf:child] pid=%d ppid=%d\n", getpid(), getppid());
        return;
    }

    int retval = -1;
    int waited = waitpid(pid, &retval);
    expect_eq("wait pid", waited, pid);
    expect_eq("child return", retval, 0);
    printf("[complex_elf] fork end\n");
}

int main()
{
    printf("[complex_elf] begin\n");
    test_stdio();
    test_memory();
    test_file();
    test_fork();
    printf("[complex_elf] failures = %d\n", failures);
    printf("[complex_elf] return 73\n");
    return 73;
}
