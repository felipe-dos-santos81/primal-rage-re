#include <stdio.h>
#include "test.h"

int g_failures = 0;

int main(void)
{
    test_scaffold();
    test_mem();
    printf(g_failures ? "FAILURES: %d\n" : "all checks passed\n", g_failures);
    return g_failures != 0;
}
