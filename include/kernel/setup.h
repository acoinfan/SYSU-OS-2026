#ifndef ENTRY_H
#define ENTRY_H

extern "C" void setup_kernel();
void test_fat12_fs();
void test_out_of_memory(void* arg);
void test_lazy_alloc_thread(void* arg);

#endif