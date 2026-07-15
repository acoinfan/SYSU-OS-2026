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
#include "fileSys/file_manager.h"
#include "keyboard.h"
#include "setup.h"

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
// 文件管理器
FileManager fileManager;
// 键盘输入管理器
KeyboardManager keyboardManager;

void idle_thread(void *arg)
{
    kprintf("start idle, pid = 0\n");
    uint32 count = 0;

    // test_fat12_fs();
    int pid = programManager.executeProcess((const char*)init, 0, 1);
    if (pid == -1)
    {
        kprintf("can not execute init\n");
        asm_halt();
    }
    kprintf("idling\n");
    while (1)
    {
        count++;
        if (count == 100000000)
        {
            // fileManager.sync_all();
            LOG_TRACE("idling\n");
            count = 0;
        };
    }
}

extern "C" void setup_kernel()
{

    // 中断管理器
    interruptManager.initialize();
    interruptManager.setTimeInterrupt((void *)asm_time_interrupt_handler);
    interruptManager.setKeyboardInterrupt((void *)asm_keyboard_interrupt_handler);
    
    // 输出管理器
    screen.initialize();
    kprintf("[setup] printf %x %d %u %s %c\n", 0x2a, -7, 42u, "ok", '!');
    // 进程/线程管理器
    programManager.initialize(SchedulerType::RR);
    
    // 内存管理器
    memoryManager.initialize();
    
    // 初始化系统调用
    systemService.initialize();

    // 文件管理器
    fileManager.initialize(IdeDrive::PrimarySlave, fs_type::FXT12);
    if (fileManager.mount("test", IdeDrive::SecondaryMaster, fs_type::FXT12) != 0) {
        kprintf("[setup] mount /mnt/test failed\n");
    }
    // 键盘输入管理器
    keyboardManager.initialize();
    
    // int testPid1 = programManager.executeThread(test_file_open_close, nullptr, "file open close test", 1, true);
    // if (testPid1 == -1)
    // {
    //     kprintf("can not execute file open close test\n");
    //     asm_halt();
    // }

    // int testPid2 = programManager.executeThread(test_file_read_write, nullptr, "file read write test", 1, true);
    // if (testPid2 == -1)
    // {
    //     kprintf("can not execute file read write test\n");
    //     asm_halt();
    // }

    // int testPid3 = programManager.executeThread(test_file_append_create_remove_seek, nullptr, "file append create remove seek test", 1, true);
    // if (testPid3 == -1)
    // {
    //     kprintf("can not execute file append create remove seek test\n");
    //     asm_halt();
    // }

    // int testPid4 = programManager.executeThread(test_vfs_full, nullptr, "vfs full test", 1, true);
    // if (testPid4 == -1)
    // {
    //     kprintf("can not execute vfs full test\n");
    //     asm_halt();
    // }

    // int testPid5 = programManager.executeProcess((const char*)test_fd_fork_process, 1, 1);
    // if (testPid5 == -1)
    // {
    //     kprintf("can not execute fd fork process test\n");
    //     asm_halt();
    // }
    
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
    interruptManager.enableKeyboardInterrupt();
    asm_switch_thread(&rubbish, firstThread);
    asm_halt();
}

// 以下是测试用函数
static FAT12_FS fat12_test_fs;

void test_fat12_fs()
{
    FAT12_FS& fs = fat12_test_fs;
    fat12_inode *root = nullptr;
    fat12_inode *dir1 = nullptr;
    fat12_inode *testf = nullptr;
    fat12_inode *hello = nullptr;
    fat12_inode *firstdir = nullptr;
    char buf[128];
    int res = 0;

    debug_log_init();
    debug_log_clear();
    test_log_printf("=== FAT12 TEST BEGIN ===\n");

    test_log_printf("[1] mount and dump root dir\n");
    if (!fs.mount(IdeDrive::SecondaryMaster)) {
        test_log_printf("[fail] mount fail\n");
        return;
    }
    test_log_printf("[ok] mount done\n");
    fs.dump_root_dir();

    memset(buf, 0, sizeof(buf));

    test_log_printf("[2] lookup / read / write / append in dir1\n");
    dir1 = fs.lookup(root, "dir1");
    if (!dir1) {
        test_log_printf("[fail] lookup dir1 failed\n");
        fs.umount();
        return;
    }
    dir1->dump();

    testf = fs.lookup(dir1, "testf");
    if (!testf) {
        test_log_printf("[fail] lookup dir1/testf failed\n");
        fs.release_inode(dir1);
        fs.umount();
        return;
    }
    testf->dump();
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

    test_log_printf("[5] umount and remount\n");
    if (!fs.umount()) {
        test_log_printf("[fail] umount failed\n");
        return;
    }
    test_log_printf("[ok] umount ok\n");

    if (!fs.mount(IdeDrive::SecondaryMaster)) {
        test_log_printf("[fail] remount fail\n");
        return;
    }
    test_log_printf("[ok] remount ok\n");
    fs.dump_root_dir();


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
    test_log_printf("=== FAT12 TEST END ===\n");
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

    fs.flush_all();
    fs.umount();
}

void test_out_of_memory(void* arg) {
    kprintf("Out Of Memory Begin\n");
    interruptManager.disableTimeInterrupt();
    char* p0 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
    char* p1 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);

    // OK
    *p0 = 0;
    kprintf("Part 1 OK\n");
    // FAIL
    *p1 = 0;
    kprintf("Never Reach Here if Kernel PA = 1\n");
    return;
}

void test_lazy_alloc_thread(void* arg) {
    kprintf("Lazy Alloc Begin\n");
    interruptManager.disableTimeInterrupt();
    char* p0 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
    // LAZY ALLOC RELEASE
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)p0, 1);

    // LAZY ALLOC TEST
    p0 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
    *p0 = 0;
    
    // LAZY_ALLOC & FIND_VICTIM
    char* testp = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
    *testp = 0;

    // VICTIM_RELEASE
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)p0, 1);
    // LAZY ALLOC RELEASE
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)testp, 1);
    kprintf("Lazy Alloc Done\n");
    interruptManager.enableTimeInterrupt();

    int pid = programManager.executeThread(test_out_of_memory, nullptr, "test_out_of_memory", 1, true);
    if (pid == -1)
    {
        kprintf("can not execute thread\n");
        asm_halt();
    }
    return;
}
