/* port/tests/test_anim.c — the animation-stream interpreter (Task 7).
 * Direct tests of 0x29F34/0x29DB8/0x2A408 plus the spawn and frame-timer walks
 * that consume them, and the Task 1 opcode-8 pin mirror. */
#include "test.h"
#include "game/actors.h"
#include "game/rng.h"
#include "platform/res.h"
#include "mem.h"
#include "symbols.h"
#include <string.h>

/* Scratch mem[] away from every resource the index allocator hands out (the
 * loaded image ends near 41 MB; MEM_SIZE is 64 MB). */
#define ANIM_SCRATCH 0x3E00000u
#define ANIM_DESC    (ANIM_SCRATCH + 0x800u)

static u32 anim_alloc_record(void)
{
    actors_reset();
    DSB(DS_00104B24) = 0;
    DSB(DS_00104B26) = 0;
    return actor_alloc(0);
}

/* 0x29F34: the variable reader. Format reference F: < 0x40 indexes the ring
 * through rec+0x51; 0x40..0x45 are the record's own fields (0x40..0x43 and 0x45
 * sign-extended, 0x44 the pset-slot word); 0x46..0x4B the parent and 0x4C..0x51
 * the child. 0x29DB8 is the mirror. */
static void check_read_write_var(void)
{
    u32 rec = anim_alloc_record();
    CHECK(rec != 0, "reader record");
    if (rec == 0) return;

    for (u32 i = 0; i < 0x40; i++) DSW(DS_00105B4C + i * 2u) = (u16)(0x1000u + i);
    DSB(rec + 0x51) = 0x10;
    CHECK_EQ_INT((int)anim_read_var(rec, 0x00),
                 (int)DSW(DS_00105B4C + ((0x00u + 0x10u) & 0x3fu) * 2u));
    CHECK_EQ_INT((int)anim_read_var(rec, 0x30),
                 (int)DSW(DS_00105B4C + ((0x30u + 0x10u) & 0x3fu) * 2u));

    DSB(rec + 0x52) = 0x80; CHECK_EQ_INT((int)anim_read_var(rec, 0x40), 0xff80);
    DSB(rec + 0x53) = 0x7f; CHECK_EQ_INT((int)anim_read_var(rec, 0x41), 0x007f);
    DSB(rec + 0x54) = 0xff; CHECK_EQ_INT((int)anim_read_var(rec, 0x42), 0xffff);
    DSB(rec + 0x55) = 0x01; CHECK_EQ_INT((int)anim_read_var(rec, 0x43), 0x0001);
    DSW(rec + 0x56) = 0xbeef; CHECK_EQ_INT((int)anim_read_var(rec, 0x44), 0xbeef);
    DSB(rec + 0x58) = 0x90; CHECK_EQ_INT((int)anim_read_var(rec, 0x45), 0xff90);

    u32 parent = actor_alloc(0);
    CHECK(parent != 0, "parent record");
    if (parent != 0) {
        DSB(rec + 0x4a) = (u8)actor_index(parent);
        DSB(parent + 0x52) = 0x85; DSB(parent + 0x53) = 0x02;
        DSB(parent + 0x54) = 0xfe; DSB(parent + 0x55) = 0x01;
        DSW(parent + 0x56) = 0x1234; DSB(parent + 0x58) = 0x80;
        CHECK_EQ_INT((int)anim_read_var(rec, 0x46), 0xff85);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x47), 0x0002);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x48), 0xfffe);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x49), 0x0001);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x4a), 0x1234);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x4b), 0xff80);
    }

    u32 child = actor_alloc(0);
    CHECK(child != 0, "child record");
    if (child != 0) {
        DSB(rec + 0x4b) = (u8)actor_index(child);
        DSB(child + 0x52) = 0x90; DSB(child + 0x53) = 0x03;
        DSB(child + 0x54) = 0xfd; DSB(child + 0x55) = 0x02;
        DSW(child + 0x56) = 0x4321; DSB(child + 0x58) = 0x7f;
        CHECK_EQ_INT((int)anim_read_var(rec, 0x4c), 0xff90);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x4d), 0x0003);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x4e), 0xfffd);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x4f), 0x0002);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x50), 0x4321);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x51), 0x007f);
    }

    anim_write_var(rec, 0x40, 0x1234);
    CHECK_EQ_INT((int)DSB(rec + 0x52), 0x34);
    anim_write_var(rec, 0x44, 0x1234);
    CHECK_EQ_INT((int)DSW(rec + 0x56), 0x0034);
    anim_write_var(rec, 0x05, 0xabcd);
    CHECK_EQ_INT((int)DSW(DS_00105B4C + ((0x05u + 0x10u) & 0x3fu) * 2u), 0xabcd);
    /* 0x29E6A: the parent/child byte stores take the operand byte at
     * DS_00105BE8, not the passed value; 0x4A/0x4B take the value. */
    if (parent != 0) {
        DSW(DS_00105BE8) = 0x11;
        anim_write_var(rec, 0x46, 0x99);
        CHECK_EQ_INT((int)DSB(parent + 0x52), 0x11);
        anim_write_var(rec, 0x4a, 0x55);
        CHECK_EQ_INT((int)DSW(parent + 0x56), 0x0055);
    }
    /* out-of-range ops are ignored, never written. */
    anim_write_var(rec, 0x7f, 0xffff);
}

