#ifndef TEST_H
#define TEST_H

void test_print_something(void* arg);
// void test_out_of_memory(void* arg);
// void test_lazy_alloc_thread(void* arg);
void COW_writer();
void COW_reader();
void fork_test();
void stack_test();
void test_file_open_close(void* arg);
void test_file_read_write(void* arg);
void init_process(void* arg);
#endif
