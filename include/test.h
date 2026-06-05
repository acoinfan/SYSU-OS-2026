#ifndef TEST_H
#define TEST_H

void test_out_of_memory(void* arg);
void test_lazy_alloc_thread(void* arg);
void COW_writer();
void COW_reader();
void fork_test();
void stack_test();
#endif