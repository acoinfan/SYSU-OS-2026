#include "pageinfo.h"
#include "assert.h"
#include "stdio.h"

uint16 PageInfo::incRef(void) {
    ASSERT(ref < 0xFFFF);
    return ++ref;
}

uint16 PageInfo::decRef(void) {
    ASSERT(ref > 0);
    return --ref;
}

uint16 PageInfo::getRef(void) const {
    return ref;
}

void PageInfo::setFlag(PageFlags flag) {
    this->flags |= flag;
}

void PageInfo::setFlag(uint16 mask) {
    this->flags |= mask;
}

void PageInfo::clearFlag(PageFlags flag) {
    this->flags &= (~flag);
}

void PageInfo::clearFlag(uint16 mask) {
    this->flags &= (~mask);
}

bool PageInfo::hasFlag(PageFlags flag) const {
    return !!(this->flags & flag);
}

void PageInfo::dump(void) const {
    printf("PageInfo debug: at address 0x%x\n"
            "ref = %x\n"
            "flags = %x\n"
            "extra = %x\n",
            this, ref, flags, extra);
}

void PageInfo::clear() {
    flags = 0;
    ref = 0;
    extra = 0;
}
