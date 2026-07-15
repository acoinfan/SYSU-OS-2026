#include "test.h"
#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"

#define USER_VADDR_START 0x8048000
#define PAGE_SIZE        4096
void test_print_something(void* arg) {
    write("Call Print something\n");
    write("Welcome to use SYSU-OS-2026\n");
    write("Author: A_coin_fan\n");
    write("Hello from 2026/6/9\n");
}


void COW_writer() {
    volatile char* p = (char*)USER_VADDR_START;
    char c[2] = {};
    c[0] = *p;
    c[1] = '\0';
    uint32 address = (uint32)p;
    pte_dump(address);
    write("[writer] before: ");
    write(&c[0]);    // 打印 'A'

    *p = 'Z';                                      // 写操作触发 COW
    c[0] = *p;
    write("\n[writer] after: ");
    write(&c[0]);

    pte_dump(address);;
}

void COW_reader() {
    volatile char* p = (char*)USER_VADDR_START;
    char c[2] = {};
    c[0] = *p;
    c[1] = '\0';
    uint32 address = (uint32)p;
    pte_dump(address);
    write("[reader] sees: ");
    write(&c[0]);  
    write("\n");
}

void fork_test() {
    int pid = fork();
    printf("[fork_test] call Fork_test\n");
    int cur_pid = getpid();

    if (pid == -1)
    {
        write("can not fork\n");
    }
    else
    {
        if (pid)
        {
            printf("I am father, pid = %d, waiting to rape the child\n", getpid());
            int sonPid = waitpid(pid, nullptr);
            printf("Father Rape his child (pid = %d)\n", sonPid);
        }
        else
        {
            printf("I am child, pid = %d\n", getpid());
            char str3[] = "Child is waiting for his child\n";
            char str4[] = "Child Rape his child\n";
            if (fork() == 0) {
                printf("I am child of child, pid = %d\n", getpid());
                execveFunc((uint32)test_print_something);
                // NEVER REACH HERE
                write("FAULT!!!!!!!\n");
            } else {
                printf("Child is waiting for his child\n");
                int sonPid = wait(nullptr);
                printf("Child Rape his child (pid = %d)\n", sonPid);
            }
        }
    }
    return;
}

void stack_test() {
    char buf[2 * PAGE_SIZE];

    for (int i = 0; i < sizeof(buf); i++)
        buf[i] = i % 26;

    write("done\n");
    return;
}

void test_file_open_close(void* arg) {
    write("[file_test] begin\n");

    int fd0 = open("/dir1/testf", 0);
    printf("[file_test] open /dir1/testf -> %d\n", fd0);

    int fd1 = open("/dir1/testf", 0);
    printf("[file_test] open same file again -> %d\n", fd1);

    int fd_bad = open("/dir1/not_exist", 0);
    printf("[file_test] open missing file -> %d\n", fd_bad);

    int close0 = close(fd0);
    printf("[file_test] close fd0 -> %d\n", close0);

    int close1 = close(fd1);
    printf("[file_test] close fd1 -> %d\n", close1);

    int close_again = close(fd1);
    printf("[file_test] close fd1 again -> %d\n", close_again);

    int fd2 = open("/dir1/testf", 0);
    printf("[file_test] reopen after close -> %d\n", fd2);

    int close2 = close(fd2);
    printf("[file_test] close fd2 -> %d\n", close2);

    write("[file_test] end\n");
}

void test_file_read_write(void* arg) {
    write("[rw_test] begin\n");

    char buf[64];
    memset(buf, 0, sizeof(buf));

    int fd = open("/dir1/testf", 0);
    printf("[rw_test] open -> %d\n", fd);

    int r = fdread(fd, buf, 31);
    printf("[rw_test] read -> %d, data = %s\n", r, buf);
    close(fd);

    char patch[] = "HELLO";
    fd = open("/dir1/testf", 0);
    int w = fdwrite(fd, patch, 5);
    printf("[rw_test] write -> %d\n", w);
    close(fd);

    memset(buf, 0, sizeof(buf));
    fd = open("/dir1/testf", 0);
    r = fdread(fd, buf, 31);
    printf("[rw_test] read after write -> %d, data = %s\n", r, buf);
    close(fd);

    write("[rw_test] end\n");
}

