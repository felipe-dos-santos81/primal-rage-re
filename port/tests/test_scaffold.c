#include "test.h"

int test_scaffold(void)
{
    CHECK(1 + 1 == 2, "arithmetic works");
    CHECK_EQ_INT(sizeof(void *) >= 4, 1);
    return g_failures;
}