/* 0x2A408: the literal reader, the two 0xD00 computed forms, the hflip fold,
 * and the real second title object's stream (descriptor 0x9AC94, pointer
 * 0x0E897A, which begins `40 CD` = word 0xCD40). */
static void check_next_sprite_id(void)
{
    u32 rec = anim_alloc_record();
    CHECK(rec != 0, "id record");
    if (rec == 0) return;
    u16 *s = (u16 *)(mem + ANIM_SCRATCH);
    u32 pset = actor_pset(rec);

    /* literal: word & 0x8000 clear, returned as word & 0x7FFF. The disassembly
     * (0x2A4AF `mov ecx,eax`) does not move the cursor for this form. */
    DSW(rec + 0x28) = 0;
    s[0] = 0x1234; DSD(rec + 8) = ANIM_SCRATCH;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x1234);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)ANIM_SCRATCH);

    /* hflip fold: the returned bit 0x8000 is (id & 0x8000) XOR (rec+0x28>>8 &
     * 0x40). A literal has its high bit clear, so setting the record flip sets
     * the returned bit. */
    s[0] = 0x1234; DSW(rec + 0x28) = 0x0000;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x1234);
    DSW(rec + 0x28) = 0x4000;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x9234);
    /* A computed id can carry the high bit: 0xCD40 + 0x8000 -> fold clears it
     * when the record flip is set. */
    DSB(rec + 0x52) = 0x00;
    s[0] = 0xcd40; s[1] = 0x8000; DSD(rec + 8) = ANIM_SCRATCH;
    DSW(rec + 0x28) = 0x4000;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x0000);
    DSW(rec + 0x28) = 0x0000;
    DSD(rec + 8) = ANIM_SCRATCH;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x8000);

    /* 0xD00 one-extra-word form: 0xCD40, (word >> 8 & 0x60) == 0x40, consumes
     * the operand word and returns read_var(rec, word & 0x7F) + that word. */
    DSW(rec + 0x28) = 0;
    DSB(rec + 0x52) = 0x10;
    s[0] = 0xcd40; s[1] = 0x0100; DSD(rec + 8) = ANIM_SCRATCH;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x0110);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)(ANIM_SCRATCH + 2));

    /* 0xD00 two-extra-word form: 0x8D00, (word >> 8 & 0x60) == 0. Consumes the
     * table pointer dword and returns table[read_var(rec, word & 0x7F)]. */
    DSW(DS_00105B4C) = 2; DSB(rec + 0x51) = 0;
    u32 tab = ANIM_SCRATCH + 0x200;
    s[0] = 0x8d00; *(u32 *)(mem + ANIM_SCRATCH + 2) = tab;
    *(u16 *)(mem + tab + 2 * 2) = 0xbeef;
    DSD(rec + 8) = ANIM_SCRATCH;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0xbeef);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)(ANIM_SCRATCH + 4));

    /* A 0x8000 word that is not 0xD00 keeps the current pset id (0x2A4A7
     * `mov cx,[edx]; and ch,0x7f`). The brief's 0xD100/0xD200 literals are not
     * 0xD00 words: (0xD100 & 0x1F00) == 0x0100. */
    DSW(pset) = 0x1234; DSW(rec + 0x28) = 0;
    s[0] = 0xd100; DSD(rec + 8) = ANIM_SCRATCH;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x1234);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)ANIM_SCRATCH);

    /* 0x2A408's flag-8 arm reads the id from the cursor itself. */
    DSW(rec + 0x28) = 0x0800; DSD(rec + 8) = 0x2222;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x2222);
}

/* The real asset: descriptor 0x9AC94's stream starts `40 CD 3D 02`, i.e.
 * word 0xCD40 (0xD00, one extra word, selector 0x40 -> rec+0x52) followed by
 * 0x023D. This closes the spec's open item on 0x29F34's enumeration with a
 * shipped asset rather than a synthetic one. */
static void check_real_stream(void)
{
    actors_reset();
    DSB(DS_00104B24) = 0;
    DSB(DS_00104B26) = 0;
    const u32 *desc = (const u32 *)(mem + 0x9AC94u);
    u32 rec = actor_spawn(desc, 0, 0xe4u, 0, 0);
    CHECK(rec != 0, "real-stream record");
    if (rec == 0) return;

    DSD(rec + 8) = 0x0e897au;          /* the stream head */
    DSB(rec + 0x52) = 0x10;            /* the 0x40 variable read */
    DSW(rec + 0x28) &= (u16)~0x4000u;
    u32 pset = actor_pset(rec);
    u32 id = anim_next_sprite_id(rec, pset);
    CHECK_EQ_INT((int)id, 0x024d);     /* 0x023D + rec+0x52 */
    CHECK_EQ_INT((int)DSD(rec + 8), 0x0e897cu);
}

