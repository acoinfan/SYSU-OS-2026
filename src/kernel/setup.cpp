#include "asm_utils.h"
#include "interrupt.h"
#include "screen.h"
#include "program.h"
#include "thread.h"
#include "sync.h"
#include "memory.h"
#include "address_pool.h"
#include "tss.h"
#include "system_service.h"
#include "test.h"
#include "debug.h"
#include "init.h"

// #include "fileSys/disk_driver.h" // for debug
// 屏幕IO处理器
SCREEN screen;
// 中断管理器
InterruptManager interruptManager;
// 程序管理器
ProgramManager programManager;
// 内存管理器
MemoryManager memoryManager;
// Task State Segment
TSS tss;
// 系统调用
SystemService systemService;

static void test_fat12_fs()
{
    FAT12_FS fs;
    fat12_inode *root = nullptr;
    fat12_inode *dir1 = nullptr;
    fat12_inode *testf = nullptr;
    fat12_inode *hello = nullptr;
    fat12_inode *firstdir = nullptr;
    char buf[128];
    int res = 0;

    debug_log_clear();
    test_log_printf("=== FAT12 TEST BEGIN ===\n");
    asm_delay(5000000000);
    test_log_printf("[1] mount and dump root dir\n");
    if (!fs.mount(IdeDrive::PrimarySlave)) {
        test_log_printf("[fail] mount fail\n");
        return;
    }
    test_log_printf("[ok] mount done\n");
    fs.dump_root_dir();

    asm_delay(5000000000);
    memset(buf, 0, sizeof(buf));

    test_log_printf("[2] lookup / read / write / append in dir1\n");
    dir1 = fs.lookup(root, "dir1");
    if (!dir1) {
        test_log_printf("[fail] lookup dir1 failed\n");
        fs.umount();
        return;
    }
    dir1->dump();

    asm_delay(5000000000);
    testf = fs.lookup(dir1, "testf");
    if (!testf) {
        test_log_printf("[fail] lookup dir1/testf failed\n");
        fs.release_inode(dir1);
        fs.umount();
        return;
    }
    testf->dump();
    asm_delay(5000000000);
    res = fs.read(testf, buf, 127, 0);
    test_log_printf("read %d bytes\n", res);
    test_log_printf("str = %s\n", res > 0 ? buf: 0);

    char write_buf[] = "longlonglonglong";
    res = fs.write(testf, write_buf, 5, 0);
    test_log_printf("write %d bytes\n", res);

    memset(buf, 0, sizeof(buf));
    res = fs.read(testf, buf, 127, 0);
    test_log_printf("after write read %d bytes\n", res);
    test_log_printf("str = %s\n", res > 0 ? buf: 0);

    char append_buf[] = " append something";
    res = fs.append(testf, append_buf, 9);
    test_log_printf("append %d bytes\n", res);

    memset(buf, 0, sizeof(buf));
    res = fs.read(testf, buf, 127, 0);
    test_log_printf("after append read %d bytes\n", res);
    test_log_printf("str = %s\n", res > 0 ? buf: 0);
    testf->dump();

    fs.release_inode(testf);
    fs.release_inode(dir1);
    
    asm_delay(5000000000);
    test_log_printf("[3] create / duplicate-create / remove file\n");
    if (fs.create_file(root, "hello.txt", fat12_attr::ARCHIVE)) {
        test_log_printf("[ok] create hello.txt ok\n");
    } else {
        test_log_printf("[fail] create hello.txt failed\n");
    }
    if (!fs.create_file(root, "hello.txt", fat12_attr::ARCHIVE)) {
        test_log_printf("[ok] duplicate create rejected\n");
    } else {
        test_log_printf("[fail] duplicate create should fail but passed\n");
    }
    fs.dump_root_dir();

    hello = fs.lookup(root, "hello.txt");
    if (!hello) {
        test_log_printf("[fail] lookup hello.txt failed\n");
    } else {
        hello->dump();
        if (!fs.remove(root, "hello.txt"))
        {
            test_log_printf("[ok] remove hello.txt blocked by refcount\n");
        }
        else
        {
            test_log_printf("[fail] remove hello.txt should fail but passed\n");
        }
        fs.release_inode(hello);
        hello = nullptr;

        if (fs.remove(root, "hello.txt"))
        {
            test_log_printf("[ok] remove hello.txt ok after release\n");
        }
        else
        {
            test_log_printf("[fail] remove hello.txt failed after release\n");
        }
    }
    fs.dump_root_dir();

    asm_delay(5000000000);
    test_log_printf("[4] create / duplicate-create / remove directory\n");
    if (fs.create_directory(root, "firstdir")) {
        test_log_printf("[ok] create firstdir ok\n");
    } else {
        test_log_printf("[fail] create firstdir failed\n");
    }
    if (!fs.create_directory(root, "firstdir")) {
        test_log_printf("[ok] duplicate dir rejected\n");
    } else {
        test_log_printf("[fail] duplicate dir should fail but passed\n");
    }
    fs.dump_root_dir();

    firstdir = fs.lookup(root, "firstdir");
    if (firstdir) {
        firstdir->dump();
        if (!fs.remove_directory(root, "firstdir"))
        {
            test_log_printf("[ok] remove firstdir blocked or not empty\n");
        }
        else
        {
            test_log_printf("[fail] remove firstdir should fail if in use\n");
        }
        fs.release_inode(firstdir);
        firstdir = nullptr;

        if (fs.remove_directory(root, "firstdir"))
        {
            test_log_printf("[ok] remove firstdir ok after release\n");
        }
        else
        {
            test_log_printf("[fail] remove firstdir failed after release\n");
        }
    }
    fs.dump_root_dir();

    asm_delay(5000000000);
    test_log_printf("[5] umount and remount\n");
    if (!fs.umount()) {
        test_log_printf("[fail] umount failed\n");
        return;
    }
    test_log_printf("[ok] umount ok\n");

    asm_delay(5000000000);
    if (!fs.mount(IdeDrive::PrimarySlave)) {
        test_log_printf("[fail] remount fail\n");
        return;
    }
    test_log_printf("[ok] remount ok\n");
    fs.dump_root_dir();

    asm_delay(5000000000);
    test_log_printf("[6] verify remounted content\n");
    root = nullptr;
    dir1 = fs.lookup(root, "dir1");
    if (dir1) {
        testf = fs.lookup(dir1, "testf");
        if (testf)
        {
            memset(buf, 0, sizeof(buf));
            res = fs.read(testf, buf, 127, 0);
            test_log_printf("after remount read %d bytes\n", res);
            test_log_printf("str = %s\n", res > 0 ? buf: 0);
            fs.release_inode(testf);
        }
        fs.release_inode(dir1);
    }

    test_log_printf("[7] write test log to fat12.log\n");
    {
        fat12_inode *log_file = fs.lookup(nullptr, "fat12.log");
        if (log_file) {
            fs.release_inode(log_file);
            if (!fs.remove(nullptr, "fat12.log")) {
                test_log_printf("[fail] remove old fat12.log failed\n");
            }
        }

        if (!fs.create_file(nullptr, "fat12.log", fat12_attr::ARCHIVE)) {
            test_log_printf("[fail] create fat12.log failed\n");
        } else {
            log_file = fs.lookup(nullptr, "fat12.log");
            if (!log_file) {
                test_log_printf("[fail] lookup fat12.log failed\n");
            } else {
                int log_size = debug_log_size();
                int written = fs.append(log_file, debug_log_data(), log_size);
                test_log_printf("append log file %d bytes\n", written);
                fs.release_inode(log_file);
            }
        }
    }

    fs.umount();
    test_log_printf("=== FAT12 TEST END ===\n");
}

