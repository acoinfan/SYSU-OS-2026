#pragma once
#include "os_type.h"
void fill_pattern(char* ptr, uint32 size, char pattern);
bool check_pattern(char* ptr, uint32 size, char pattern);
int malloc_test();