/* Build a descriptor at ANIM_DESC whose stream is ANIM_SCRATCH, spawn it. */
static u32 anim_spawn_stream(void)
{
    memset(mem + ANIM_DESC, 0, 0x14);
    *(u32 *)(mem + ANIM_DESC + 0x00) = ANIM_SCRATCH;
    DSB(ANIM_DESC + 0x04) = 0x00;              /* render type 0 */
    DSB(ANIM_DESC + 0x05) = 0x00;
    return actor_spawn((const u32 *)(mem + ANIM_DESC), 0, 0, 0, 0);
}

/* The spawn/frame-timer walks: the literal word is left at the cursor by
 * 0x2A408, and the next walk advances the cursor by one word and loads it. */
static void check_walk(void)
{
    u16 *s = (u16 *)(mem + ANIM_SCRATCH);
    s[0] = 0x2c11; s[1] = 0x2c12;
    u32 rec = anim_spawn_stream();
    CHECK(rec != 0, "walk record");
    if (rec == 0) return;
    u32 pset = actor_pset(rec);
    CHECK_EQ_INT((int)DSW(pset), 0x2c11);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)ANIM_SCRATCH);

    DSD(rec + 0x20) = 0;                       /* expired timer */
    DSD(rec + 0x24) = 0x3f800000u;             /* 1.0f: frame_timer runs */
    DSW(rec + 0x2a) = 0;
    DSW(rec + 0x28) &= (u16)~0x0810u;
    actors_update();
    CHECK_EQ_INT((int)DSW(pset), 0x2c12);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)(ANIM_SCRATCH + 2));
}

/* Opcode 8 is the in-window RNG consumer; the Task 1 pin replaced its 0x5D7DC
 * call with `mov eax, 0`, mirrored by actors_pin_anim_tick_zero. Command word
 * 0x88FF is opcode 8 with operand 0xFFFF. */
static void check_opcode8_pin(void)
{
    u16 *s = (u16 *)(mem + ANIM_SCRATCH);
    s[0] = 0x88ff; s[1] = 0;
    actors_pin_anim_tick_zero(1);
    rng_seed(0xabcd);
    u32 rec = anim_spawn_stream();
    actors_pin_anim_tick_zero(0);
    CHECK(rec != 0, "opcode-8 record");
    if (rec == 0) return;
    union { float f; u32 u; } fu;
    fu.u = DSD(rec + 0x20);
    CHECK_EQ_INT((int)fu.f, 0);                /* pinned draw is exactly 0 */
    u32 pset = actor_pset(rec);
    CHECK_EQ_INT((int)DSW(pset), 0x01e1);      /* status 2 -> engine id 0x1E1 */
}

/* 0x2BCF4/0x2BC30: the stream-entry helpers also load the first id through
 * 0x2A408. 0x2BCF4 only re-points the cursor; 0x2BC30 also resets the cache and
 * the frame timer and pre-walks the commands. */
static void check_entry_helpers(void)
{
    u16 *s = (u16 *)(mem + ANIM_SCRATCH);
    u32 rec = anim_alloc_record();
    CHECK(rec != 0, "entry record");
    if (rec == 0) return;
    DSW(rec + 0x56) = 0;
    u32 pset = actor_pset(rec);

    s[0] = 0x2c21;
    DSW(rec + 0x28) = 0x0014;              /* bits 0x2BCF4 clears */
    DSB(rec + 0x2b) = 0xff;
    actors_anim_seek(rec, ANIM_SCRATCH);
    CHECK_EQ_INT((int)DSW(pset), 0x2c21);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)ANIM_SCRATCH);
    CHECK_EQ_INT((int)(DSW(rec + 0x28) & 0x0014), 0);

    s[0] = 0x2c22;
    DSB(rec + 0x50) = 0x55;
    DSB(rec + 0x61) = 0x66;
    DSB(rec + 0x2b) = 0xff;
    actors_anim_begin(rec, ANIM_SCRATCH, 7);
    CHECK_EQ_INT((int)DSW(pset), 0x2c22);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)ANIM_SCRATCH);
    CHECK_EQ_INT((int)DSB(rec + 0x50), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x61), 0);
    CHECK_EQ_INT((int)(DSB(rec + 0x2b) & 0x04), 0);
    union { float f; u32 u; } fu;
    fu.u = DSD(rec + 0x20);
    CHECK_EQ_INT((int)fu.f, 7);
    fu.u = DSD(rec + 0x24);
    CHECK_EQ_INT((int)fu.f, 7);
}

int test_anim(void)
{
    int before = g_failures;
    if (res_count() != 69)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") == 69,
              "index loaded");
    CHECK(DSD(DS_001014F4) != 0, "actor pool allocated");

    check_read_write_var();
    check_next_sprite_id();
    check_real_stream();
    check_walk();
    check_entry_helpers();
    check_opcode8_pin();
    return g_failures - before;
}
