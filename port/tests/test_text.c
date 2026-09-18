/* port/tests/test_text.c — the text renderer (Task 8b): 0x2F830 and its
 * 0x2F5A0 glyph emitter. The raw disassembly shows text is not a framebuffer
 * blit: 0x2F5A0 releases the cell's old record through 0x2AD40 and spawns each
 * non-space glyph as an actor through 0x2AE14, storing the record offset in the
 * 43-wide cell grid at DS_00105F38. The glyph's pixels therefore arrive later
 * through the actor display list (0x1C390), so the assertions below check the
 * grid, the spawned glyph's pset sprite id and position, and the cursor extent,
 * all hand-computed from the disassembly and the loaded font tables at
 * DS 0x3D048 / 0x3D1EC / 0x3CD7C / 0x3D38D. */
#include "test.h"
#include "game/actors.h"
#include "platform/res.h"
#include "mem.h"
#include "symbols.h"

static u32 grid(s32 row, s32 col)
{
    return DSD(DS_00105F38 + (u32)row * 0xacu + (u32)col * 4u);
}

int test_text(void)
{
    int before = g_failures;
    if (res_count() != 69)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") == 69,
              "index loaded");
    CHECK(actors_init() == 1, "actors_init validates the pools");

    /* Mode 3 maps by class: 'A' -> class 10 (table entry sprite 0x3FDD, width
     * 16), 'I' -> class 18 (sprite 0x3FE5, width 8). 0x2F5A0 advances the
     * running column by the glyph width, so "AI" lands at columns 0 and 2; the
     * cursor is advanced by 0x2F830's return (the glyph count), not the pixel
     * width, so its high word is 0 + 2. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    text_cursor_set(0, 0, (const u8 *)"AI", 3);
    u32 ga = grid(0, 0), gi = grid(0, 2);
    CHECK(ga != 0, "A glyph spawned at col 0");
    CHECK(gi != 0, "I glyph spawned at col 2");
    CHECK_EQ_INT((int)grid(0, 1), 0);              /* col 1 skipped: width 16 */
    if (ga != 0) {
        CHECK_EQ_INT((int)(DSW(actor_pset(ga)) & 0x7fffu), 0x3fdd);
        CHECK_EQ_INT((int)DSD(ga + 0x18), 0);      /* x = col 0 * 0x200 */
        CHECK_EQ_INT((int)DSD(ga + 0x1c), 0);      /* y = row 0 * 0x200 */
        CHECK_EQ_INT((int)DSW(ga + 0x40), 0x2000); /* extent 0x80 << 6 */
    }
    if (gi != 0) {
        CHECK_EQ_INT((int)(DSW(actor_pset(gi)) & 0x7fffu), 0x3fe5);
        CHECK_EQ_INT((int)DSD(gi + 0x18), 0x400);  /* x = col 2 * 0x200 */
        CHECK_EQ_INT((int)DSD(gi + 0x1c), 0);
    }
    CHECK_EQ_INT((int)DSW(DS_00105F34), 0);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 2);

    /* Mode 0 indexes the char table at DS 0x3CD7C (all width 8, advance 1). A
     * space is counted and advances the column but spawns no actor. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    text_cursor_set(0, 0, (const u8 *)"A B", 0);
    u32 g0 = grid(0, 0), g1 = grid(0, 1), g2 = grid(0, 2);
    CHECK(g0 != 0 && g2 != 0, "A and B glyphs");
    CHECK_EQ_INT((int)g1, 0);                      /* space: no actor */
    if (g0 != 0)
        CHECK_EQ_INT((int)(DSW(actor_pset(g0)) & 0x7fffu), 0x3f56);
    if (g2 != 0)
        CHECK_EQ_INT((int)(DSW(actor_pset(g2)) & 0x7fffu), 0x3f57);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 3);    /* glyph count includes space */

    /* All-spaces takes 0x2F830's early arm: it clears the width-long run of
     * cells through 0x2F280 (releasing their records) and returns 0. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    u32 r0 = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                         0x1B00u, 0u);
    u32 r1 = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                         0x1B00u, 0u);
    CHECK(r0 != 0 && r1 != 0, "pre-placed cells");
    DSD(DS_00105F38) = r0;
    DSD(DS_00105F38 + 4u) = r1;
    text_cursor_set(0, 0, (const u8 *)"  ", 0);
    CHECK_EQ_INT((int)DSD(DS_00105F38), 0);
    CHECK_EQ_INT((int)DSD(DS_00105F38 + 4u), 0);
    if (r0 != 0) CHECK_EQ_INT((int)(DSW(r0 + 0x28) & 8u), 8);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 0);    /* extent 0 */

    /* An occupied cell is released through 0x2AD40 and re-spawned as the glyph:
     * the cell's pset sprite id changes from the pre-placed descriptor's
     * 0x2C11 to the glyph's 0x3F56. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    u32 old = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                          0x1B00u, 0u);
    CHECK(old != 0, "old cell");
    DSD(DS_00105F38) = old;
    if (old != 0) CHECK_EQ_INT((int)(DSW(actor_pset(old)) & 0x7fffu), 0x2c11);
    text_cursor_set(0, 0, (const u8 *)"A", 0);
    u32 neu = grid(0, 0);
    CHECK(neu != 0, "cell re-spawned");
    if (neu != 0)
        CHECK_EQ_INT((int)(DSW(actor_pset(neu)) & 0x7fffu), 0x3f56);

    /* A negative class (mode 3 '"') makes 0x2F5A0 return 1, so 0x2F830 aborts
     * with 0 and emits nothing. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    text_cursor_set(0, 0, (const u8 *)"\"", 3);
    CHECK_EQ_INT((int)grid(0, 0), 0);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 0);

    return g_failures - before;
}