void first_process()
{
    asm_system_call(0, 132, 324, 12, 124);
    asm_halt();
}

void first_thread(void *arg)
{
    // 第1个线程不可以返回
    // stdio.moveCursor(0);·
    // for (int i = 0; i < 25 * 80; ++i)
    // {
    //     stdio.print(' ');
    // }
    // stdio.moveCursor(0);
    interruptManager.disableTimeInterrupt();
    char *p0 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
    *p0 = 0;
    char *testp = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
    *testp = 0;
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)p0, 1);
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)testp, 1);

    char *p1 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 1, VP_RW);
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)p1, 1);
    p1 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 100, VP_RW);
    asm_halt();
    char *p2 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 10, VP_RW);
    char *p3 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 100, VP_RW);

    kprintf("%x %x %x\n", p1, p2, p3);
    *p3 = 0xFF;

    memoryManager.releasePages(AddressPoolType::KERNEL, (int)p2, 10);
    p2 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 100, VP_RW);

    kprintf("%x\n", p2);

    p2 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 10, VP_RW);

    kprintf("%x\n", p2);
    kprintf("0x100000 %x ;", *(int *)0x100000);
    // 获取0x100000对应的PTE信息,储存在0x101000 + 4 * 256
    kprintf("0x101400 %x\n", *(int *)0x101400);

    kprintf("Try to access 0x101000\n");
    int value0 = *(int *)0x101000;
    kprintf("value = %d\n", value0);
    *(int *)0x101000 = value0;
    kprintf("after value = %d\n", *(int *)0x101000);

    kprintf("Try to access 0xC0101000\n");
    int value1 = *(int *)0xC0101000;
    kprintf("value = %d\n", value1);
    *(int *)0xC0101000 = 114514;
    kprintf("after value = %d\n", *(int *)0xC0101000);

    kprintf("Try to access 0x40000000\n");
    int value2 = *(int *)0x40000000;
    kprintf("value = %d\n", value2);
    *(int *)0x40000000 = 114514;
    asm_halt();
}

