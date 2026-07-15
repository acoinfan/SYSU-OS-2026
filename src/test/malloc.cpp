#include "os_type.h"
#include "stdio.h"
#include "stdlib.h"

static void fill_pattern(char* ptr, uint32 size, char pattern)
{
    for (uint32 i = 0; i < size; i++) {
        ptr[i] = pattern;
    }
}

static bool check_pattern(char* ptr, uint32 size, char pattern)
{
    for (uint32 i = 0; i < size; i++) {
        if (ptr[i] != pattern) {
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    printf("[malloc] begin\n");

    char* p1 = (char*)malloc(16);
    char* p2 = (char*)malloc(64);
    char* p3 = (char*)malloc(512);
    if (!p1 || !p2 || !p3) {
        printf("[malloc][fail] basic allocation\n");
        return 1;
    }

    fill_pattern(p1, 16, 'A');
    fill_pattern(p2, 64, 'B');
    fill_pattern(p3, 512, 'C');
    free(p2);

    if (!check_pattern(p1, 16, 'A') || !check_pattern(p3, 512, 'C')) {
        printf("[malloc][fail] isolation check\n");
        return 2;
    }

    void* big_p1 = malloc(3000);
    void* big_p2 = malloc(5000);
    void* big_p3 = malloc(2000);
    if (!big_p1 || !big_p2 || !big_p3) {
        printf("[malloc][fail] heap expansion\n");
        return 3;
    }

    free(p1);
    free(p3);
    free(big_p1);
    free(big_p2);
    free(big_p3);

    void* reuse = malloc(4096);
    if (!reuse) {
        printf("[malloc][fail] reuse allocation\n");
        return 4;
    }
    free(reuse);

    printf("[malloc] end\n");
    return 0;
}
