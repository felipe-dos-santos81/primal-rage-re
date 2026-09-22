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
    /* 0x7F is out of every selector range; the switch falls through without a
     * store. The ring entry it would collide with if mishandled is checked
     * unchanged. */
    DSW(DS_00105B4C + 0x0f * 2u) = 0x1234;
    anim_write_var(rec, 0x7f, 0xffff);
    CHECK_EQ_INT((int)DSW(DS_00105B4C + 0x0f * 2u), 0x1234);
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

/* 0x12720: the animation opcode 0x11 target the globe's first presentation
 * stream reaches. The word at 0xE89A8 is 0xD100 — opcode 0x11, mode 0x4000 —
 * so anim_operand loads the code pointer 0x12720 into DS_00105BD4 and the
 * dispatcher's indirect call reaches it. It spawns the globe's fourth layer as
 * a child of DS_000F0A58: descriptor 0x9AC80 (stream 0x0E89F6, frame hold 7,
 * layer 0xE2). Driven from the real descriptor 0x9AC44 with the cursor at the
 * op-0x11 word, so the walk's own operand decode is what supplies the target.
 * The spawned record is identified as the one the active list did not hold
 * before the call: with the target skipped the list is unchanged and the
 * assertions below cannot pass on a record the test itself left behind. */
static void check_globe_opcode11_spawn(void)
{
    actors_reset();
    DSB(DS_00104B24) = 0;
    DSB(DS_00104B26) = 0;
    u32 parent = actor_spawn((const u32 *)(mem + 0x9AC44u),
                             0x2A00u, 0xE0u, 0x5A00u, 0u);
    CHECK(parent != 0, "globe first-layer record");
    if (parent == 0) return;
    u32 saved_a58 = DSD(DS_000F0A58);
    DSW(parent + 0x56) = 7;             /* a non-zero slot: the child's +0x4A is the parent's slot */
    DSD(DS_000F0A58) = parent;          /* 0x12720 reads this global */
    DSD(parent + 8) = 0x0E89A6u;        /* the walk reads the op-0x11 word at 0xE89A8 */
    DSD(parent + 0x20) = 0;             /* expired frame timer */
    DSD(parent + 0x24) = 0x3f800000u;   /* 1.0f: frame_timer runs */
    DSW(parent + 0x2a) = 0;
    DSW(parent + 0x28) &= (u16)~0x0810u;

    u32 before[64];
    int nbefore = 0;
    for (u32 r = actor_list_head(); r != 0 && nbefore < 64; r = actor_next(r))
        before[nbefore++] = r;

    actor_sync(parent);

    /* The record the walk spawned is the one the active list did not hold. */
    u32 child = 0;
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) {
        int seen = 0;
        for (int i = 0; i < nbefore; i++) if (before[i] == r) { seen = 1; break; }
        if (!seen) child = r;
    }
    CHECK(child != 0, "the opcode-0x11 target spawned the fourth layer");
    if (child != 0) {
        union { float f; u32 u; } fu;
        CHECK_EQ_INT((int)DSB(child + 0x49), 0xE2);            /* layer 0xE2 */
        CHECK_EQ_INT((int)DSB(child + 0x4a), 7);               /* a5 = the parent slot */
        /* The 0x400 in a5 selects the parent-relative spawn: the descriptor's
         * own u16@8 (0x2000) plus the 0x400 parent bit. */
        CHECK_EQ_INT((int)DSW(child + 0x28), 0x2400);
        CHECK_EQ_INT((int)DSW(actor_pset(child)), 0x0235);     /* stream 0x0E89F6's first id */
        fu.u = DSD(child + 0x24);
        CHECK_EQ_INT((int)fu.f, 7);                            /* descriptor hold 7 */
        fu.u = DSD(child + 0x20);
        CHECK_EQ_INT((int)fu.f, 6);                            /* 7 - 1 */
    }
    DSD(DS_000F0A58) = saved_a58;
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
    actors_anim_begin(rec, ANIM_SCRATCH, 0x40e00000u);   /* IEEE-754 7.0f bits */
    CHECK_EQ_INT((int)DSW(pset), 0x2c22);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)ANIM_SCRATCH);
    CHECK_EQ_INT((int)DSB(rec + 0x50), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x61), 0);
    CHECK_EQ_INT((int)(DSB(rec + 0x2b) & 0x04), 0);
    /* 0x2BC8B stores the raw dword argument, not a converted integer. */
    CHECK_EQ_INT((int)DSD(rec + 0x20), 0x40e00000);
    CHECK_EQ_INT((int)DSD(rec + 0x24), 0x40e00000);
}