void idle_thread(void *arg)
{
    kprintf("start idle, pid = 0\n");

    // debug: read write

    // char buf1[512], buf2[512] = "deadbeef";
    // ASSERT(ide_read_sector(IdeDrive::PrimarySlave, 0, buf1));
    // kprintf("previous: \n");
    // kprintf(buf1);
    // ASSERT(ide_write_sector(IdeDrive::PrimarySlave, 0, buf2));
    // ASSERT(ide_read_sector(IdeDrive::PrimarySlave, 0, buf1));
    // kprintf("\nafter: \n");
    // kprintf(buf1);
    // end debug

    // debug: fat12_fs read test
    // FAT12_FS fs;
    // if (!fs.mount(IdeDrive::PrimarySlave)) {
    //     kprintf("mount fail\n");
    // } else
    //     kprintf("mount done\n");

    // char test[100];
    // memset(test, 0, 100);
    // fat12_inode* inode = fs.lookup(nullptr, "test2.txt");
    // inode->dump();
    // int res = fs.read(inode, test, 99, 0);
    // kprintf("read %d bytes\n", res);
    // kprintf("str = %s, size = %d\n", test, strlen(test));

    // memset(test, 0, 100);
    // inode = fs.lookup(nullptr, "dir1");
    // inode->dump();
    // fat12_inode* inode2 = fs.lookup(inode, "testf");
    // if (!inode2) {
    //     kprintf("fail to find\n");
    // } else {
    //     inode->dump();
    //     res = fs.read(inode2, test, 99, 0);
    //     kprintf("read %d bytes\n", res);
    //     kprintf("str = %s, size = %d\n", test, strlen(test));
    // }
    // end debug

    // debug: fat12_dir_iter
    // fat12_inode* root = nullptr;
    // fat12_dir_iter iter;
    // if (iter.init(&fs, root)) {
    //     while (iter.next()) {
    //         fat12_normalized_entry entry = iter.get();
    //         entry.dump();
    //     }
    // }

    // end debug

    // debug: fat12 create/remove file

    // fat12_inode* dir = nullptr;
    // fs.create_file(dir, "hello.txt", fat12_attr::ARCHIVE);
    // fs.dump_root_dir();

    // if (!fs.create_file(dir, "hello.txt", fat12_attr::ARCHIVE)) {
    //     kprintf("file exists\n");
    // }

    // fat12_inode* file = fs.lookup(dir, "hello.txt");
    // if (!fs.remove(dir, "hello.txt")) {
    //     kprintf("file is in use\n");
    // }
    // fs.release_inode(file);
    // fs.remove(dir, "hello.txt");
    // fs.dump_root_dir();

    // end debug

    // debug: fat12 create-remove directory

    // fat12_inode* target_dir = nullptr;
    // fs.create_directory(target_dir, "firstdir");
    // fs.dump_root_dir();

    // if (!fs.create_directory(target_dir, "firstdir")) {
    //     kprintf("dir exists\n");
    // }

    // fat12_inode* to_remove = fs.lookup(target_dir, "firstdir");
    // if (!fs.remove_directory(target_dir, "firstdir")) {
    //     kprintf("dir is in use\n");
    // }
    // if (!fs.remove_directory(target_dir, "dir1")) {
    //     kprintf("dir1 has some files\n");
    // }
    // fs.release_inode(to_remove);
    // fs.remove_directory(target_dir, "firstdir");
    // fs.dump_root_dir();

    // end debug

    // debug: fat12 append, write

    // char test1[100];
    // memset(test1, 0, 100);
    // fat12_inode* inode11 = fs.lookup(nullptr, "dir1");
    // inode11->dump();
    // fat12_inode* inode21 = fs.lookup(inode11, "testf");
    // if (!inode21) {
    //     kprintf("fail to find\n");
    // } else {
    //     inode21->dump();
    //     int res = fs.read(inode21, test1, 99, 0);
    //     kprintf("read %d bytes\n", res);
    //     kprintf("str = %s, size = %d\n", test1, strlen(test1));
    // }

    // // 超出长度写
    // char test2[] = "longlonglonglong";
    // int res = fs.write(inode21, test2, 10, 0);
    // res = fs.read(inode21, test1, 99, 0);
    // kprintf("read %d bytes\n", res);
    // kprintf("str = %s, size = %d\n", test1, strlen(test1));

    // // append
    // char test3[] = "append something\n";
    // res = fs.append(inode21, test3, 10);
    // res = fs.read(inode21, test1, 99, 0);
    // kprintf("read %d bytes\n", res);
    // kprintf("str = %s, size = %d\n", test1, strlen(test1));
    // inode21->dump();

    // programManager.executeProcess((const char *)init, 0, 1);
    uint32 count = 0;
    // sleep
    test_fat12_fs();
    kprintf("idling\n");
    while (1)
    {
        count++;
        if (count == 100000000)
        {
            LOG_TRACE("idling\n");
            count = 0;
        };
    }
    // int pid = programManager.executeThread(test_lazy_alloc_thread, nullptr, "test_lazy_alloc_thread", 1);
    // if (pid == -1)
    // {
    //     printf("can not execute thread\n");
    //     asm_halt();
    // }
    // asm_halt();
}

extern "C" void setup_kernel()
{

    // 中断管理器
    interruptManager.initialize();
    interruptManager.setTimeInterrupt((void *)asm_time_interrupt_handler);
    
    // 输出管理器
    screen.initialize();
    kprintf("test%x\n", 1);
    // 进程/线程管理器
    programManager.initialize(SchedulerType::RR);
    
    // 内存管理器
    memoryManager.initialize();
    
    // 初始化系统调用
    systemService.initialize();
    
    // 创建第一个线程
    int pid = programManager.executeThread(idle_thread, nullptr, "idle thread", 1, true);
    if (pid == -1)
    {
        kprintf("can not execute thread\n");
        asm_halt();
    }
    
    PCB *firstThread;
    PCB rubbish;
    
    switch (programManager.sType)
    {
        case SchedulerType::RR:
        firstThread = programManager.rrScheduler.pickNext();
        break;
        case SchedulerType::FIFS:
        firstThread = programManager.fifsScheduler.pickNext();
        break;
    }
    firstThread->status = ProgramStatus::RUNNING;
    programManager.running = firstThread;
    
    // 第一次切换 pid=0
    
    interruptManager.enableTimeInterrupt();
    asm_switch_thread(&rubbish, firstThread);
    asm_halt();
}
