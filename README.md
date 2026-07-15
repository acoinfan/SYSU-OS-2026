# SYSU OS 2026

一个基于 i386 的教学操作系统实验项目。项目从基础内核框架出发，补充了进程调度、虚拟内存、系统调用、用户态运行时、FAT12 文件系统、VFS、ELF 加载和一个简单 shell，目标是让内核能够启动用户态程序，并通过 shell 访问文件系统。

## 功能概览

- i386 bootloader 与分页内核启动。
- 可调度的 kernel thread / user process 管理。
- `int 0x80` 系统调用接口。
- demand paging 与 copy-on-write fork。
- 用户态 `libc.a`，包含 `_start`、syscall wrapper、`printf`、字符串函数和 `malloc`。
- i386 ELF 解析与 `execve(path, argv)`。
- FAT12 文件系统与 VFS。
- 每进程 fd table + 全局 open file table。
- shell `osh`，支持执行外部 ELF、`cd`、`pwd`、前后台任务的基础语义。
- 基础用户命令：`ls`/`dir`、`mkdir`、`rmdir`、`touch`、`rm`、`echo`、`cat`、`writef`、`appendf`。
- 键盘输入作为 stdin，`fd 0/1/2` 对应 stdin/stdout/stderr。

## 目录结构

```text
build/          顶层构建入口与 config.mk
include/        kernel/libc/builtin 头文件
src/boot/       MBR、bootloader、早期分页代码
src/kernel/     内核主体
src/libc/       用户态 libc 与 syscall wrapper
src/bin/        系统自带用户态命令，安装到 root.img 的 /bin
src/test/       用户态测试程序，安装到 test.img
src/user/       用户自定义 ELF 编译入口，只负责编译到 run/user
run/root/       root.img 的种子目录，构建时生成
run/test/       test.img 的最小种子目录
```

## 环境依赖

需要一个能构建 32 位 freestanding C++ 的 Linux 环境。常见依赖包括：

```bash
sudo apt install build-essential gcc-multilib g++-multilib nasm qemu-system-x86 mtools dosfstools
```

项目使用 `mkfs.fat` 和 `mcopy/mmd` 构造 FAT12 镜像，因此需要 `dosfstools`/`mtools`。

## 构建与运行

在仓库根目录执行：

```bash
make -C build image
```

这会生成：

- `run/hd.img`：启动盘，包含 MBR、bootloader 和 kernel。
- `run/root.img`：root FAT12 盘，QEMU 中作为 `hdb`，挂载为 `/`。
- `run/test.img`：测试 FAT12 盘，QEMU 中作为 `hdc`，挂载为 `/mnt/test`。

运行系统：

```bash
make -C build run
```

启动后会进入用户态 shell：

```text
osh>
```

可以尝试：

```text
osh> ls
osh> cd /mnt/test
osh> pwd
osh> ls
osh> mkdir demo
osh> touch demo/a.txt
osh> echo hello
osh> exit
```

`exit` 会结束 `osh`，便于系统回收进程并触发文件系统同步逻辑。

## 导出磁盘内容

运行后如果想查看 FAT12 镜像内容：

```bash
make -C build export
```

导出路径：

```text
run/export/           root.img 的内容
run/export/mnt/test/  test.img 的内容
```

也可以分别导出：

```bash
make -C build export-root
make -C build export-test
```

## 用户程序：编译 ELF

用户自定义程序放在 `src/user`。每个 `.cpp` 会被编译成一个独立的 i386 ELF，输出到 `run/user/<文件名>`。

示例 `src/user/hello.cpp`：

```cpp
#include "stdio.h"
#include "syscall.h"

int main(int argc, char** argv)
{
    printf("hello from user elf\n");
    printf("argc=%d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d]=%s\n", i, argv[i]);
    }
    return 0;
}
```

编译：

```bash
make -C build user
```

输出：

```text
run/user/hello
```

用户程序入口是普通的：

```cpp
int main(int argc, char** argv)
```

`src/libc/start.cpp` 中的 `_start` 会调用 `main`，并把 `main` 的返回值传给 `exit`。

## 把用户 ELF 加载到 OS

`src/user` 默认只负责编译 ELF，不自动写入镜像。可以按需要手动拷贝到 root 镜像或 test 镜像。

拷贝到 root 盘的 `/bin`，作为 shell 命令运行：

```bash
mcopy -i run/root.img run/user/hello ::/bin/hello
```

然后启动系统：

```bash
make -C build run
```

在 shell 中执行：

```text
osh> hello arg1 arg2
```

拷贝到 test 盘：

```bash
mcopy -i run/test.img run/user/hello ::/hello
```

在 shell 中用绝对路径执行：

```text
osh> /mnt/test/hello arg1 arg2
```

如果你希望某个程序成为系统内置命令，更推荐把源码放到 `src/bin`。`make -C build image` 会自动把 `src/bin/*.cpp` 编译后安装到 `root.img` 的 `/bin`。

## 测试程序

`src/test` 放用户态测试 ELF，例如：

- `args`：测试 argc/argv。
- `complex`：综合测试文件、fork、wait、返回值。
- `kbdtest`：测试 stdin/KeyboardManager。
- `malloc`：测试用户态 malloc。

构建测试 ELF：

```bash
make -C build test
```

完整构建镜像时，`make -C build image` 会把 `src/test` 生成的 ELF 和 `run/test` 中的 seed 数据一起写入 `test.img`。