/* The dispatcher driven through a stream (0x2BC30's pre-walk, EBX=0), covering
 * the opcodes the title streams actually reach: 0x12, 0x18 (both branches),
 * 0x00 and 0x01. The 0xCD40/0x8D00 cases above exercise 0x2A408, not the
 * dispatcher. */
static void check_dispatcher_streams(void)
{
    u16 *s = (u16 *)(mem + ANIM_SCRATCH);

    /* 0x12: operand 0x14 -> rec+0x2E = 0x140, rec+0x4E = 1, then literal id. */
    u32 rec = anim_alloc_record();
    CHECK(rec != 0, "dispatch 0x12 record");
    if (rec != 0) {
        DSW(rec + 0x56) = 0;
        DSW(rec + 0x2e) = 0;
        s[0] = 0x9214; s[1] = 0x2c11;
        actors_anim_begin(rec, ANIM_SCRATCH, 0x40e00000u);
        CHECK_EQ_INT((int)DSW(rec + 0x2e), 0x0140);
        CHECK_EQ_INT((int)DSB(rec + 0x4e), 1);
        CHECK_EQ_INT((int)DSW(actor_pset(rec)), 0x2c11);
    }

    /* 0x18 fall-through: bound 0, so the counter (rec+0x52) increments once and
     * the cursor advances to the literal id. */
    rec = anim_alloc_record();
    CHECK(rec != 0, "dispatch 0x18 fallthrough record");
    if (rec != 0) {
        DSW(rec + 0x56) = 0;
        DSB(rec + 0x52) = 0;
        s[0] = 0xb840; s[1] = 0x0000; s[2] = 0x0000; s[3] = 0x0000;
        s[4] = 0x2c11;                         /* cursor+8 after fall-through */
        actors_anim_begin(rec, ANIM_SCRATCH, 0x40e00000u);
        CHECK_EQ_INT((int)DSB(rec + 0x52), 1);
        CHECK_EQ_INT((int)DSW(actor_pset(rec)), 0x2c11);
    }

    /* 0x18 jump: bound 3 and the table dword points back at the stream, so the
     * counter counts up to the bound before falling through. */
    rec = anim_alloc_record();
    CHECK(rec != 0, "dispatch 0x18 jump record");
    if (rec != 0) {
        DSW(rec + 0x56) = 0;
        DSB(rec + 0x52) = 0;
        s[0] = 0xb840; s[1] = 0x0003;
        *(u32 *)(mem + ANIM_SCRATCH + 4) = ANIM_SCRATCH;
        s[4] = 0x2c11;                         /* cursor+8 after fall-through */
        actors_anim_begin(rec, ANIM_SCRATCH, 0x40e00000u);
        CHECK_EQ_INT((int)DSB(rec + 0x52), 3);
        CHECK_EQ_INT((int)DSW(actor_pset(rec)), 0x2c11);
    }

    /* 0x00: set_dead (rec+0x28 0x08) + frame reset, returns 2. */
    rec = anim_alloc_record();
    CHECK(rec != 0, "dispatch 0x00 record");
    if (rec != 0) {
        DSW(rec + 0x56) = 0;
        DSB(rec + 0x28) &= (u8)~0x08u;
        s[0] = 0x8000; s[1] = 0x2c11;
        actors_anim_begin(rec, ANIM_SCRATCH, 0x40e00000u);
        CHECK_EQ_INT((int)(DSB(rec + 0x28) & 0x08), 0x08);
        CHECK_EQ_INT((int)DSD(rec + 0x24), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x20), 0);
    }

    /* 0x01: frame reset, returns 2. */
    rec = anim_alloc_record();
    CHECK(rec != 0, "dispatch 0x01 record");
    if (rec != 0) {
        DSW(rec + 0x56) = 0;
        s[0] = 0x8100; s[1] = 0x2c11;
        actors_anim_begin(rec, ANIM_SCRATCH, 0x40e00000u);
        CHECK_EQ_INT((int)DSD(rec + 0x24), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x20), 0);
    }
}

