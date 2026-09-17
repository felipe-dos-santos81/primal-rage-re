#include <stdio.h>
#include "test.h"

int g_failures = 0;

int main(void)
{
    test_scaffold();
    test_mem();
    test_le();
    test_res();
    test_gra();
    test_gfx();
    test_input();
    test_host();
    test_flow();
    test_opl();
    test_mixer();
    printf(g_failures ? "FAILURES: %d\n" : "all checks passed\n", g_failures);
    return g_failures != 0;
}