void test_file_append_create_remove_seek(void* arg) {
    write("[append_test] begin\n");

    int cr = create_file("/vfsnew.txt", 0);
    printf("[append_test] create /vfsnew.txt -> %d\n", cr);

    int fd = open("/vfsnew.txt", 0);
    printf("[append_test] open new file -> %d\n", fd);

    char part1[] = "abc";
    int a1 = fdappend(fd, part1, 3);
    printf("[append_test] append abc -> %d\n", a1);

    char part2[] = "def";
    int a2 = fdappend(fd, part2, 3);
    printf("[append_test] append def -> %d\n", a2);

    int seek0 = fseek(fd, 0, FSEEK_SET);
    printf("[append_test] seek set 0 -> %d\n", seek0);

    char buf[16];
    memset(buf, 0, sizeof(buf));
    int r = fdread(fd, buf, 6);
    printf("[append_test] read all -> %d, data = %s\n", r, buf);

    int seek_cur = fseek(fd, -3, FSEEK_CUR);
    printf("[append_test] seek cur -3 -> %d\n", seek_cur);

    memset(buf, 0, sizeof(buf));
    r = fdread(fd, buf, 3);
    printf("[append_test] read tail -> %d, data = %s\n", r, buf);

    int seek_end = fseek(fd, 0, FSEEK_END);
    printf("[append_test] seek end -> %d\n", seek_end);

    int rm_busy = remove_file("/vfsnew.txt");
    printf("[append_test] remove while open -> %d\n", rm_busy);

    int c = close(fd);
    printf("[append_test] close new file -> %d\n", c);

    int rm = remove_file("/vfsnew.txt");
    printf("[append_test] remove after close -> %d\n", rm);

    int fd_bad = open("/vfsnew.txt", 0);
    printf("[append_test] open removed file -> %d\n", fd_bad);
    if (fd_bad >= 0) {
        close(fd_bad);
    }

    int keep_cr = create_file("/vfskeep.txt", 0);
    printf("[append_test] create /vfskeep.txt -> %d\n", keep_cr);

    int keep_fd = open("/vfskeep.txt", 0);
    printf("[append_test] open keep file -> %d\n", keep_fd);
    if (keep_fd >= 0) {
        char keep[] = "create append fseek ok\n";
        int keep_append = fdappend(keep_fd, keep, 23);
        printf("[append_test] append keep file -> %d\n", keep_append);
        close(keep_fd);
    }

    sync();
    write("[append_test] sync done\n");

    write("[append_test] end\n");
}

void test_vfs_full(void* arg) {
    write("[vfs_full] begin\n");

    int mk0 = mkdir("/vfsa");
    printf("[vfs_full] mkdir /vfsa -> %d\n", mk0);

    int mk1 = mkdir("/vfsa/sub");
    printf("[vfs_full] mkdir /vfsa/sub -> %d\n", mk1);

    int cf = create_file("/vfsa/sub/data.txt", 0);
    printf("[vfs_full] create /vfsa/sub/data.txt -> %d\n", cf);

    int fd = open("/vfsa/sub/data.txt", 0);
    printf("[vfs_full] open data -> %d\n", fd);
    if (fd >= 0) {
        char data[] = "nested vfs file\n";
        int app = fdappend(fd, data, 16);
        printf("[vfs_full] append data -> %d\n", app);
        fd_dump(fd);

        int seek0 = fseek(fd, 0, FSEEK_SET);
        printf("[vfs_full] seek data head -> %d\n", seek0);

        char buf[32];
        memset(buf, 0, sizeof(buf));
        int rd = fdread(fd, buf, 16);
        printf("[vfs_full] read data -> %d, data = %s\n", rd, buf);
        close(fd);
    }

    int rm_nonempty = rmdir("/vfsa/sub");
    printf("[vfs_full] rmdir non-empty /vfsa/sub -> %d\n", rm_nonempty);

    int rf = remove_file("/vfsa/sub/data.txt");
    printf("[vfs_full] remove data file -> %d\n", rf);

    int rd_sub = rmdir("/vfsa/sub");
    printf("[vfs_full] rmdir /vfsa/sub -> %d\n", rd_sub);

    int rd_parent = rmdir("/vfsa");
    printf("[vfs_full] rmdir /vfsa -> %d\n", rd_parent);

    int keep_mk = mkdir("/vfsexport");
    printf("[vfs_full] mkdir /vfsexport -> %d\n", keep_mk);

    int keep_cf = create_file("/vfsexport/keep.txt", 0);
    printf("[vfs_full] create /vfsexport/keep.txt -> %d\n", keep_cf);

    int keep_fd = open("/vfsexport/keep.txt", 0);
    printf("[vfs_full] open keep -> %d\n", keep_fd);
    if (keep_fd >= 0) {
        char keep[] = "mkdir rmdir fd_dump ok\n";
        int keep_app = fdappend(keep_fd, keep, 23);
        printf("[vfs_full] append keep -> %d\n", keep_app);
        fd_dump(keep_fd);
        close(keep_fd);
    }

    sync();
    write("[vfs_full] sync done\n");
    write("[vfs_full] end\n");
}

static int fork_log_fd = -1;

static void fork_log(const char* str) {
    if (fork_log_fd >= 0 && str) {
        fdappend(fork_log_fd, (void*)str, strlen(str));
    }
}

static void fork_log_int(int value) {
    char num[16];
    int pos = 0;

    if (value < 0) {
        fork_log("-");
        value = -value;
    }

    itos(num, (uint32)value, 10);
    while (num[pos]) {
        pos++;
    }
    if (pos == 0) {
        fork_log("0");
    } else {
        fork_log(num);
    }
}

