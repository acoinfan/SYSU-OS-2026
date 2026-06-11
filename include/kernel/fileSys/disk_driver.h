#ifndef DISK_DRIVER
#define DISK_DRIVER

#define IDE_PRIMARY_IO      0x1F0
#define IDE_PRIMARY_CTRL    0x3F6
#define IDE_SECONDARY_IO    0x170
#define IDE_SECONDARY_CTRL  0x376

#define IDE_REG_DATA         0x0
#define IDE_REG_SECTOR_COUNT 0x2
#define IDE_REG_LBA_LOW      0x3
#define IDE_REG_LBA_MID      0x4
#define IDE_REG_LBA_HIGH     0x5
#define IDE_REG_DRIVE_SELECT 0x6
#define IDE_REG_COMMAND      0x7
#define IDE_REG_STATUS       0x7


#define IDE_DRIVE_COUNT 4

#define IDE_CMD_READ_SECTOR 0x20
#define IDE_CMD_WRITE_SECTOR 0x30
#define IDE_CMD_CACHE_FLUSH 0xE7

#define IDE_WAIT_RETRY 100000

#include "os_type.h"
#include "enum.h"

static inline void outb(uint16 port, uint8 val) {
    asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8 inb(uint16 port) {
    uint8 ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// insw: 从 port 读取 count 个 word (2 bytes) 到 buf
static inline void insw(uint16 port, void* buf, int count) {
    asm volatile("rep insw" : "=D"(buf), "=c"(count) : "d"(port), "0"(buf), "1"(count) : "memory");
}

static inline void outsw(uint16 port, const void* buf, int count) {
    asm volatile("rep outsw"
                 : "+S"(buf), "+c"(count)
                 : "d"(port)
                 : "memory");
}

typedef struct {
    uint16 io_base;
    uint16 ctrl_base;
    uint8  drive_select;
} IdeChannel;

// 根据驱动号计算通道信息
static inline IdeChannel ide_get_channel(IdeDrive drive) {
    IdeChannel channel;
    switch (drive) {
    case IdeDrive::PrimaryMaster:
        channel.io_base = IDE_PRIMARY_IO;
        channel.ctrl_base = IDE_PRIMARY_CTRL;
        channel.drive_select = 0xE0; // 0xA0 | 0x40, master + LBA
        break;
    case IdeDrive::PrimarySlave:
        channel.io_base = IDE_PRIMARY_IO;
        channel.ctrl_base = IDE_PRIMARY_CTRL;
        channel.drive_select = 0xF0; // 0xB0 | 0x40, slave + LBA
        break;
    case IdeDrive::SecondaryMaster:
        channel.io_base = IDE_SECONDARY_IO;
        channel.ctrl_base = IDE_SECONDARY_CTRL;
        channel.drive_select = 0xE0;
        break;
    case IdeDrive::SecondarySlave:
        channel.io_base = IDE_SECONDARY_IO;
        channel.ctrl_base = IDE_SECONDARY_CTRL;
        channel.drive_select = 0xF0;
        break;
    }
    return channel;
}

static inline uint16 ide_reg(const IdeChannel& channel, uint16 reg) {
    return channel.io_base + reg;
}

// 等待 IDE 就绪：BSY=0（必要时 DRQ=1），并检查错误位
bool ide_wait_ready(IdeDrive drive, int require_drq);
void ide_send_control(IdeDrive drive, uint8 value);

// ATA 规范要求在写入驱动选择寄存器后等待约 400ns
void ide_select_drive_delay(IdeDrive drive);

// lba: 扇区号 (0-based)
// buf: 至少512B
bool ide_read_sector(IdeDrive drive, uint32 lba, void* buf);

bool ide_write_sector(IdeDrive drive, uint32 lba, const void* buf);
#endif