## Shell 与文件系统约定

当前系统使用两个 FAT12 盘：

- `/` 来自 `root.img`，其中 `/bin` 存放系统命令。
- `/mnt/test` 来自 `test.img`，用于测试和实验数据。

shell 查找外部命令时主要查找：

```text
/bin
/usr/bin
```

显式路径可以直接执行：

```text
osh> /mnt/test/args a b c
```

文件名需要注意 FAT12 的短文件名限制。为了兼容当前实现，建议使用 8.3 风格或短小文件名，例如 `complex`、`hello`、`test.txt`。

## 挂载额外磁盘

VFS 当前支持把 FAT12 磁盘挂载到 `/mnt/<disk_name>`。启动时在 `src/kernel/setup.cpp` 中完成默认挂载：

```cpp
fileManager.initialize(IdeDrive::PrimarySlave, fs_type::FXT12);
fileManager.mount("test", IdeDrive::SecondaryMaster, fs_type::FXT12);
```

其中：

- `PrimarySlave` 对应 QEMU 的 `hdb`，当前作为 root 盘 `/`。
- `SecondaryMaster` 对应 QEMU 的 `hdc`，当前作为 `/mnt/test`。
- `SecondarySlave` 对应 QEMU 的 `hdd`，可以用来接第三块实验盘。

如果要自己挂载一块新盘，可以：

1. 创建 FAT12 镜像：

```bash
dd if=/dev/zero of=run/data.img bs=1M count=8
mkfs.fat -F 12 -n DATAVOL run/data.img
```

2. 修改 QEMU 启动参数，例如在 `build/makefile` 的 `run` 目标中增加 `-hdd $(RUNDIR)/data.img`。

3. 在 `setup_kernel()` 中增加挂载：

```cpp
fileManager.mount("data", IdeDrive::SecondarySlave, fs_type::FXT12);
```

启动后访问路径就是：

```text
/mnt/data
```

当前限制：

- 最多支持 4 个 IDE 盘位，对应 `hda/hdb/hdc/hdd`。
- root 盘固定由 `fileManager.initialize()` 初始化，不应通过 `umount` 卸载。
- 目前实际实现的文件系统类型只有 FAT12。
- VFS 挂载名建议使用短小 ASCII 名称，例如 `test`、`data`。
- 路径最大长度为 256 字节。
- FAT12 目录项当前只稳定支持 SFN，也就是 8.3 短文件名。遇到 LFN 长文件名目录项时会跳过对应长名序列，因此建议镜像内文件名使用 `HELLO`、`TEST.TXT`、`COMPLEX` 这类短文件名。
- 当前 FAT12 实现主要面向实验环境，不建议把宿主机重要文件镜像直接挂上来测试。

## 开发与调试

常用开发命令：

```bash
make -C build image      # 完整构建内核、root.img、test.img
make -C build run        # curses 模式运行 QEMU
make -C build debug      # QEMU 等待 GDB 连接
make -C build gdb        # 使用 run/gdbinit 连接调试
make -C build export     # 导出 root/test 两个 FAT12 镜像内容
```

`make -C build debug` 会以 `-S -s` 启动 QEMU，CPU 会停在启动处等待 GDB。另一个终端执行：

```bash
make -C build gdb
```

内核调试输出主要使用：

- `kprintf(...)`：内核态直接打印。
- `LOG_TRACE(...)` 等宏：见 `include/kernel/debug.h`。
- `ASSERT(...)` / `PANIC(...)`：用于尽早暴露内核不变量错误。

文件系统相关问题可以优先使用：

- `make -C build export` 查看镜像最终内容。
- `fileManager.sync_all()` 或用户态 `sync()` 主动落盘。
- `src/kernel/setup.cpp` 中的 `test_fat12_fs()` 做 FAT12 层单测。
- `src/builtin/test.cpp` 中的 VFS / fd / fork 测试。
- `src/test/*.cpp` 中的用户态测试 ELF。

如果要调试用户态 ELF，通常先确认三件事：

1. ELF 是否真的被复制进了 `root.img` 或 `test.img`。
2. 文件名是否符合当前 FAT12 SFN 限制。
3. shell 执行路径是否正确，例如命令查找走 `/bin:/usr/bin`，测试盘程序则可以用 `/mnt/test/<name>` 显式执行。

## 清理

```bash
make -C build clean
```

这会清理构建产物和镜像文件。`run/test` 中提交到仓库的 seed 文件会保留。


## 致谢
感谢助教开发的框架(虽然被我改的差不多了)
感谢codex(bushi)
感谢Genshin Impact, Wuthering Wave, HSR
感谢Iuno, Denia, Fu Xuan, Aemeath

## 开发建议
如果想要优化这个os,大概还有几个方向
1. shell的行为测试不足(事实上没测试过bg fg), /bin提供的功能不够
2. 拓展libc
3. 支持pipe mmap之类的
4. 支持signal
5. 帮我找找那个Kernel Physical的内存泄漏问题
6. 实现Fat12文件系统的LFN支持(这个我记得留了接口,但是处理文件名实在太恶心了)
7. donate作者50rmb用于疯狂星期四开发(bushi)

## 写在后面
其实比较古早的版本就别用了, 我自己都不太记得原本的框架
这个90%人工开发,10%Codex(古法编程这一块)

写于2026/7/16 00:12, acoinfan + codex