static void fork_log_kv(const char* key, int value) {
    fork_log(key);
    fork_log_int(value);
    fork_log("\n");
}

void test_fd_fork_process() {
    write("[fork_fd] begin\n");

    remove_file("/forkfd.log");
    create_file("/forkfd.log", 0);
    fork_log_fd = open("/forkfd.log", 0);
    fork_log("[fork_fd] begin\n");
    fork_log_kv("[fork_fd] log fd = ", fork_log_fd);

    remove_file("/forkfd.txt");
    int cr = create_file("/forkfd.txt", 0);
    printf("[fork_fd] create /forkfd.txt -> %d\n", cr);
    fork_log_kv("[fork_fd] create /forkfd.txt -> ", cr);

    int fd = open("/forkfd.txt", 0);
    printf("[fork_fd] open -> %d\n", fd);
    fork_log_kv("[fork_fd] open -> ", fd);
    if (fd < 0) {
        write("[fork_fd] open failed\n");
        fork_log("[fork_fd] open failed\n");
        exit(-1);
    }

    char data[] = "0123456789";
    int app = fdappend(fd, data, 10);
    printf("[fork_fd] append -> %d\n", app);
    fork_log_kv("[fork_fd] append -> ", app);

    int seek0 = fseek(fd, 0, FSEEK_SET);
    printf("[fork_fd] seek head -> %d\n", seek0);
    fork_log_kv("[fork_fd] seek head -> ", seek0);
    fd_dump(fd);

    int pid = fork();
    if (pid == -1) {
        write("[fork_fd] fork failed\n");
        fork_log("[fork_fd] fork failed\n");
        close(fd);
        exit(-1);
    }

    if (pid == 0) {
        printf("[fork_fd:child1] pid=%d inherited fd=%d\n", getpid(), fd);
        fork_log_kv("[fork_fd:child1] pid = ", getpid());
        fork_log_kv("[fork_fd:child1] inherited fd = ", fd);
        fd_dump(fd);

        char child_buf[6];
        memset(child_buf, 0, sizeof(child_buf));
        int rd = fdread(fd, child_buf, 5);
        printf("[fork_fd:child1] read -> %d, data = %s\n", rd, child_buf);
        fork_log_kv("[fork_fd:child1] read -> ", rd);
        fork_log("[fork_fd:child1] data = ");
        fork_log(child_buf);
        fork_log("\n");
        fd_dump(fd);

        write("[fork_fd:child1] exit without close\n");
        fork_log("[fork_fd:child1] exit without close\n");
        exit(11);
    }

    int waited = waitpid(pid, nullptr);
    printf("[fork_fd:parent] wait child1 -> %d\n", waited);
    fork_log_kv("[fork_fd:parent] wait child1 -> ", waited);
    fd_dump(fd);

    char parent_buf[6];
    memset(parent_buf, 0, sizeof(parent_buf));
    int rd = fdread(fd, parent_buf, 5);
    printf("[fork_fd:parent] read after child exit -> %d, data = %s\n", rd, parent_buf);
    fork_log_kv("[fork_fd:parent] read after child exit -> ", rd);
    fork_log("[fork_fd:parent] data = ");
    fork_log(parent_buf);
    fork_log("\n");
    fd_dump(fd);

    int pid2 = fork();
    if (pid2 == -1) {
        write("[fork_fd] second fork failed\n");
        fork_log("[fork_fd] second fork failed\n");
        close(fd);
        exit(-1);
    }

    if (pid2 == 0) {
        printf("[fork_fd:child2] pid=%d inherited fd=%d\n", getpid(), fd);
        fork_log_kv("[fork_fd:child2] pid = ", getpid());
        fork_log_kv("[fork_fd:child2] inherited fd = ", fd);
        fd_dump(fd);
        int c = close(fd);
        printf("[fork_fd:child2] close inherited fd -> %d\n", c);
        fork_log_kv("[fork_fd:child2] close inherited fd -> ", c);
        exit(12);
    }

    waited = waitpid(pid2, nullptr);
    printf("[fork_fd:parent] wait child2 -> %d\n", waited);
    fork_log_kv("[fork_fd:parent] wait child2 -> ", waited);
    fd_dump(fd);

    int c = close(fd);
    printf("[fork_fd:parent] close fd -> %d\n", c);
    fork_log_kv("[fork_fd:parent] close fd -> ", c);

    int fd2 = open("/forkfd.txt", 0);
    printf("[fork_fd:parent] reopen after all close -> %d\n", fd2);
    fork_log_kv("[fork_fd:parent] reopen after all close -> ", fd2);
    if (fd2 >= 0) {
        fd_dump(fd2);
        close(fd2);
    }

    fork_log("[fork_fd] end\n");
    if (fork_log_fd >= 0) {
        close(fork_log_fd);
        fork_log_fd = -1;
    }
    sync();
    write("[fork_fd] sync done\n");
    write("[fork_fd] end\n");
    exit(0);
}

