#include "platform/input.h"
#include "mem.h"
#include "symbols.h"
#include "test.h"
#include <stdint.h>

/* PORT: scratch linear address inside mem[] the test points the BIOS base
 * (DS_00101514) at, matching game_init()'s GAME_BIOS_BASE. A host pointer would
 * break the mem[] linear-address invariant. */
#define TEST_KEY_BASE 0x3000000u

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

    {
        /* The input bitfield. 0x50161 is a level/latch selector over the raw
         * level word DAT_000E1C34; 0x4F644 turns it into the two masks the game
         * reads. Host bits enter through the key bitmap at
         * DS_00101514 + 0x2d8/0x2d9, which the test drives directly. */
        u32 saved_base = DSD(DS_00101514);
        u32 saved_30 = DSD(DS_000E1C30);
        u32 saved_34 = DSD(DS_000E1C34);
        u32 saved_38 = DSD(DS_000E1C38);
        u32 saved_3c = DSD(DS_000E1C3C);
        u16 saved_40 = DSW(DS_000E1C40);
        u8 saved_e4[4], saved_d8[4];

        mem_fill(TEST_KEY_BASE, 0, 0x400);
        DSD(DS_00101514) = TEST_KEY_BASE;
        for (u32 i = 0; i < 4u; i++) {
            saved_e4[i] = DSB(DS_001088E4 + i);
            saved_d8[i] = DSB(DS_001088D8 + i);
        }
        DSD(DS_000E1C30) = 0; DSD(DS_000E1C34) = 0; DSD(DS_000E1C38) = 0;
        DSD(DS_000E1C3C) = 0; DSW(DS_000E1C40) = 0;

        /* 0x2D2F0 is a constant 0 (`xor eax,eax; ret`). */
        CHECK_EQ_INT((int)input_joystick_device(0xFFu), 0);

        /* 0x500C4: level word = (byte[+0x2d8] << 24) | (byte[+0x2d9] << 8).
         * The debounce holds a bit's previous level for one frame, so a press
         * needs two samples to appear. */
        DSB(TEST_KEY_BASE + 0x2d9) = 0x01;           /* mask 0x00000100 */
        CHECK_EQ_INT((int)input_pump(), 0);          /* change is debounced */
        CHECK_EQ_INT((int)input_pump(), 0x00000100); /* second sample holds it */

        /* 0x4F644: with the joystick accessor 0 the merge never fires, so
         * DS_001088E4 is the newly-pressed bits and DS_001088D8 the held bits.
         * The press is visible in E4 for exactly one frame. */
        input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0x00000100);
        CHECK_EQ_INT((int)DSD(DS_001088D8), 0x00000100);

        input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0);          /* no longer new */
        CHECK_EQ_INT((int)DSD(DS_001088D8), 0x00000100); /* still held */

        /* Release: the debounce holds the level one more frame, then drops it;
         * both masks clear. E4 is the newly-pressed (rising) edge, so it stays
         * clear through a release. */
        DSB(TEST_KEY_BASE + 0x2d9) = 0;
        CHECK_EQ_INT((int)input_pump(), 0x00000100);  /* still held this frame */
        CHECK_EQ_INT((int)input_pump(), 0);           /* dropped on the second */
        input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0);
        CHECK_EQ_INT((int)DSD(DS_001088D8), 0);

        /* 0x50161 applies its mask to the latch and leaves the unmasked bits of
         * the level word alone. */
        DSD(DS_000E1C34) = 0x0F000000u;
        DSD(DS_000E1C38) = 0;
        CHECK_EQ_INT((int)input_select_bits(0x01000000u), 0x0F000000);
        CHECK_EQ_INT((int)DSD(DS_000E1C38), 0x01000000);
        CHECK_EQ_INT((int)input_select_bits(0x10000000u), 0x0F000000);
        CHECK_EQ_INT((int)DSD(DS_000E1C38), 0x01000000);   /* bit 28 not live yet */
        DSD(DS_000E1C34) = 0x1F000000u;                    /* now bit 28 is live */
        CHECK_EQ_INT((int)input_select_bits(0x10000000u), 0x1F000000);
        CHECK_EQ_INT((int)DSD(DS_000E1C38), 0x11000000);   /* latched, not held */

        DSD(DS_00101514) = saved_base;
        DSD(DS_000E1C30) = saved_30; DSD(DS_000E1C34) = saved_34;
        DSD(DS_000E1C38) = saved_38; DSD(DS_000E1C3C) = saved_3c;
        DSW(DS_000E1C40) = saved_40;
        for (u32 i = 0; i < 4u; i++) {
            DSB(DS_001088E4 + i) = saved_e4[i];
            DSB(DS_001088D8 + i) = saved_d8[i];
        }
    }

    return g_failures - before;
}