/* 0x37A58: the fighters' idle-animation tick, an opcode-0x10 target. The
 * character streams (the T-rex 0xE6DD2, the raptor 0xD2136) carry the word
 * 0xD000 (opcode 0x10, mode 0x4000), so anim_operand loads the dword 0x37A58
 * into DS_00105BD4 and the dispatcher's indirect call reaches it. It advances
 * rec+0x52 — the offset the 0xCD40 form selects the sprite id with — and on
 * rec+0x4C's expiry draws rng(2), flips rec+0x58 between +1 and 0xFF and
 * reseeds rec+0x4C to 3 * (rec+0x4D / 3). Before it was registered anim_indirect
 * returned NULL and the variable froze, so the idle animation held its first
 * sprite. */
static void check_idle_tick_37a58(void)
{
    u16 *s = (u16 *)(mem + ANIM_SCRATCH);
    u32 rec = anim_alloc_record();
    CHECK(rec != 0, "idle-tick record");
    if (rec == 0) return;
    DSW(rec + 0x56) = 0;
    DSW(DS_00104B00) = 3;                /* not 6: the rng(3) arm is skipped */
    s[0] = 0x2c10;
    s[1] = 0xd000;                       /* opcode 0x10, mode 0x4000 */
    *(u32 *)(mem + ANIM_SCRATCH + 4) = 0x37a58u;
    s[4] = 0x2c11;                       /* the literal id after the command */
    DSW(rec + 0x28) = 0;
    DSW(rec + 0x2a) = 0;
    DSB(rec + 0x4b) = 0;
    DSB(rec + 0x4f) = 0;
    DSB(rec + 0x51) = 0;
    DSB(rec + 0x4d) = 0x1e;

    /* Tick A: the countdown has not expired. The variable advances by rec+0x58
     * and no rng is drawn. */
    DSB(rec + 0x4c) = 5;
    DSB(rec + 0x52) = 0x10;              /* seeded sentinel, not the post-state */
    DSB(rec + 0x58) = 1;
    DSD(rec + 8) = ANIM_SCRATCH;
    DSD(rec + 0x20) = 0;                 /* expired frame timer */
    DSD(rec + 0x24) = 0x3f800000u;       /* 1.0f: frame_timer runs */
    rng_seed(0xabcd);
    u32 lcg0 = DSD(DS_000EF6D8);
    actor_sync(rec);
    CHECK_EQ_INT((int)DSB(rec + 0x52), 0x11);        /* 0x10 + 1 */
    CHECK_EQ_INT((int)DSB(rec + 0x4c), 4);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)lcg0);  /* no rng draw */

    /* Tick B: the countdown expires. rng(2) flips rec+0x58 and rec+0x4C is
     * reseeded to 3 * (rec+0x4D / 3). The expected flip is the same draw the
     * callback makes, so seed and draw it here. */
    rng_seed(0xabcd);
    u32 draw = rng_next(2u);
    DSB(rec + 0x4c) = 0;
    DSB(rec + 0x52) = 5;
    DSB(rec + 0x58) = 1;
    DSD(rec + 8) = ANIM_SCRATCH;
    DSD(rec + 0x20) = 0;
    DSD(rec + 0x24) = 0x3f800000u;
    DSW(rec + 0x28) = 0;
    DSW(rec + 0x2a) = 0;
    rng_seed(0xabcd);
    u32 lcg1 = DSD(DS_000EF6D8);
    actor_sync(rec);
    CHECK_EQ_INT((int)DSB(rec + 0x4c), 0x1e);        /* 3 * (0x1e / 3) */
    CHECK(DSD(DS_000EF6D8) != lcg1, "the expiry drew rng(2)");
    CHECK_EQ_INT((int)DSB(rec + 0x58), draw != 0u ? 1 : 0xff);
    CHECK_EQ_INT((int)DSB(rec + 0x52), draw != 0u ? 6 : 4);
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
    check_dispatcher_streams();
    check_opcode8_pin();
    check_globe_opcode11_spawn();
    check_idle_tick_37a58();
    return g_failures - before;
}
