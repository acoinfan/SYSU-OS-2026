#include "fileSys/disk_driver.h"

// 轮询硬盘状态，等待 BSY 清零，必要时确认 DRQ 置位，同时捕获 ERR/DF。
bool ide_wait_ready(IdeDrive drive, int require_drq) {
    IdeChannel channel = ide_get_channel(drive);
    uint8 status;
    for (int i = 0; i < IDE_WAIT_RETRY; ++i) {
        status = inb(ide_reg(channel, IDE_REG_STATUS));
        if (status & 0x21) { // ERR or DF
            return false;
        }
        if (!(status & 0x80)) { // BSY 清零
            if (!require_drq || (status & 0x08)) {
                return true;
            }
        }
        asm volatile("pause");
    }
    return false;
}

void ide_send_control(IdeDrive drive, uint8 value) {
    IdeChannel channel = ide_get_channel(drive);
    outb(channel.ctrl_base, value);
}

// 根据 ATA 规范在写入驱动选择寄存器后提供约 400ns 延迟。
void ide_select_drive_delay(IdeDrive drive) {
    IdeChannel channel = ide_get_channel(drive);
    inb(ide_reg(channel, IDE_REG_STATUS));
    inb(ide_reg(channel, IDE_REG_STATUS));
    inb(ide_reg(channel, IDE_REG_STATUS));
    inb(ide_reg(channel, IDE_REG_STATUS));
}

// 以 28-bit LBA 读取单个扇区，成功返回 true，失败返回 false。
bool ide_read_sector(IdeDrive drive, uint32 lba, void* buf) {
    IdeChannel channel = ide_get_channel(drive);

    // 选择目标盘并装载 LBA 高 4 位
    outb(ide_reg(channel, IDE_REG_DRIVE_SELECT), channel.drive_select | ((lba >> 24) & 0x0F));
    ide_send_control(drive, 0);
    ide_select_drive_delay(drive);

    // 设置待读扇区号与数量
    outb(ide_reg(channel, IDE_REG_SECTOR_COUNT), 1);
    outb(ide_reg(channel, IDE_REG_LBA_LOW),  lba & 0xFF);
    outb(ide_reg(channel, IDE_REG_LBA_MID),  (lba >> 8) & 0xFF);
    outb(ide_reg(channel, IDE_REG_LBA_HIGH), (lba >> 16) & 0xFF);

    // 下发 READ 命令
    outb(ide_reg(channel, IDE_REG_COMMAND), IDE_CMD_READ_SECTOR);

    // 等待设备准备好数据
    if (!ide_wait_ready(drive, 1)) {
        return false;
    }

    // 以 256 word 读出 512 字节数据
    insw(ide_reg(channel, IDE_REG_DATA), buf, 256);

    // 读完后再确认控制器空闲
    if (!ide_wait_ready(drive, 0)) {
        return false;
    }

    return true;
}

// 以 28-bit LBA 写入单个扇区并刷新缓存，成功返回 true，失败返回 false。
bool ide_write_sector(IdeDrive drive, uint32 lba, const void* buf) {
    IdeChannel channel = ide_get_channel(drive);

    // 选择目标磁盘并装载 LBA 高 4 位
    outb(ide_reg(channel, IDE_REG_DRIVE_SELECT), channel.drive_select | ((lba >> 24) & 0x0F));
    ide_select_drive_delay(drive);

    // 设置待写扇区号与数量
    outb(ide_reg(channel, IDE_REG_SECTOR_COUNT), 1);
    outb(ide_reg(channel, IDE_REG_LBA_LOW),  (uint8)(lba));
    outb(ide_reg(channel, IDE_REG_LBA_MID),  (uint8)(lba >> 8));
    outb(ide_reg(channel, IDE_REG_LBA_HIGH), (uint8)(lba >> 16));

    // 下发 WRITE 命令
    outb(ide_reg(channel, IDE_REG_COMMAND), IDE_CMD_WRITE_SECTOR);

    // 等待设备接受写入数据
    if (!ide_wait_ready(drive, 1)) {
        return false;
    }

    // 以 256 word 写入 512 字节数据
    outsw(ide_reg(channel, IDE_REG_DATA), buf, 256);

    // 数据阶段完成后再确认控制器空闲
    if (!ide_wait_ready(drive, 0)) {
        return false;
    }

    // 发送缓存刷新命令并等待完成
    outb(ide_reg(channel, IDE_REG_COMMAND), IDE_CMD_CACHE_FLUSH);
    if (!ide_wait_ready(drive, 0)) {
        return false;
    }
    ide_send_control(drive, 0);
    outb(ide_reg(channel, IDE_REG_COMMAND), IDE_CMD_CACHE_FLUSH);
    if (!ide_wait_ready(drive, 0)) {
        return false;
    }

    return true;
}
