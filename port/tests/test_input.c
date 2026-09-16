#include "platform/input.h"
#include "test.h"

/* Dequeues one key, failing instead of blocking forever: input_get_key() spins
 * on host_pump() while empty, and the headless suite has no window to produce a
 * key, so a missing key must fail rather than hang. */
static int take(void)
{
    CHECK(input_has_key(), "key present before input_get_key");
    if (!input_has_key()) return -1;
    return (int)input_get_key();
}

int test_input(void)
{
    int before = g_failures;

    input_clear();
    CHECK(!input_has_key(), "cleared queue is empty");
    CHECK_EQ_INT(input_check_key(), 0);

    /* FIFO order and (scan << 8) | ascii packing; check_key() peeks. */
    input_push(0x1E, 'a');
    input_push(0x10, 'Q');
    CHECK(input_has_key(), "has_key after pushes");
    CHECK_EQ_INT(input_check_key(), (0x1E << 8) | 'a');
    CHECK(input_has_key(), "check_key did not consume");
    CHECK_EQ_INT(take(), (0x1E << 8) | 'a');
    CHECK_EQ_INT(take(), (0x10 << 8) | 'Q');
    CHECK(!input_has_key(), "queue drained in FIFO order");

    /* Packing edges: scan 0, ascii 0, both 0xFF. */
    input_push(0x00, 'x');
    input_push(0x48, 0x00);
    input_push(0xFF, 0xFF);
    CHECK_EQ_INT(take(), 0x0078);
    CHECK_EQ_INT(take(), 0x4800);
    CHECK_EQ_INT(take(), 0xFFFF);

    /* input_clear() empties a non-empty queue. */
    input_push(0x1E, 0x1E);
    input_clear();
    CHECK(!input_has_key(), "input_clear() empties the queue");
    CHECK_EQ_INT(input_check_key(), 0);

    /* Overflow drops the OLDEST entry: pushing 0..99 into the 64-entry ring
     * leaves exactly 36..99, in order. Draining the whole window and asserting
     * each value proves which entries survived, not merely that the count
     * capped (a "drop newest" bug would leave 0..63 and fail on the first). */
    input_clear();
    for (int i = 0; i < 100; i++) input_push((u8)i, (u8)(i ^ 0x5A));
    for (int i = 36; i < 100; i++)
        CHECK_EQ_INT(take(), ((i & 0xFF) << 8) | (i ^ 0x5A));
    CHECK(!input_has_key(), "ring held exactly 64 entries");

    return g_failures - before;
}
