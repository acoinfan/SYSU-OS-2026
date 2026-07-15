#include "stdio.h"
#include "stdlib.h"
#include "syscall.h"

static int failures = 0;

static void expect_eq(const char* name, int got, int expected)
{
    if (got == expected) {
        printf("[complex][ok] %s = %d\n", name, got);
    } else {
        printf("[complex][fail] %s got %d expected %d\n", name, got, expected);
        failures++;
    }
}

static void expect_str(const char* name, const char* got, const char* expected)
{
    if (strcmp(got, expected) == 0) {
        printf("[complex][ok] %s = %s\n", name, got);
    } else {
        printf("[complex][fail] %s got %s expected %s\n", name, got, expected);
        failures++;
    }
}

static void test_memory()
{
    printf("[complex] memory begin\n");

    char* p = (char*)malloc(128);
    if (!p) {
        printf("[complex][fail] malloc returned null\n");
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
    printf("[complex] memory end\n");
}

static void test_stdio()
{
    printf("[complex] stdio begin pid=%d ppid=%d\n", getpid(), getppid());
    write(1, "[complex] stdout fd write\n", strlen("[complex] stdout fd write\n"));
    write(2, "[complex] stderr fd write\n", strlen("[complex] stderr fd write\n"));
    expect_eq("write stdin", write(0, "x", 1), -1);
    printf("[complex] stdio end\n");
}

static void test_file()
{
    printf("[complex] file begin\n");

    const char* path = "/complex.txt";
    remove_file(path);

    expect_eq("create", create_file(path, 0), 0);
    int fd = open(path, 0);
    if (fd < 3) {
        printf("[complex][fail] open returned %d\n", fd);
        failures++;
        return;
    }
    printf("[complex][ok] open fd = %d\n", fd);

    char hello[] = "hello";
    char tail[] = "+tail";
    char patch[] = "TAIL";
    expect_eq("append hello", fdappend(fd, hello, 5), 5);
    expect_eq("append tail", fdappend(fd, tail, 5), 5);
    expect_eq("seek set", fseek(fd, 0, FSEEK_SET), 0);

    char buf[16];
    memset(buf, 0, sizeof(buf));
    expect_eq("read file", read(fd, buf, 10), 10);
    expect_str("file data", buf, "hello+tail");

    expect_eq("seek patch", fseek(fd, 6, FSEEK_SET), 6);
    expect_eq("write patch", write(fd, patch, 4), 4);
    expect_eq("seek read patched", fseek(fd, 0, FSEEK_SET), 0);
    memset(buf, 0, sizeof(buf));
    expect_eq("read patched", read(fd, buf, 10), 10);
    expect_str("patched data", buf, "hello+TAIL");

    expect_eq("seek cur back", fseek(fd, -4, FSEEK_CUR), 6);
    memset(buf, 0, sizeof(buf));
    expect_eq("read tail", read(fd, buf, 4), 4);
    expect_str("tail data", buf, "TAIL");

    expect_eq("append stdout", fdappend(1, tail, 5), -1);
    expect_eq("close", close(fd), 0);
    expect_eq("remove", remove_file(path), 0);
    expect_eq("open removed", open(path, 0), -1);

    printf("[complex] file end\n");
}

static void test_subdir_file()
{
    printf("[complex] subdir begin\n");

    const char* dir = "/vdir";
    const char* path = "/vdir/nlog";
    remove_file(path);
    rmdir(dir);

    expect_eq("mkdir vdir", mkdir(dir), 0);
    expect_eq("create sub file", create_file(path, 0), 0);

    int fd = open(path, 0);
    if (fd < 3) {
        printf("[complex][fail] open sub fd = %d\n", fd);
        failures++;
        return;
    }

    char msg[] = "subdir-data";
    char buf[16];
    expect_eq("append sub file", fdappend(fd, msg, 11), 11);
    expect_eq("seek sub head", fseek(fd, 0, FSEEK_SET), 0);
    memset(buf, 0, sizeof(buf));
    expect_eq("read sub file", read(fd, buf, 11), 11);
    expect_str("sub file data", buf, "subdir-data");
    expect_eq("close sub fd", close(fd), 0);
    sync();

    fd = open(path, 0);
    if (fd < 3) {
        printf("[complex][fail] reopen sub fd = %d\n", fd);
        failures++;
        return;
    }
    memset(buf, 0, sizeof(buf));
    expect_eq("reread sub file", read(fd, buf, 11), 11);
    expect_str("reread sub data", buf, "subdir-data");
    expect_eq("close sub reopen", close(fd), 0);

    expect_eq("remove sub file", remove_file(path), 0);
    expect_eq("open removed sub", open(path, 0), -1);
    expect_eq("rmdir vdir", rmdir(dir), 0);
    expect_eq("open removed dir", open(dir, 0), -1);

    printf("[complex] subdir end\n");
}

static int test_fork()
{
    printf("[complex] fork begin\n");

    int pid = fork();
    if (pid < 0) {
        printf("[complex][fail] fork returned %d\n", pid);
        failures++;
        return 0;
    }

    if (pid == 0) {
        printf("[complex:child] pid=%d ppid=%d\n", getpid(), getppid());
        printf("[complex:child] return 0\n");
        return 1;
    }

    int retval = -1;
    int waited = waitpid(pid, &retval);
    expect_eq("wait pid", waited, pid);
    expect_eq("child return", retval, 0);
    printf("[complex] fork end\n");
    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    printf("[complex] begin\n");
    test_stdio();
    test_memory();
    test_file();
    test_subdir_file();
    if (test_fork()) {
        return 0;
    }
    printf("[complex] failures = %d\n", failures);
    printf("[complex] return 73\n");
    return 73;
}
