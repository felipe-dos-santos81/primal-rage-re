/* port/tests/diff_runner.c — port side of the differential harness (spec
 * 2026-09-30-reverse-completion-design §5). A tool binary (`diffrun`), not a unit test: it loads
 * PRAGE.EXE's LE image into mem[], then for each case in --cases applies the registers and pokes,
 * calls the named C function through its binding, and prints the output register and every byte
 * of mem[] that changed. tools/diff_verify.py compares that with the original's own bytes run by
 * tools/diff_emu.py from the same image. */
#include "mem.h"
#include "symbols.h"
#include "game/actors.h"
#include "game/attract.h"
#include "game/effects.h"
#include "game/config.h"
#include "game/fighter.h"
#include "game/flow.h"
#include "game/rng.h"
#include "game/svcmenu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* s0..s3 are the dwords above the return address: a callee's stack arguments (record E3 §E3.5). */
enum { R_EAX, R_EBX, R_ECX, R_EDX, R_ESI, R_EDI, R_EBP, R_S0, R_S1, R_S2, R_S3, R_N };
static const char *const k_reg[R_N] = { "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp",
                                        "s0", "s1", "s2", "s3" };

/* A binding adapts the original's register calling convention (Watcom: EAX, EDX, EBX, ECX) to
 * the port function's C signature. eax_mask is the part of EAX the original's callers read: a
 * function that returns through AL leaves the upper bits of EAX as scratch, which the port's C
 * return value does not reproduce, so comparing them would flag a difference no caller can see. */
typedef struct {
    const char *name;
    void (*call)(const u32 in[R_N], u32 *eax);
    u32 eax_mask;
} binding_t;

static void b_rng_next(const u32 *r, u32 *eax)         { *eax = rng_next(r[R_EAX]); }
static void b_slot_flag(const u32 *r, u32 *eax)        { *eax = (u32)fighter_slot_flag(r[R_EAX]); }
static void b_credit_spend(const u32 *r, u32 *eax)     { *eax = config_credit_spend(r[R_EAX]); }
static void b_codeword_len(const u32 *r, u32 *eax)     { *eax = config_codeword_len(r[R_EAX]); }
/* 0x3640C and 0x37DCC (record gameplay-u6 §U6.4/§U6.5): animation-opcode targets with no C return
 * value. Their callers, the three `call [0x105BD4]` sites of 0x2B2A0 (0x2B56D, 0x2B594, 0x2B5EA),
 * each overwrite EAX at once (`mov eax,ecx` at 0x2B575, 0x2B59A, 0x2B5F0), so no caller reads it:
 * the binding mask is 0 and the comparison is the changed bytes. */
static void b_3640c(const u32 *r, u32 *eax)            { fighter_3640c(r[R_EAX]); *eax = 0u; }
static void b_37dcc(const u32 *r, u32 *eax)            { (void)r; fighter_37dcc(); *eax = 0u; }
/* E3 (record 2026-10-01-reverse-e3 §E3.6). The reaction callbacks take EAX = slot, EDX = rec,
 * EBX = side (0x34E2C's call at 0x35045) and return AL = 1; the animation-opcode targets are
 * reached through fn_resolve as (rec, arg) = (EAX, EDX), as anim_indirect calls them. The two
 * context builders write six dwords at EAX (a stack buffer in every original caller); the binding
 * hands the C function a local array and copies it to mem[EAX], the original's own store. */
typedef void (*anim_code_fn)(u32 rec, u32 arg);
static void b_23130(const u32 *r, u32 *eax)            { *eax = (u32)fighter_23130(r[R_EAX], r[R_EDX], r[R_EBX]); }
static void b_45878(const u32 *r, u32 *eax)            { fighter_45878(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_anim(u32 addr, const u32 *r, u32 *eax)
{
    anim_code_fn fn = (anim_code_fn)(void *)fn_resolve(addr);
    if (fn) fn(r[R_EAX], r[R_EDX]);
    *eax = 0u;
}
static void b_10fa8(const u32 *r, u32 *eax)            { b_anim(0x10FA8u, r, eax); }
static void b_3e4e4(const u32 *r, u32 *eax)            { b_anim(0x3E4E4u, r, eax); }
static void b_ctx_same(const u32 *r, u32 *eax)
{
    u32 out[6];
    fighter_ctx_same(out, r[R_EDX]);
    memcpy(mem + r[R_EAX], out, sizeof out);
    *eax = 0u;
}
static void b_anim_ctx(const u32 *r, u32 *eax)
{
    u32 out[6];
    hit_anim_ctx(out, r[R_EDX]);
    memcpy(mem + r[R_EAX], out, sizeof out);
    *eax = 0u;
}
static void b_3c4cc(const u32 *r, u32 *eax)            { hit_anim_start_b(r[R_EAX], r[R_EDX], r[R_S0]); *eax = 0u; }
/* A probe of the port's code-pointer table: EAX = an original address, the result 1 when the port
 * registered it and 0 when not (fn_resolve_from then reports the miss to the hook, as in a spec's
 * indirect call). Nothing is called. Tested over every fn_register site (test_diff_verify.py). */
static void b_fn_resolved(const u32 *r, u32 *eax)      { *eax = fn_resolve(r[R_EAX]) != NULL ? 1u : 0u; }

/* Self-check mutants. Each is a plausible porting bug, kept only so tools/diff_verify.py
 * --self-check can prove the harness reports a difference (an assertion that cannot fail proves
 * nothing, AGENTS.md). They are bound under "<name>@mutant" (or another "@" suffix) and live in
 * this tool, never in prage_core. */
static void m_rng_next(const u32 *r, u32 *eax)         /* increment off by one */
{
    DSD(DS_000EF6D8) = DSD(DS_000EF6D8) * 0xB90D12B9u + 0x38CE0520u;
    *eax = (DSD(DS_000EF6D8) >> 16) * (r[R_EAX] & 0xFFFFu) >> 16;
}
static void m_slot_flag(const u32 *r, u32 *eax)        /* forgets to set the bit */
{
    u32 m = 1u << (r[R_EAX] & 0xffu);
    *eax = (DSD(DS_00107EE0) & m) != 0 ? 1u : 0u;
}
static void m_credit_spend(const u32 *r, u32 *eax)     /* `>=` for the unsigned guard */
{
    if (DSB(DS_00105D60) != 0u) { *eax = 1u; return; }
    if (r[R_EAX] >= DSD(DS_00105C00)) { *eax = 0u; return; }
    if (DSB(DS_00104B1F) == 0u) DSD(DS_00105C00) -= r[R_EAX];
    *eax = 1u;
}
static void m_credit_spend_signed(const u32 *r, u32 *eax)  /* signed compare for the unsigned `ja` */
{
    if (DSB(DS_00105D60) != 0u) { *eax = 1u; return; }
    if ((s32)r[R_EAX] > (s32)DSD(DS_00105C00)) { *eax = 0u; return; }
    if (DSB(DS_00104B1F) == 0u) DSD(DS_00105C00) -= r[R_EAX];
    *eax = 1u;
}
static void m_codeword_len(const u32 *r, u32 *eax)     /* `>` for the signed `jge` */
{
    u32 e = r[R_EAX], d = 1u;
    for (;;) {
        e++;
        if ((s32)d > (s32)e) break;
        d += d;
    }
    *eax = e;
}

static void m_3640c(const u32 *r, u32 *eax)            /* forgets the owner slot's +0x52 */
{
    DSB(r[R_EAX] + 0x52u) = 0u;
    DSD(r[R_EAX] + 0x24u) = 0x40400000u;
    DSB(r[R_EAX] + 0x4Du) = 0x14u;
    *eax = 0u;
}
static void m_37dcc(const u32 *r, u32 *eax)            /* stores 2 */
{
    (void)r;
    DSB(DS_001078FC) = 2u;
    *eax = 0u;
}

static void m_23130_voice(const u32 *r, u32 *eax)     /* the wrong voice id */
{
    u32 ctx[6];
    fighter_ctx_same(ctx, r[R_EBX]);
    DSB(r[R_EAX] + 0x52u) = 9u;
    DSB(r[R_EAX] + 0x53u) = 7u;
    DSB(r[R_EAX] + 0x54u) = 0u;
    DSD(r[R_EAX] + 0x0Cu) = 0u;
    hit_anim_start_b(r[R_EDX], 0x000E4872u, 0x40400000u);
    (void)sound_voice(0x7Du);
    *eax = 1u;
}
static void m_23130_novoice(const u32 *r, u32 *eax)   /* forgets the voice call */
{
    u32 ctx[6];
    fighter_ctx_same(ctx, r[R_EBX]);
    DSB(r[R_EAX] + 0x52u) = 9u;
    DSB(r[R_EAX] + 0x53u) = 7u;
    DSB(r[R_EAX] + 0x54u) = 0u;
    DSD(r[R_EAX] + 0x0Cu) = 0u;
    hit_anim_start_b(r[R_EDX], 0x000E4872u, 0x40400000u);
    *eax = 1u;
}
static void m_23130_reorder(const u32 *r, u32 *eax)   /* the slot stores after 0x3C4CC, not before */
{
    u32 ctx[6];
    fighter_ctx_same(ctx, r[R_EBX]);
    hit_anim_start_b(r[R_EDX], 0x000E4872u, 0x40400000u);
    DSB(r[R_EAX] + 0x52u) = 9u;
    DSB(r[R_EAX] + 0x53u) = 7u;
    DSB(r[R_EAX] + 0x54u) = 0u;
    DSD(r[R_EAX] + 0x0Cu) = 0u;
    (void)sound_voice(0x7Cu);
    *eax = 1u;
}
static void m_45878(const u32 *r, u32 *eax)            /* the 0x2BC30 frame at 2.0, not 3.0 */
{
    actors_anim_begin(r[R_EDX], 0x000EB58Cu, 0x40000000u);
    DSB(r[R_EDX] + 0x53u) = 1u;
    DSB(r[R_EDX] + 0x59u) = 1u;
    DSB(r[R_EAX] + 0x52u) = 9u;
    DSB(r[R_EAX] + 0x53u) = 7u;
    DSB(r[R_EAX] + 0x54u) = 1u;
    DSB(r[R_EAX] + 0x57u) = 0u;
    DSD(r[R_EAX] + 0x0Cu) = 0x0004579Cu;
    DSD(r[R_EAX] + 0x18u) = 0x00045640u;
    DSD(r[R_EAX] + 0x1Cu) = 0x000456A8u;
    DSW(r[R_EAX] + 0x88u) = 0u;
    DSB(r[R_EAX] + 0x42u) |= 4u;
    *eax = 0u;
}
static void m_10fa8(const u32 *r, u32 *eax)            /* layer 0xE5 */
{
    (void)r;
    actor_spawn((const u32 *)(mem + 0x9AD08u), 0, 0xE5, 0, 0);
    *eax = 0u;
}
static void m_3e4e4(const u32 *r, u32 *eax)            /* skips the +0x14 test */
{
    u32 slot = DSD(r[R_EAX] + 0x14u);
    actors_anim_begin(r[R_EAX], 0x000E7BFAu, 0x40400000u);
    DSW(r[R_EAX] + 0x36u) = 0x320u;
    DSW(r[R_EAX] + 0x44u) = 0x23u;
    if (slot) DSB(slot + 0x57u) = 0u;
    *eax = 0u;
}
static void m_ctx_same(const u32 *r, u32 *eax)         /* out[1] = side */
{
    u32 out[6];
    fighter_ctx_same(out, r[R_EDX]);
    out[1] = r[R_EDX];
    memcpy(mem + r[R_EAX], out, sizeof out);
    *eax = 0u;
}
static void m_anim_ctx(const u32 *r, u32 *eax)         /* the side from +0x50 */
{
    u32 out[6];
    u32 s = DSB(r[R_EDX] + 0x50u);
    out[0] = s;
    out[1] = 1u - s;
    out[2] = DS_001077B0 + s * 0x94u;
    out[3] = DS_001077B0 + (1u - s) * 0x94u;
    out[4] = DSD(out[2]);
    out[5] = DSD(out[3]);
    memcpy(mem + r[R_EAX], out, sizeof out);
    *eax = 0u;
}
static void m_3c4cc(const u32 *r, u32 *eax)            /* forgets the 0x3C480 arm */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], r[R_EDX], r[R_S0]);
    *eax = 0u;
}

static void m_3c4cc_set(const u32 *r, u32 *eax)        /* state 0 falls out of the 0x2BC30 set */
{
    /* hit_anim_start_a is static to fighter.c: state 0 is sent to the 0x3C480 arm by running the
     * real function with the slot's state byte set to a state outside the set, then restoring it */
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    u32 at = ctx[2] + 0x52u;
    u8 st = DSB(at);
    if (st == 0u) DSB(at) = 3u;
    hit_anim_start_b(r[R_EAX], r[R_EDX], r[R_S0]);
    DSB(at) = st;
    *eax = 0u;
}

/* Track P batch 1 (record 2026-10-02-reverse-p1 §P1.5): the finisher entries as 0x379C4 calls them
 * at 0x379E8 (EAX = slot, EDX = rec). Mask 0xFF: the raw sets AL = 1 over its last callee's EAX and
 * the only caller tests EAX != 0 (0x379EE), which AL = 1 settles (record §P1.4). */
static void b_1567c(const u32 *r, u32 *eax)            { *eax = (u32)fighter_1567c(r[R_EAX], r[R_EDX]); }
static void b_15908(const u32 *r, u32 *eax)            { *eax = (u32)fighter_15908(r[R_EAX], r[R_EDX]); }
static void b_23ec0(const u32 *r, u32 *eax)            { *eax = (u32)fighter_23ec0(r[R_EAX], r[R_EDX]); }
static void b_45d14(const u32 *r, u32 *eax)            { *eax = (u32)fighter_45d14(r[R_EAX], r[R_EDX]); }
static void m_1567c(const u32 *r, u32 *eax)            /* 0x15908's stream 0xD3334 */
{
    u32 slot = r[R_EAX];
    actors_anim_begin(r[R_EDX], 0x000D3334u, 0x40400000u);
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0x00015584u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    DSB(slot + 0x42u) = (u8)(DSB(slot + 0x42u) | 8u);
    DSD(slot + 0x14u) = 0u;
    (void)sound_voice(0xAFu);
    *eax = 1u;
}
static void m_15908(const u32 *r, u32 *eax)            /* the voice 0xAA (0x23EC0's) */
{
    u32 slot = r[R_EAX];
    actors_anim_begin(r[R_EDX], 0x000D3334u, 0x40400000u);
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0x0001579Cu;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    DSB(slot + 0x42u) = (u8)(DSB(slot + 0x42u) | 8u);
    DSD(slot + 0x14u) = 0u;
    (void)sound_voice(0xAAu);
    *eax = 1u;
}
static void m_23ec0(const u32 *r, u32 *eax)            /* the slot stores after the voice, not before */
{
    u32 slot = r[R_EAX];
    actors_anim_begin(r[R_EDX], 0x000E1B24u, 0x40400000u);
    (void)sound_voice(0xAAu);
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0x00023D38u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    DSD(slot + 0x14u) = 0u;
    *eax = 1u;
}
static void m_45d14(const u32 *r, u32 *eax)            /* the frame 2.0, not 3.0 */
{
    u32 slot = r[R_EAX];
    actors_anim_begin(r[R_EDX], 0x000EB876u, 0x40000000u);
    DSB(slot + 0x53u) = 3u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    DSD(slot + 0x14u) = 0u;
    *eax = 1u;
}

/* Track P batch 1 (record 2026-10-02-reverse-p1 §P1.4/§P1.6): the callee 0x1A570 (EAX = side; mask 0xFF,
 * all 69 direct callers read AL), 0x23BF8 (mask 0xFFFFFFFF: its early return's EAX is a word with AL
 * cleared, which 0x379EE tests whole) and 0x402FC (mask 0xFF, as the other entries). */
static void b_1a570(const u32 *r, u32 *eax)            { *eax = (u32)fighter_actor_bit15_clear(r[R_EAX]); }
static void b_23bf8(const u32 *r, u32 *eax)            { *eax = fighter_23bf8(r[R_EAX], r[R_EDX]); }
static void b_402fc(const u32 *r, u32 *eax)            { *eax = (u32)fighter_402fc(r[R_EAX], r[R_EDX]); }
static void m_1a570(const u32 *r, u32 *eax)            /* tests bit 14, not 15 */
{
    u32 rec = DSD(DS_001077B0 + r[R_EAX] * 0x94u);
    u32 actor = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
    *eax = (DSW(actor) & 0x4000u) == 0 ? 1u : 0u;
}
static void m_23bf8(const u32 *r, u32 *eax)            /* the two compares swapped */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u32 w = DSW(DS_00104AFC), stream = 0x000E1A06u;
    if (DSB(0x000A83C4u + w) == 0u) { *eax = w & 0xFF00u; return; }
    if (fighter_actor_bit15_clear(DSB(rec + 0x51u))) {
        if ((s32)DSD(rec + 0x18u) > (s32)DSD(0x000A83CCu + w * 4u)) stream = 0x000E19E6u;
    } else if ((s32)DSD(rec + 0x18u) < (s32)DSD(0x000A83CCu + w * 4u)) {
        stream = 0x000E19E6u;
    }
    actors_anim_begin(rec, stream, 0x40400000u);
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0x00023B68u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    DSD(slot + 0x14u) = 0u;
    DSB(slot + 0x42u) = (u8)(DSB(slot + 0x42u) | 8u);
    *eax = 1u;
}
static void m_23bf8_zero(const u32 *r, u32 *eax)       /* the early return as 0, not the word with AL cleared */
{
    if (DSB(0x000A83C4u + DSW(DS_00104AFC)) == 0u) { *eax = 0u; return; }
    *eax = fighter_23bf8(r[R_EAX], r[R_EDX]);
}
static void m_23bf8_ne(const u32 *r, u32 *eax)         /* the AL-set compare as x != threshold */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u32 w = DSW(DS_00104AFC), stream = 0x000E1A06u;
    if (DSB(0x000A83C4u + w) == 0u) { *eax = w & 0xFF00u; return; }
    if (fighter_actor_bit15_clear(DSB(rec + 0x51u))) {
        if ((s32)DSD(rec + 0x18u) != (s32)DSD(0x000A83CCu + w * 4u)) stream = 0x000E19E6u;
    } else if ((s32)DSD(rec + 0x18u) > (s32)DSD(0x000A83CCu + w * 4u)) {
        stream = 0x000E19E6u;
    }
    actors_anim_begin(rec, stream, 0x40400000u);
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0x00023B68u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    DSD(slot + 0x14u) = 0u;
    DSB(slot + 0x42u) = (u8)(DSB(slot + 0x42u) | 8u);
    *eax = 1u;
}
static void m_402fc(const u32 *r, u32 *eax)            /* the word store after the 0x3C4CC call */
{
    u32 ctx[6], slot = r[R_EAX], rec = r[R_EDX];
    hit_anim_ctx(ctx, rec);
    hit_anim_start_b(rec, 0x000E7C40u, 0x40400000u);
    DSW(0x001080A0u + ctx[0] * 2u) = 0u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x54u) = 2u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x0Cu) = 0x000401D4u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    *eax = 1u;
}

/* Track P batch 1 (record 2026-10-02-reverse-p1 §P1.8): the slot +0x0C callbacks as 0x3531C case 7 calls
 * them at 0x35431 (EAX = slot, EDX = rec, EBX = side). Mask 0: 0x35434 `xor eax,eax` overwrites EAX, and
 * 0x38434's call at 0x384D9 returns it to 0x3856B, which loads EAX at once (`mov eax,esi`). */
static void b_15584(const u32 *r, u32 *eax)            { fighter_15584(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_1579c(const u32 *r, u32 *eax)            { fighter_1579c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_23d38(const u32 *r, u32 *eax)            { fighter_23d38(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_15584(const u32 *r, u32 *eax)            /* case 1's voice by the slot's own character */
{
    u32 ctx[6], slot = r[R_EAX];
    fighter_ctx_same(ctx, r[R_EBX]);
    if (DSB(slot + 0x57u) == 1u) {
        actors_anim_begin(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40000000u);
        DSB(ctx[3] + 0x42u) = (u8)(DSB(ctx[3] + 0x42u) | 4u);
        (void)sound_voice(DSW(0x000C75AAu + (u32)DSB(ctx[2] + 0x7Au) * 2u));
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
    } else {
        fighter_15584(slot, r[R_EDX], r[R_EBX]);
    }
    *eax = 0u;
}
static void m_1579c(const u32 *r, u32 *eax)            /* case 3's palette handle 0x1F874610 (0x45B50's) */
{
    u32 ctx[6], slot = r[R_EAX];
    fighter_ctx_same(ctx, r[R_EBX]);
    if (DSB(slot + 0x57u) == 3u
            && (u16)(DSW(ctx[5] + 0x2Cu) - 0x40u) <= 0x400u) {
        DSW(ctx[5] + 0x2Cu) = (u16)(DSW(ctx[5] + 0x2Cu) - 0x40u);
        if ((DSW(ctx[5] + 0x28u) & 0x4000u) != 0u) {
            DSB(ctx[5] + 0x29u) = (u8)(DSB(ctx[5] + 0x29u) & 0xBFu);
            DSW(ctx[5] + 0x34u) = 0x0080u;
        } else {
            DSB(ctx[5] + 0x29u) = (u8)(DSB(ctx[5] + 0x29u) | 0x40u);
            DSW(ctx[5] + 0x34u) = 0xFF80u;
        }
        actors_anim_begin(ctx[5], 0x000E8C82u, 0x40400000u);
        actor_pset_palette(ctx[5], 0x18u, 0x1F874610u);
        DSW(ctx[5] + 0x2Cu) = 0x1000u;
        DSB(ctx[5] + 0x28u) = (u8)(DSB(ctx[5] + 0x28u) | 0x80u);
        (void)sound_voice(0xEAu);
        DSB(slot + 0x53u) = 3u;
        DSB(slot + 0x52u) = 9u;
        DSB(DS_000F0AFE) = 4u;
        DSB(DS_001078FC) = 1u;
    } else {
        fighter_1579c(slot, r[R_EDX], r[R_EBX]);
    }
    *eax = 0u;
}
static void m_23d38(const u32 *r, u32 *eax)            /* case 3 starts the held record first */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], o = DSD(slot + 8u);
    u32 dd = DSD(rec + 0x18u) - DSD(o + 0x18u);
    if ((s32)dd < 0) dd = 0u - dd;
    if (DSB(slot + 0x57u) == 3u && (s32)dd <= 0xB00) {
        DSD(o + 0x18u) = DSD(rec + 0x18u);
        DSD(o + 0x1Cu) = DSD(rec + 0x1Cu);
        DSW(o + 0x34u) = 0u;
        DSW(o + 0x2Cu) = 0x0E00u;
        if ((DSW(rec + 0x28u) & 0x4000u) != 0u) DSB(o + 0x29u) = (u8)(DSB(o + 0x29u) | 0x40u);
        else DSB(o + 0x29u) = (u8)(DSB(o + 0x29u) & 0xBFu);
        actors_anim_begin(o, 0x000E1C0Cu, 0x40400000u);
        actors_anim_begin(rec, 0x000E1BD2u, 0x40400000u);
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
        (void)sound_voice(0xD6u);
    } else {
        fighter_23d38(slot, rec, r[R_EBX]);
    }
    *eax = 0u;
}

/* Task 4 review fold-in: the boundaries and signedness only the cases gE..gJ, 0x15584's c9 and 0x1579C's c7
 * reach. */
static void m_23d38_ge(const u32 *r, u32 *eax)         /* case 0: `jg`/`jl` where the raw has `jge`/`jle` */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    if (DSB(slot + 0x57u) == 0u) {
        if ((DSW(rec + 0x28u) & 0x4000u) != 0u) {
            if ((s32)(DSD(DS_000F0AF0) + 0x3000u) > (s32)DSD(rec + 0x18u)) { *eax = 0u; return; }
            DSD(rec + 0x18u) = DSD(DS_000F0AF0) - 0x3000u;
        } else {
            if ((s32)(DSD(DS_000F0AF0) - 0x3000u) < (s32)DSD(rec + 0x18u)) { *eax = 0u; return; }
            DSD(rec + 0x18u) = DSD(DS_000F0AF0) + 0x3000u;
        }
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
    } else {
        fighter_23d38(slot, rec, r[R_EBX]);
    }
    *eax = 0u;
}
static void m_23d38_unsigned(const u32 *r, u32 *eax)   /* cases 0 and 1 compared unsigned */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], st = DSB(slot + 0x57u);
    if (st == 0u) {
        if ((DSW(rec + 0x28u) & 0x4000u) != 0u) {
            if (DSD(DS_000F0AF0) + 0x3000u >= DSD(rec + 0x18u)) { *eax = 0u; return; }
            DSD(rec + 0x18u) = DSD(DS_000F0AF0) - 0x3000u;
        } else {
            if (DSD(DS_000F0AF0) - 0x3000u <= DSD(rec + 0x18u)) { *eax = 0u; return; }
            DSD(rec + 0x18u) = DSD(DS_000F0AF0) + 0x3000u;
        }
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
    } else if (st == 1u) {
        u32 d = DSD(DS_000F0AF0) - DSD(rec + 0x18u);
        if ((s32)d < 0) d = 0u - d;
        if (d > 0x2000u) { *eax = 0u; return; }
        actors_anim_begin(rec, 0x000E1BAEu, 0x40400000u);
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
    } else {
        fighter_23d38(slot, rec, r[R_EBX]);
    }
    *eax = 0u;
}
static void m_23d38_bit(const u32 *r, u32 *eax)        /* case 0 tests the whole word +0x28, not bit 14 */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    if (DSB(slot + 0x57u) == 0u) {
        if (DSW(rec + 0x28u) != 0u) {
            if ((s32)(DSD(DS_000F0AF0) + 0x3000u) >= (s32)DSD(rec + 0x18u)) { *eax = 0u; return; }
            DSD(rec + 0x18u) = DSD(DS_000F0AF0) - 0x3000u;
        } else {
            if ((s32)(DSD(DS_000F0AF0) - 0x3000u) <= (s32)DSD(rec + 0x18u)) { *eax = 0u; return; }
            DSD(rec + 0x18u) = DSD(DS_000F0AF0) + 0x3000u;
        }
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
    } else {
        fighter_23d38(slot, rec, r[R_EBX]);
    }
    *eax = 0u;
}
static void m_15584_ge(const u32 *r, u32 *eax)         /* case 3 clamps below 0x400 only (`jge` for `jg`) */
{
    u32 ctx[6], slot = r[R_EAX];
    fighter_ctx_same(ctx, r[R_EBX]);
    if (DSB(slot + 0x57u) == 3u) {
        DSW(ctx[5] + 0x2Cu) = (u16)(DSW(ctx[5] + 0x2Cu) - 0x40u);
        if (DSW(ctx[5] + 0x2Cu) < 0x400u) {
            DSW(ctx[5] + 0x2Cu) = 0x400u;
            DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
        }
        DSW(DSD(ctx[3] + 4u) + 0x2Cu) = DSW(ctx[5] + 0x2Cu);
    } else {
        fighter_15584(slot, r[R_EDX], r[R_EBX]);
    }
    *eax = 0u;
}
static void m_1579c_ge(const u32 *r, u32 *eax)         /* case 3 copies at 0x400 too (`jge` for `jg`) */
{
    u32 ctx[6], slot = r[R_EAX];
    fighter_ctx_same(ctx, r[R_EBX]);
    if (DSB(slot + 0x57u) == 3u && (u16)(DSW(ctx[5] + 0x2Cu) - 0x40u) == 0x400u) {
        DSW(ctx[5] + 0x2Cu) = 0x400u;
        DSW(DSD(ctx[3] + 4u) + 0x2Cu) = 0x400u;
    } else {
        fighter_1579c(slot, r[R_EDX], r[R_EBX]);
    }
    *eax = 0u;
}

/* Track P batch 1 (record 2026-10-02-reverse-p1 §P1.9): 0x23B68 and 0x401D4 as the other +0x0C callbacks
 * (mask 0, §P1.8), and 0x38034 (EAX = side; mask 0: its only caller, 0x402A7, loads EAX at once). */
static void b_23b68(const u32 *r, u32 *eax)            { fighter_23b68(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_401d4(const u32 *r, u32 *eax)            { fighter_401d4(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_38034(const u32 *r, u32 *eax)            { fighter_38034(r[R_EAX]); *eax = 0u; }
static void m_38034(const u32 *r, u32 *eax)            /* the spawn word without bit 10 */
{
    u32 slot = DS_001077B0 + r[R_EAX] * 0x94u, rec = DSD(slot);
    actors_anim_begin(rec, DSD(0x000BDD00u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
    u32 flag = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 0x4000u : 0u;
    u32 sp = actor_spawn((const u32 *)(mem + DSD(0x000BDD1Cu + (u32)DSB(slot + 0x7Au) * 4u)),
                         0u, 0u, 0u, flag | (u32)DSW(rec + 0x56u));
    DSB(sp + 0x59u) = 1u;
    *eax = 0u;
}
static void m_23b68(const u32 *r, u32 *eax)            /* the spawn's x and y swapped */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    if (DSB(slot + 0x57u) == 1u && DSD(rec + 0x1Cu) == 0u) {
        s32 q = (s32)0x400000 / ((s32)DSD(rec + 0x30u) >> 16);
        DSW(rec + 0x2Cu) = (u16)q;
        DSW(DSD(slot + 4u) + 0x2Cu) = (u16)q;
        DSW(rec + 0x38u) = 0u;
        actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
        (void)actor_spawn((const u32 *)(mem + 0x000A83B0u), DSD(rec + 0x1Cu),
                          (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x18u), 0u);
        (void)sound_voice(0x5Cu);
        DSB(slot + 0x53u) = 3u;
        DSB(DS_001078FC) = 1u;
        DSB(slot + 0x52u) = 9u;
        DSB(DS_000F0AFE) = 4u;
    } else {
        fighter_23b68(slot, rec, r[R_EBX]);
    }
    *eax = 0u;
}
static void m_401d4(const u32 *r, u32 *eax)            /* case 3's 0x38034 after the first voice */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    if (DSB(slot + 0x57u) == 3u) {
        u32 ctx[6];
        fighter_ctx_same(ctx, r[R_EBX]);
        (void)sound_voice(0x46u);
        fighter_38034(DSB(DS_001078FD));
        if (DSB(DS_00105B3A) < 2u)
            (void)actor_spawn((const u32 *)(mem + 0x000BB0ECu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
        (void)sound_voice(0x6Cu);
        DSB(slot + 0x53u) = 3u;
        DSB(slot + 0x52u) = 9u;
        DSB(DS_001078FC) = 1u;
    } else {
        fighter_401d4(slot, rec, r[R_EBX]);
    }
    *eax = 0u;
}

static void m_23d38_noneg(const u32 *r, u32 *eax)      /* case 1 without the `neg` at 0x23DC1 */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    if (DSB(slot + 0x57u) == 1u) {
        if ((s32)(DSD(DS_000F0AF0) - DSD(rec + 0x18u)) > 0x2000) { *eax = 0u; return; }
        actors_anim_begin(rec, 0x000E1BAEu, 0x40400000u);
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
    } else {
        fighter_23d38(slot, rec, r[R_EBX]);
    }
    *eax = 0u;
}
static void m_401d4_no36(const u32 *r, u32 *eax)       /* case 1 without the store rec+0x36 = 0 (0x4027E) */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], ctx[6];
    fighter_ctx_same(ctx, r[R_EBX]);
    if (DSB(slot + 0x57u) == 1u) {
        s32 thr = (s32)DSD(0x000BD882u + (u32)DSB(ctx[2] + 0x7Au) * 2u) >> 16;
        if (thr > (s32)DSD(ctx[2] + 0x30u) && (s16)DSW(ctx[4] + 0x36u) < 0) {
            hit_anchor_set(ctx[0], DSD(rec + 0x18u), 0u);
            actors_anim_begin(ctx[4], 0x000E876Au, 0x40400000u);
            DSW(rec + 0x34u) = 0u;
            DSW(rec + 0x44u) = 0u;
            DSB(ctx[2] + 0x54u) = 0u;
            DSB(ctx[2] + 0x57u) = 3u;
        }
    } else {
        fighter_401d4(slot, rec, r[R_EBX]);
    }
    *eax = 0u;
}

/* Final review I1/I2: the stores that were unobservable under the first seeds (slot 1's +0x42 had bit 2
 * set, the held record's +0x29 bit 6 clear) and the two slots' +4 records, now distinct. */
static void m_cb_no42(const u32 *r, void (*fn)(u32, u32, u32))   /* case 1 without `or [ctx[3]+0x42],4` */
{
    u32 ctx[6], slot = r[R_EAX];
    fighter_ctx_same(ctx, r[R_EBX]);
    if (DSB(slot + 0x57u) == 1u) {
        actors_anim_begin(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40000000u);
        (void)sound_voice(DSW(0x000C75AAu + (u32)DSB(ctx[3] + 0x7Au) * 2u));
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
    } else {
        fn(slot, r[R_EDX], r[R_EBX]);
    }
}
static void m_15584_no42(const u32 *r, u32 *eax)       { m_cb_no42(r, fighter_15584); *eax = 0u; }
static void m_1579c_no42(const u32 *r, u32 *eax)       { m_cb_no42(r, fighter_1579c); *eax = 0u; }
static void m_15584_own4(const u32 *r, u32 *eax)       /* case 3 copies to the own slot's +4 record */
{
    u32 ctx[6], slot = r[R_EAX];
    fighter_ctx_same(ctx, r[R_EBX]);
    if (DSB(slot + 0x57u) == 3u) {
        DSW(ctx[5] + 0x2Cu) = (u16)(DSW(ctx[5] + 0x2Cu) - 0x40u);
        if (DSW(ctx[5] + 0x2Cu) <= 0x400u) {
            DSW(ctx[5] + 0x2Cu) = 0x400u;
            DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
        }
        DSW(DSD(ctx[2] + 4u) + 0x2Cu) = DSW(ctx[5] + 0x2Cu);
    } else {
        fighter_15584(slot, r[R_EDX], r[R_EBX]);
    }
    *eax = 0u;
}
static void m_23d38_noand(const u32 *r, u32 *eax)      /* case 3 without the `and [..+0x29],0xbf` (0x23E70) */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], o = DSD(slot + 8u);
    if (DSB(slot + 0x57u) == 3u && (DSW(rec + 0x28u) & 0x4000u) == 0u) {
        u32 dd = DSD(rec + 0x18u) - DSD(o + 0x18u);
        if ((s32)dd < 0) dd = 0u - dd;
        if ((s32)dd > 0xB00) { *eax = 0u; return; }
        DSD(o + 0x18u) = DSD(rec + 0x18u);
        DSD(o + 0x1Cu) = DSD(rec + 0x1Cu);
        DSW(o + 0x34u) = 0u;
        DSW(o + 0x2Cu) = 0x0E00u;
        actors_anim_begin(rec, 0x000E1BD2u, 0x40400000u);
        actors_anim_begin(o, 0x000E1C0Cu, 0x40400000u);
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
        (void)sound_voice(0xD6u);
    } else {
        fighter_23d38(slot, rec, r[R_EBX]);
    }
    *eax = 0u;
}

/* Final review I4 (record 2026-10-02-reverse-p1 §P1.12): the 0xD100 targets of the finisher streams, as the
 * animation dispatcher's opcode 0x11 calls them (0x2B57F..0x2B594: EAX = rec, EDX = the operand word, ECX =
 * 0). Mask 0: the dispatcher overwrites EAX (`mov eax,ecx` 0x2B59A). */
static void b_156d4(const u32 *r, u32 *eax)            { fighter_156d4(r[R_EAX]); *eax = 0u; }
static void b_23ca4(const u32 *r, u32 *eax)            { fighter_23ca4(r[R_EAX]); *eax = 0u; }
static void b_23868(const u32 *r, u32 *eax)            { fighter_23868(r[R_EAX], r[R_EDX]); *eax = 0u; }
static void b_3f174(const u32 *r, u32 *eax)            { fighter_3f174(r[R_EAX]); *eax = 0u; }
static void m_156d4(const u32 *r, u32 *eax)            /* the record's own +0x57, not its owner slot's */
{
    if (DSD(r[R_EAX] + 0x14u) != 0u) DSB(r[R_EAX] + 0x57u) = (u8)(DSB(r[R_EAX] + 0x57u) + 1u);
    *eax = 0u;
}
static void m_23ca4(const u32 *r, u32 *eax)            /* the distance divided unsigned */
{
    u32 rec = r[R_EAX], slot = DSD(rec + 0x14u);
    *eax = 0u;
    if (slot == 0u || DSD(DS_001077A8 + ((u32)(DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u) == 0u) return;
    DSB(slot + 0x54u) = 2u;
    DSW(rec + 0x36u) = 0x0200u;
    DSW(rec + 0x38u) = 0x0028u;
    DSW(rec + 0x44u) = 0x0010u;
    DSW(rec + 0x34u) = (u16)((DSD(0x000A83CCu + (u32)DSW(DS_00104AFC) * 4u) - DSD(rec + 0x18u)) / 0x48u);
    DSB(DS_000F0AFE) = 0u;
    DSB(DS_000F0AFF) = DSB(rec + 0x51u);
    DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
}
static void m_23868(const u32 *r, u32 *eax)            /* the held record's x offset by the other bit-14 arm */
{
    u32 rec = r[R_EAX], slot = DSD(rec + 0x14u), w, e, e2, bit;
    *eax = 0u;
    if (slot == 0u) return;
    w = DSW(0x000A8364u + r[R_EDX] * 2u);
    bit = DSW(rec + 0x28u) & 0x4000u;
    e = actor_spawn((const u32 *)(mem + 0x000BB330u), DSD(rec + 0x18u) + (bit ? 0xFFFFF400u : 0xC00u),
                    (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x1Cu) + 0x1200u, bit ? 0x4000u : 0u);
    DSD(slot + 8u) = e;
    DSW(e + 0x34u) = (u16)(bit ? w : 0u - w);
    DSD(DSD(slot + 8u) + 0x14u) = slot;
    e2 = actor_spawn((const u32 *)(mem + 0x000BB344u), 0u, 0u, 0u, (u32)DSW(rec + 0x56u) | 0x400u);
    DSB(e2 + 0x59u) = 2u;
    DSB(e2 + 0x60u) = 1u;
    DSB(rec + 0x4Bu) = DSB(e2 + 0x56u);
    if (DSB(rec + 0x51u) == 0u) return;
    actor_pset_palette(DSD(slot + 8u), 0x0Cu, 0u);
    actor_pset_palette(e2, 0x0Cu, 0u);
}
static void m_3f174(const u32 *r, u32 *eax)            /* +0x44 = 0x28 */
{
    DSW(r[R_EAX] + 0x36u) = 0x0280u;
    DSW(r[R_EAX] + 0x44u) = 0x0028u;
    *eax = 0u;
}

/* Track P batch 2 (record 2026-10-02-reverse-p2 §P2.2): the move callbacks as 0x34E2C calls them at
 * 0x35045, and the slot +0x0C callbacks as 0x3531C case 7 calls them at 0x35431 (EAX = slot, EDX = rec,
 * EBX = side). Mask 0: no caller reads the EAX they return. */
static void b_237d0(const u32 *r, u32 *eax)            { fighter_237d0(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_2381c(const u32 *r, u32 *eax)            { fighter_2381c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_3dadc(const u32 *r, u32 *eax)            { fighter_3dadc(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_3db34(const u32 *r, u32 *eax)            { fighter_3db34(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_3d10c(const u32 *r, u32 *eax)            { fighter_3d10c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_22a00(const u32 *r, u32 *eax)            { fighter_22a00(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_229fc(const u32 *r, u32 *eax)            { fighter_229fc(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_237d0(const u32 *r, u32 *eax)            /* 0x2381C's stream 0xE1506 */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000E1506u, 0x40400000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x0Cu) = 0u;
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
    (void)sound_voice(0xAAu);
}
static void m_237d0_guard(const u32 *r, u32 *eax)      /* the guard tests the low byte of +8 */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSB(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000E14D8u, 0x40400000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x0Cu) = 0u;
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
    (void)sound_voice(0xAAu);
}
static void m_2381c(const u32 *r, u32 *eax)            /* the voice 0xB8 (character 5's) */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000E1506u, 0x40400000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x0Cu) = 0u;
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
    (void)sound_voice(0xB8u);
}
static void m_3dadc(const u32 *r, u32 *eax)            /* +0x64/+0x5F stored after the voice */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000D4AB2u, 0x40400000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x18u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x1Cu) = 0u;
    (void)sound_voice(0xB8u);
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
}
static void m_3db34(const u32 *r, u32 *eax)            /* the frame 2.0, not 3.0 */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000D4AFAu, 0x40000000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x18u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x1Cu) = 0u;
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
    (void)sound_voice(0xB8u);
}
static void m_3d10c(const u32 *r, u32 *eax)            /* the record started before the voice */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u32 i = (u32)DSB(rec + 0x51u);
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(rec, 0x000E84C8u, 0x40400000u);
    (void)sound_voice(0x91u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSB(slot + 0x5Fu) = 0xFFu;
    DSB(slot + 0x64u) = r5f;
    DSW(0x001080ACu + i * 2u) = 0x0080u;
}
static void m_3d10c_side(const u32 *r, u32 *eax)       /* indexes the word by side, not rec+0x51 */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    (void)sound_voice(0x91u);
    hit_anim_start_b(rec, 0x000E84C8u, 0x40400000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSB(slot + 0x5Fu) = 0xFFu;
    DSB(slot + 0x64u) = r5f;
    DSW(0x001080ACu + r[R_EBX] * 2u) = 0x0080u;
}
static void m_3d10c_sext(const u32 *r, u32 *eax)       /* rec+0x51 sign-extended (movsx), not zero-extended */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u32 i = (u32)(s32)(s8)DSB(rec + 0x51u);
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    (void)sound_voice(0x91u);
    hit_anim_start_b(rec, 0x000E84C8u, 0x40400000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSB(slot + 0x5Fu) = 0xFFu;
    DSB(slot + 0x64u) = r5f;
    DSW(0x001080ACu + i * 2u) = 0x0080u;
}
static void m_22a00(const u32 *r, u32 *eax)            /* the slot stores before the 0x3C4CC call */
{
    u32 slot = r[R_EAX];
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0x000229FCu;
    DSB(slot + 0x57u) = 0u;
    DSB(slot + 0x64u) = DSB(slot + 0x5Fu);
    hit_anim_start_b(r[R_EDX], 0x000E1534u, 0x40400000u);
}
static void m_229fc(const u32 *r, u32 *eax)            /* steps the slot's +0x57 */
{
    DSB(r[R_EAX] + 0x57u) = (u8)(DSB(r[R_EAX] + 0x57u) + 1u);
    *eax = 0u;
}

/* §P2.4: the 0xD000 targets as the animation dispatcher calls them at 0x2B56D (EAX = rec), mask 0 (0x2B575
 * overwrites EAX). */
static void b_14ef8(const u32 *r, u32 *eax)            { fighter_14ef8(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_14f50(const u32 *r, u32 *eax)            { fighter_14f50(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_14fa8(const u32 *r, u32 *eax)            { fighter_14fa8(r[R_EAX]); *eax = 0u; }
static void b_14ff8(const u32 *r, u32 *eax)            { fighter_14ff8(r[R_EAX]); *eax = 0u; }
static void b_150ac(const u32 *r, u32 *eax)            { fighter_150ac(r[R_EAX]); *eax = 0u; }
static void m_14ef8(const u32 *r, u32 *eax)            /* the frame 3.0, not 2.0 */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000D2E26u, 0x40400000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x18u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x1Cu) = 0u;
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
    (void)sound_voice(0xB2u);
}
static void m_14f50(const u32 *r, u32 *eax)            /* 0x14EF8's stream 0xD2E26 */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000D2E26u, 0x40000000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x18u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x1Cu) = 0u;
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
    (void)sound_voice(0xB2u);
}
static void m_14fa8(const u32 *r, u32 *eax)            /* 0x14FF8's descriptor 0xBB380 */
{
    u32 rec = r[R_EAX];
    u32 e = actor_spawn((const u32 *)(mem + 0x000BB380u), 0u, 0u, 0u, (u32)(DSW(rec + 0x56u) | 0x400u));
    DSB(e + 0x59u) = 2u;
    if (DSB(rec + 0x51u) != 0u) {
        DSB(e + 0x4Eu) = 1u;
        DSW(e + 0x2Eu) = (u16)(DSW(e + 0x2Eu) + 4u);
    }
    DSB(rec + 0x4Bu) = DSB(e + 0x56u);
    DSB(e + 0x60u) = 1u;
    *eax = 0u;
}
static void m_held(u32 rec, u32 w1, u32 w0, int same_dx)
{
    u32 slot = DSD(rec + 0x14u), w, dx, flag, e;
    if (slot == 0u) return;
    if (fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0) { w = w1; dx = same_dx ? 0x1200u : 0xFFFFEE00u; }
    else { w = w0; dx = 0x1200u; }
    flag = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 0x4000u : 0u;
    e = actor_spawn((const u32 *)(mem + 0x000BB380u), DSD(rec + 0x18u) + (u32)(s32)(s16)dx,
                    (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x1Cu) + (same_dx ? 0x1600u : 0x1200u), flag);
    DSD(slot + 8u) = e;
    DSB(e + 0x59u) = 2u;
    DSW(DSD(slot + 8u) + 0x34u) = (u16)w;
    DSD(DSD(slot + 8u) + 0x14u) = slot;
    if (DSB(rec + 0x51u) == 0u) return;
    DSW(DSD(slot + 8u) + 0x2Eu) = (u16)(DSW(DSD(slot + 8u) + 0x2Eu) + 4u);
    DSB(DSD(slot + 8u) + 0x4Eu) = 1u;
}
static void m_14ff8(const u32 *r, u32 *eax)            /* the x offset +0x1200 on both arms */
{
    m_held(r[R_EAX], 0xFFFFFEB6u, 0x14Au, 1);
    *eax = 0u;
}
static void m_150ac(const u32 *r, u32 *eax)            /* the z offset 0x1200, not 0x1600 */
{
    m_held(r[R_EAX], 0xFFFFFDDAu, 0x226u, 0);
    *eax = 0u;
}

/* §P2.5: the unconditional move callbacks (0x34E2C at 0x35045), mask 0. */
static void b_15478(const u32 *r, u32 *eax)            { fighter_15478(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_3dcec(const u32 *r, u32 *eax)            { fighter_3dcec(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_21114(const u32 *r, u32 *eax)            { fighter_21114(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_15478(const u32 *r, u32 *eax)            /* the slot stores before the 0x3C4CC call */
{
    u32 slot = r[R_EAX];
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x53u) = 8u;
    DSB(slot + 0x54u) = 0u;
    hit_anim_start_b(r[R_EDX], 0x000D2DD2u, 0x40800000u);
    *eax = 0u;
}
static void m_3dcec(const u32 *r, u32 *eax)            /* the image's stream 0xD40F2 hard-coded */
{
    u32 slot = r[R_EAX];
    hit_anim_start_b(r[R_EDX], 0x000D40F2u, 0x40800000u);
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x53u) = 8u;
    DSB(slot + 0x54u) = 1u;
    *eax = 0u;
}
static void m_21114_at(const u32 *r, u32 *eax, u32 idx, u32 s1, u32 s6)
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u32 p = DSD(DS_001077A8 + idx * 4u);
    *eax = 0u;
    if (p == 0u) return;
    if (DSB(p + 0x7Au) == 1u) hit_anim_start_b(rec, s1, 0x40A00000u);
    else if (DSB(p + 0x7Au) == 6u) hit_anim_start_b(rec, s6, 0x40A00000u);
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x53u) = 8u;
    DSB(slot + 0x54u) = 1u;
}
static void m_21114(const u32 *r, u32 *eax)            /* the two characters' streams swapped */
{
    m_21114_at(r, eax, (u32)DSB(r[R_EDX] + 0x51u), 0x000E1702u, 0x000E481Cu);
}
static void m_21114_side(const u32 *r, u32 *eax)       /* the other side's pointer (rec+0x51 ^ 1) */
{
    m_21114_at(r, eax, (u32)DSB(r[R_EDX] + 0x51u) ^ 1u, 0x000E481Cu, 0x000E1702u);
}

/* §P2.6: the context-built move callbacks (0x34E2C at 0x35045), mask 0. */
static void b_21374(const u32 *r, u32 *eax)            { fighter_21374(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_22938(const u32 *r, u32 *eax)            { fighter_22938(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_21374(const u32 *r, u32 *eax)            /* the stream by the other slot's character */
{
    u32 ctx[6];
    fighter_ctx_same(ctx, r[R_EBX]);
    hit_anim_start_b(ctx[4], DSD(0x000C8950u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40000000u);
    DSB(ctx[2] + 0x53u) = 7u;
    DSB(ctx[2] + 0x52u) = 9u;
    DSB(ctx[2] + 0x54u) = 0u;
    DSD(ctx[2] + 0x0Cu) = 0x000212CCu;
    DSD(ctx[2] + 0x18u) = 0x0002116Cu;
    DSD(ctx[2] + 0x1Cu) = 0x000211F0u;
    DSB(ctx[2] + 0x57u) = 0u;
    DSB(ctx[2] + 0x41u) = (u8)(DSB(ctx[2] + 0x41u) | 0x80u);
    *eax = 0u;
}
static void m_22938_at(const u32 *r, u32 *eax, int own_flash, int late_first)
{
    u32 ctx[6];
    *eax = 0u;
    fighter_ctx_same(ctx, r[R_EBX]);
    if ((DSB(ctx[3] + 0x42u) & 0x10u) != 0u) return;
    DSB(ctx[2] + 0x57u) = 0u;
    DSD(ctx[2] + 0x0Cu) = 0x00022638u;
    DSD(ctx[2] + 0x18u) = 0x00022510u;
    DSD(ctx[2] + 0x1Cu) = 0x00022588u;
    DSB(ctx[2] + 0x52u) = 9u;
    DSB(ctx[2] + 0x54u) = 0u;
    DSB(ctx[2] + 0x53u) = 7u;
    DSW(0x00104758u + ctx[0] * 2u) = 0u;
    if (late_first) {
        DSW(0x00104754u + ctx[0] * 2u) = 0u;
        DSD(0x00104738u + ctx[0] * 4u) = 0x40400000u;
    }
    hit_anim_start_b(ctx[4], 0x000E4DB4u, 0x40400000u);
    hit_flash_pair(own_flash ? ctx[0] : ctx[1]);
    DSB(ctx[2] + 0x42u) = (u8)(DSB(ctx[2] + 0x42u) | 4u);
    DSW(0x00104754u + ctx[0] * 2u) = 0u;
    DSD(0x00104738u + ctx[0] * 4u) = 0x40400000u;
}
static void m_22938(const u32 *r, u32 *eax)            /* 0x34D8C on the own side */
{
    m_22938_at(r, eax, 1, 0);
}
static void m_22938_order(const u32 *r, u32 *eax)      /* the word 0x104754 and the float before the calls */
{
    m_22938_at(r, eax, 0, 1);
}

/* §P2.7: the slot +0x18 hooks as 0x19020 calls them at 0x1903F (EAX = side); 0x19048 tests the whole EAX. */
static void b_2116c(const u32 *r, u32 *eax)            { *eax = fighter_2116c(r[R_EAX]); }
static void b_22510(const u32 *r, u32 *eax)            { *eax = fighter_22510(r[R_EAX]); }
static u32 m_hook(u32 side, u32 lo_box, u32 hi_box, int mode)
{
    u32 ctx[6];
    u8 f[16];
    u32 k;
    fighter_ctx_same(ctx, side);
    for (k = 0; k < 16u; k++) f[k] = 2u;
    f[1] = f[8] = f[4] = f[0xE] = f[7] = 0u;
    f[5] = 1u;
    f[0xD] = (mode == 1) ? 2u : 0u;
    if (lo_box == 0x000A81BEu) {
        if (mode == 2) {
            if (DSW(0x000A81AEu) < DSW(ctx[2] + 0x88u) || DSW(0x000A81ACu) > DSW(ctx[2] + 0x88u)) return 1u;
        } else if ((s16)DSW(0x000A81AEu) < (s16)DSW(ctx[2] + 0x88u)
                   || (s16)DSW(0x000A81ACu) > (s16)DSW(ctx[2] + 0x88u)) {
            return 1u;
        }
        k = (u32)fighter_18c14(ctx[0], f, lo_box, hi_box);
        return mode == 3 ? 1u : k;
    }
    f[9] = (mode == 1) ? 0u : 1u;
    f[0xD] = 0u;
    {
        s32 w = (s32)(s16)DSW(0x00104758u + ctx[0] * 2u);
        if ((mode == 4 ? w >= 0x14 : w > 0x14) || w < 0x0D) return 1u;
    }
    return (u32)fighter_18c14(ctx[0], f, lo_box, hi_box);
}
static void m_2116c(const u32 *r, u32 *eax)            /* flag 0xD left at 2 */
{
    *eax = m_hook(r[R_EAX], 0x000A81BEu, 0x000A81C8u, 1);
}
static void m_2116c_unsigned(const u32 *r, u32 *eax)   /* the bounds compared unsigned */
{
    *eax = m_hook(r[R_EAX], 0x000A81BEu, 0x000A81C8u, 2);
}
static void m_2116c_eax(const u32 *r, u32 *eax)        /* 1 instead of 0x18C14's result */
{
    *eax = m_hook(r[R_EAX], 0x000A81BEu, 0x000A81C8u, 3);
}
static void m_22510(const u32 *r, u32 *eax)            /* flag 9 = 0, not 1 */
{
    *eax = m_hook(r[R_EAX], 0x000A82C4u, 0x000A82CEu, 1);
}
static void m_22510_ge(const u32 *r, u32 *eax)         /* `>= 0x14` for `jg` */
{
    *eax = m_hook(r[R_EAX], 0x000A82C4u, 0x000A82CEu, 4);
}

/* §P2.8: the slot +0x1C callbacks as 0x193B0 calls them at 0x19505 (EAX = side; 0x19508 reloads EAX), and
 * 0x22404 as 0x22588 (0x225A6) calls it (0x225AB reloads EAX): mask 0. */
static void b_22404(const u32 *r, u32 *eax)            { fighter_22404(r[R_EAX]); *eax = 0u; }
static void b_211f0(const u32 *r, u32 *eax)            { fighter_211f0(r[R_EAX]); *eax = 0u; }
static void b_22588(const u32 *r, u32 *eax)            { fighter_22588(r[R_EAX]); *eax = 0u; }
static void m_22404_at(u32 side, int late57, int zext)
{
    u32 ctx[6];
    u32 w;
    fighter_ctx_same(ctx, side);
    actors_anim_begin(ctx[4], 0x000E4DEAu, 0x40000000u);
    if (!late57) DSB(ctx[2] + 0x57u) = 2u;
    hit_anim_start_a(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40000000u);
    if (late57) DSB(ctx[2] + 0x57u) = 2u;
    w = DSW(0x000A82D8u + (u32)DSB(ctx[3] + 0x7Au) * 2u);
    fighter_3c208(ctx[0], zext ? (s32)w : (s32)(s16)w);
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x52u) = 9u;
    DSB(ctx[3] + 0x54u) = 0u;
    DSD(ctx[3] + 0x10u) = 0u;
}
static void m_22404(const u32 *r, u32 *eax)            /* +0x57 = 2 after the 0x3C480 call */
{
    m_22404_at(r[R_EAX], 1, 0);
    *eax = 0u;
}
static void m_22404_signed(const u32 *r, u32 *eax)     /* the distance word zero-extended */
{
    m_22404_at(r[R_EAX], 0, 1);
    *eax = 0u;
}
static void m_211f0_at(u32 side, int own_char, int early57, int sext, int other5f)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);
    hit_flash_pair(ctx[0]);
    hit_anim_start_b(ctx[4], 0x000E1672u, 0x40000000u);
    hit_anim_start_a(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[own_char ? 2 : 3] + 0x7Au) * 4u), 0x40000000u);
    fighter_18af8();
    fighter_3c208(ctx[0], sext ? (s32)(s16)DSW(0x000A81B0u + (u32)DSB(ctx[3] + 0x7Au) * 2u)
                              : (s32)(u32)DSW(0x000A81B0u + (u32)DSB(ctx[3] + 0x7Au) * 2u));
    fighter_39834(ctx[1], (s32)(u32)DSB(ctx[other5f ? 3 : 2] + 0x5Fu));
    fighter_3c358(ctx[0]);
    fighter_39a10(ctx[4], 0x29Au);
    fighter_39a10(ctx[5], 0x29Au);
    if (early57) DSB(ctx[2] + 0x57u) = 2u;
    (void)sound_voice((u32)DSW(0x000C75AAu + (u32)DSB(ctx[3] + 0x7Au) * 2u));
    DSB(ctx[2] + 0x57u) = 2u;
    DSB(ctx[3] + 0x53u) = 0x0Fu;
}
static void m_211f0(const u32 *r, u32 *eax)            /* the other record's stream by the own character */
{
    m_211f0_at(r[R_EAX], 1, 0, 0, 0);
    *eax = 0u;
}
static void m_211f0_order(const u32 *r, u32 *eax)      /* +0x57 = 2 before the voice */
{
    m_211f0_at(r[R_EAX], 0, 1, 0, 0);
    *eax = 0u;
}
static void m_211f0_zext(const u32 *r, u32 *eax)       /* the 0xA81B0 word sign-extended */
{
    m_211f0_at(r[R_EAX], 0, 0, 1, 0);
    *eax = 0u;
}
static void m_211f0_slot(const u32 *r, u32 *eax)       /* 0x39834's byte from the other slot's +0x5F */
{
    m_211f0_at(r[R_EAX], 0, 0, 0, 1);
    *eax = 0u;
}
static void m_22588_at(u32 side, int own_flash, int late5d)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);
    fighter_18af8();
    hit_flash_pair(own_flash ? ctx[0] : ctx[1]);
    fighter_22404(ctx[0]);
    fighter_3c358(ctx[0]);
    if (!late5d) DSB(ctx[3] + 0x5Du) = 0x44u;
    fighter_3c208(ctx[0], (s32)(s16)DSW(0x000A82D8u + (u32)DSB(ctx[3] + 0x7Au) * 2u));
    if (late5d) DSB(ctx[3] + 0x5Du) = 0x44u;
    fighter_39a10(ctx[4], 0x29Au);
    fighter_39a10(ctx[5], 0x29Au);
    (void)sound_voice((u32)DSW(0x000C75AAu + (u32)DSB(ctx[3] + 0x7Au) * 2u));
}
static void m_22588(const u32 *r, u32 *eax)            /* 0x34D8C on the own side */
{
    m_22588_at(r[R_EAX], 1, 0);
    *eax = 0u;
}
static void m_22588_order(const u32 *r, u32 *eax)      /* +0x5D after the 0x3C208 call */
{
    m_22588_at(r[R_EAX], 0, 1);
    *eax = 0u;
}

/* §P2.9: the slot +0x0C callbacks as 0x3531C case 7 calls them (0x35431), mask 0. */
static void b_212cc(const u32 *r, u32 *eax)            { fighter_212cc(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_22638(const u32 *r, u32 *eax)            { fighter_22638(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_212cc_at(const u32 *r, int mode)
{
    u32 ctx[6];
    u8 st;
    fighter_ctx_same(ctx, r[R_EBX]);
    st = DSB(ctx[2] + 0x57u);
    if (st == 0u) {
        if (mode == 1 ? DSW(0x000A81AEu) >= DSW(ctx[2] + 0x88u)
                      : (s16)DSW(0x000A81AEu) >= (s16)DSW(ctx[2] + 0x88u)) return;
        DSB(ctx[2] + 0x57u) = 1u;
        return;
    }
    if (st != 1u) return;
    {
        u32 p = DSD(DS_001077A8 + (mode == 2 ? ctx[0] : (u32)DSB(r[R_EDX] + 0x51u)) * 4u);
        u8 c;
        if (p == 0u) return;
        c = DSB(p + 0x7Au);
        if (c == 1u) hit_anim_start_b(ctx[4], mode == 3 ? 0x000E16E6u : 0x000E4A18u, 0x40400000u);
        else if (c == 6u) hit_anim_start_b(ctx[4], mode == 3 ? 0x000E4A18u : 0x000E16E6u, 0x40400000u);
    }
    DSB(ctx[2] + 0x52u) = 9u;
    DSB(ctx[2] + 0x57u) = 2u;
    DSB(ctx[2] + 0x8Au) = 0u;
}
static void m_212cc(const u32 *r, u32 *eax)            /* the two characters' streams swapped */
{
    m_212cc_at(r, 3);
    *eax = 0u;
}
static void m_212cc_signed(const u32 *r, u32 *eax)     /* the state-0 bound compared unsigned */
{
    m_212cc_at(r, 1);
    *eax = 0u;
}
static void m_212cc_side(const u32 *r, u32 *eax)       /* the pointer by the context's side, not rec+0x51 */
{
    m_212cc_at(r, 2);
    *eax = 0u;
}
static void m_22638(const u32 *r, u32 *eax)            /* the voice 0x78 for the bit-0 start too */
{
    u32 ctx[6], c, cmd, a = DS_001088E0;
    union { float f; u32 u; } v;
    double d;
    fighter_ctx_same(ctx, r[R_EBX]);
    {
        u32 t = (u32)DSW(a + ctx[1] * 2u) & 0xF0u;
        DSW(0x00104758u + ctx[0] * 2u) = (u16)(DSW(0x00104758u + ctx[0] * 2u) + 1u);
        if (t != 0u || DSB(ctx[3] + 0x63u) != 0u) {
            c = (u32)DSB(ctx[3] + 0x7Au);
            if ((s32)(u32)DSB(ctx[3] + 0x5Du) <= (s32)(s16)DSW(0x000A82ECu + c * 2u))
                DSB(ctx[3] + 0x5Du) = 0u;
            else
                DSB(ctx[3] + 0x5Du) = (u8)(DSB(ctx[3] + 0x5Du) - DSB(0x000A82ECu + c * 2u));
        }
    }
    c = (u32)DSB(ctx[3] + 0x7Au);
    if ((s16)DSW(0x000A8300u + c * 2u) > (s16)DSW(0x00104758u + ctx[0] * 2u)) {
        u8 b = DSB(ctx[3] + 0x5Du);
        DSB(ctx[3] + 0x5Du) = (u8)(b >= 1u ? b : 1u);
    }
    cmd = (u32)DSW(a + ctx[0] * 2u);
    v.u = DSD(0x00104738u + ctx[0] * 4u);
    if ((cmd & 1u) != 0u) {
        memcpy(&d, mem + 0x0008098Cu, sizeof d);
        v.f = (float)((double)v.f + d);
        if (v.f < 1.0f) v.f = 1.0f;
        DSD(0x00104738u + ctx[0] * 4u) = v.u;
    } else {
        double sum;
        memcpy(&d, mem + 0x00080980u, sizeof d);
        sum = (double)v.f + d;
        v.f = (float)sum;
        if (sum > 3.0) v.f = 3.0f;
        DSD(0x00104738u + ctx[0] * 4u) = v.u;
    }
    if ((cmd & 0x0Eu) != 0u) DSW(0x00104754u + ctx[0] * 2u) = (u16)cmd;
    *eax = 0u;
    switch (DSB(ctx[2] + 0x57u)) {
    case 0:
        if ((s16)DSW(0x00104758u + ctx[0] * 2u) > 0x14) DSB(ctx[2] + 0x57u) = 1u;
        return;
    case 1:
        actors_anim_begin(ctx[4], 0x000E4DCEu, 0x40400000u);
        DSB(ctx[2] + 0x57u) = 3u;
        DSB(ctx[2] + 0x8Au) = 0u;
        return;
    case 2:
        if (DSB(ctx[3] + 0x5Du) < 1u) { fighter_36870(ctx[5]); DSB(ctx[2] + 0x57u) = 1u; return; }
        if (DSB(ctx[3] + 0x53u) != 0x0Au) { DSB(ctx[2] + 0x57u) = 1u; return; }
        if ((cmd & 1u) != 0u) {
            actors_anim_begin(ctx[4], 0x000E4E08u, DSD(0x00104738u + ctx[0] * 4u));
            DSB(ctx[2] + 0x57u) = 4u;
            (void)sound_voice(0x78u);
        } else if ((DSW(0x00104754u + ctx[0] * 2u) & 4u) != 0u) {
            actors_anim_begin(ctx[4], 0x000E4E34u, 0x40400000u);
            DSB(ctx[2] + 0x57u) = 5u;
            (void)sound_voice(0x78u);
        } else if ((DSW(0x00104754u + ctx[0] * 2u) & 2u) != 0u) {
            actors_anim_begin(ctx[4], 0x000E4E4Au, 0x40400000u);
            DSB(ctx[2] + 0x57u) = 6u;
            (void)sound_voice(0x78u);
        } else if ((DSW(0x00104754u + ctx[0] * 2u) & 8u) != 0u) {
            actors_anim_begin(ctx[4], 0x000E4E72u, 0x40400000u);
            DSB(ctx[2] + 0x57u) = 7u;
            (void)sound_voice(0x78u);
        }
        return;
    default:
        return;
    }
}
/* 0x22638's two boundary mutants run the port, then redo the one store their bug changes. */
static void m_22638_signed(const u32 *r, u32 *eax)     /* the +0x5D floor's count compared unsigned */
{
    u32 ctx[6], c;
    u8 b5d;
    int floor_s, floor_u;
    fighter_ctx_same(ctx, r[R_EBX]);
    fighter_22638(r[R_EAX], r[R_EDX], r[R_EBX]);
    c = (u32)DSB(ctx[3] + 0x7Au);
    floor_s = (s16)DSW(0x000A8300u + c * 2u) > (s16)DSW(0x00104758u + ctx[0] * 2u);
    floor_u = DSW(0x000A8300u + c * 2u) > DSW(0x00104758u + ctx[0] * 2u);
    b5d = DSB(ctx[3] + 0x5Du);
    if (floor_s && !floor_u && b5d == 1u) DSB(ctx[3] + 0x5Du) = 0u;
    *eax = 0u;
}
static void m_22638_char(const u32 *r, u32 *eax)       /* the floor's 0xA8300 indexed by the own slot's character */
{
    u32 ctx[6], c_oth, c_own;
    int floor_ok, floor_bad;
    fighter_ctx_same(ctx, r[R_EBX]);
    fighter_22638(r[R_EAX], r[R_EDX], r[R_EBX]);
    c_oth = (u32)DSB(ctx[3] + 0x7Au);
    c_own = (u32)DSB(ctx[2] + 0x7Au);
    floor_ok = (s16)DSW(0x000A8300u + c_oth * 2u) > (s16)DSW(0x00104758u + ctx[0] * 2u);
    floor_bad = (s16)DSW(0x000A8300u + c_own * 2u) > (s16)DSW(0x00104758u + ctx[0] * 2u);
    if (floor_ok && !floor_bad && DSB(ctx[3] + 0x5Du) == 1u) DSB(ctx[3] + 0x5Du) = 0u;
    *eax = 0u;
}
static void m_22638_byte5d(const u32 *r, u32 *eax)     /* the drain compares +0x5D as a signed byte */
{
    u32 ctx[6], c;
    s8 b;
    int drain;
    fighter_ctx_same(ctx, r[R_EBX]);
    c = (u32)DSB(ctx[3] + 0x7Au);
    b = (s8)DSB(ctx[3] + 0x5Du);
    drain = (DSW(DS_001088E0 + ctx[1] * 2u) & 0xF0u) != 0u || DSB(ctx[3] + 0x63u) != 0u;
    fighter_22638(r[R_EAX], r[R_EDX], r[R_EBX]);
    if (drain && b < 0 && (s32)b <= (s32)(s16)DSW(0x000A82ECu + c * 2u)) DSB(ctx[3] + 0x5Du) = 0u;
    *eax = 0u;
}

/* Track P batch 3 (record 2026-10-03-reverse-p3 §P3.3): character 2's move callbacks with no context, as 0x34E2C
 * calls them at 0x35045 (EAX = slot, EDX = rec, EBX = side). Mask 0 (record 2026-10-02-reverse-p2 §P2.2). */
static void b_475ec(const u32 *r, u32 *eax)            { fighter_475ec(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_47608(const u32 *r, u32 *eax)            { fighter_47608(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_47624(const u32 *r, u32 *eax)            { fighter_47624(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_48964(const u32 *r, u32 *eax)            { fighter_48964(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_489a0(const u32 *r, u32 *eax)            { fighter_489a0(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_475ec_at(const u32 *r, u32 by_rec, u32 b43, u32 w34, int late)
{
    u32 rec = r[R_EDX];
    int al;
    if (!late) {
        DSB(rec + 0x43u) = (u8)b43;
        DSW(rec + 0x34u) = (u16)w34;
    }
    al = fighter_actor_bit15_clear(by_rec ? (u32)DSB(rec + 0x51u) : r[R_EBX]);
    if (late) {
        DSB(rec + 0x43u) = (u8)b43;
        DSW(rec + 0x34u) = (u16)w34;
    }
    if (al != 0) DSW(rec + 0x34u) = (u16)(0u - DSW(rec + 0x34u));
}
static void m_475ec(const u32 *r, u32 *eax)            /* the two stores after the 0x1A570 call */
{
    m_475ec_at(r, 0u, 0x28u, 0x0258u, 1);
    *eax = 0u;
}
static void m_475ec_side(const u32 *r, u32 *eax)       /* 0x1A570 on rec+0x51, not EBX */
{
    m_475ec_at(r, 1u, 0x28u, 0x0258u, 0);
    *eax = 0u;
}
static void m_47608(const u32 *r, u32 *eax)            /* the two stores after the 0x1A570 call */
{
    m_475ec_at(r, 0u, 0x22u, 0x02EEu, 1);
    *eax = 0u;
}
static void m_47624(const u32 *r, u32 *eax)            /* the slot stores before the 0x2BC30 call */
{
    DSB(r[R_EAX] + 0x52u) = 9u;
    DSB(r[R_EAX] + 0x53u) = 8u;
    DSB(r[R_EAX] + 0x54u) = 0u;
    actors_anim_begin(r[R_EDX], 0x000ED79Au, 0x40800000u);
    *eax = 0u;
}
static void m_48964_at(const u32 *r, u32 when_al, u32 other, int by_side, int late41, int sext)
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    int al;
    if ((DSB(slot + 0x41u) & 0x40u) != 0u) return;
    if (!late41) DSB(slot + 0x41u) = (u8)(DSB(slot + 0x41u) | 0x40u);
    al = fighter_actor_bit15_clear(by_side ? r[R_EBX]
                                   : sext ? (u32)(int)(signed char)DSB(rec + 0x51u) : (u32)DSB(rec + 0x51u));
    if (late41) DSB(slot + 0x41u) = (u8)(DSB(slot + 0x41u) | 0x40u);
    fighter_state_35838(slot, rec, al != 0 ? when_al : other);
}
static void m_48964(const u32 *r, u32 *eax)            /* the directions swapped */
{
    m_48964_at(r, 0x1000u, 0x2000u, 0, 0, 0);
    *eax = 0u;
}
static void m_48964_late(const u32 *r, u32 *eax)       /* +0x41 bit 6 set after the 0x1A570 call */
{
    m_48964_at(r, 0x2000u, 0x1000u, 0, 1, 0);
    *eax = 0u;
}
static void m_48964_side(const u32 *r, u32 *eax)       /* 0x1A570 on EBX, not rec+0x51 */
{
    m_48964_at(r, 0x2000u, 0x1000u, 1, 0, 0);
    *eax = 0u;
}
static void m_48964_sext(const u32 *r, u32 *eax)       /* rec+0x51 sign-extended (the original zero-extends) */
{
    m_48964_at(r, 0x2000u, 0x1000u, 0, 0, 1);
    *eax = 0u;
}
static void m_489a0(const u32 *r, u32 *eax)            /* +0x41 bit 6 set after the 0x1A570 call */
{
    m_48964_at(r, 0x1000u, 0x2000u, 0, 1, 0);
    *eax = 0u;
}
static void m_489a0_swap(const u32 *r, u32 *eax)       /* the directions swapped */
{
    m_48964_at(r, 0x2000u, 0x1000u, 0, 0, 0);
    *eax = 0u;
}
static void m_489a0_side(const u32 *r, u32 *eax)       /* 0x1A570 on EBX, not rec+0x51 */
{
    m_48964_at(r, 0x1000u, 0x2000u, 1, 0, 0);
    *eax = 0u;
}
static void m_489a0_sext(const u32 *r, u32 *eax)       /* rec+0x51 sign-extended (the original zero-extends) */
{
    m_48964_at(r, 0x1000u, 0x2000u, 0, 0, 1);
    *eax = 0u;
}

/* §P3.4: 0x47720 (0x34E2C at 0x35045) and the +0x0C callback 0x476FC (0x3531C case 7), mask 0; the +0x18 hook
 * 0x47648 (0x19020 at 0x1903F, the whole EAX tested); the +0x1C callback 0x47688 (0x193B0 at 0x19505, mask 0). */
static void b_47720(const u32 *r, u32 *eax)            { fighter_47720(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_476fc(const u32 *r, u32 *eax)            { fighter_476fc(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_47648(const u32 *r, u32 *eax)            { *eax = fighter_47648(r[R_EAX]); }
static void b_47688(const u32 *r, u32 *eax)            { fighter_47688(r[R_EAX]); *eax = 0u; }
static void m_47720_at(const u32 *r, int by_edx, int by_ebx)
{
    u32 ctx[6];
    if (by_ebx) fighter_ctx_same(ctx, r[R_EBX]);
    else hit_anim_ctx(ctx, r[R_EDX]);
    hit_anim_start_b(by_edx ? r[R_EDX] : ctx[4], 0x000ECE1Cu, 0x40000000u);
    DSB(ctx[2] + 0x52u) = 9u;
    DSB(ctx[2] + 0x53u) = 7u;
    DSD(ctx[2] + 0x0Cu) = 0x000476FCu;
    DSD(ctx[2] + 0x18u) = 0x00047648u;
    DSD(ctx[2] + 0x1Cu) = 0x00047688u;
}
static void m_47720(const u32 *r, u32 *eax)            /* the EDX record started, not the slot's own */
{
    m_47720_at(r, 1, 0);
    *eax = 0u;
}
static void m_47720_ebx(const u32 *r, u32 *eax)        /* the context by EBX, not the record's +0x51 */
{
    m_47720_at(r, 0, 1);
    *eax = 0u;
}
static void m_476fc_at(const u32 *r, int sext, s32 bound)
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    s32 b = sext ? (s32)(s8)DSB(rec + 0x63u) : (s32)(u32)DSB(rec + 0x63u);
    if (b < bound) return;
    DSD(slot + 0x18u) = 0x00047648u;
    DSD(slot + 0x1Cu) = 0x00047688u;
    DSD(slot + 0x0Cu) = 0u;
}
static void m_476fc(const u32 *r, u32 *eax)            /* the bound 6 */
{
    m_476fc_at(r, 0, 6);
    *eax = 0u;
}
static void m_476fc_sext(const u32 *r, u32 *eax)       /* +0x63 read as a signed byte */
{
    m_476fc_at(r, 1, 5);
    *eax = 0u;
}
static void m_47648(const u32 *r, u32 *eax)            /* flag 0 left at 2 */
{
    u32 ctx[6], k;
    u8 f[16];
    fighter_ctx_same(ctx, r[R_EAX]);
    for (k = 0; k < 16u; k++) f[k] = 2u;
    f[1] = 0;
    f[8] = 0;
    *eax = (u32)fighter_18c14(ctx[0], f, 0u, 0u);
}
static void m_47688_at(u32 side, u32 stance, int own_pivot, int zext, int whole_eax)
{
    u32 ctx[6];
    u32 w, al;
    fighter_ctx_same(ctx, side);
    al = (u32)fighter_command_dispatch(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));
    if ((whole_eax ? al : (u32)(u8)al) != 0u) return;
    fighter_39834(ctx[1], (s32)(u32)DSB(ctx[2] + 0x5Fu));
    if (DSB(ctx[3] + 0x54u) == 2u) fighter_39fb0(own_pivot ? ctx[2] : ctx[3]);
    else fighter_3a95c(ctx[1], stance);
    w = DSW(0x000BEDD8u);
    fighter_39a10(ctx[5], zext ? w : (u32)(s32)(s16)w);
}
static void m_47688(const u32 *r, u32 *eax)            /* 0x3A95C with 0xE */
{
    m_47688_at(r[R_EAX], 0x0Eu, 0, 0, 0);
    *eax = 0u;
}
static void m_47688_pivot(const u32 *r, u32 *eax)      /* 0x39FB0 on the own slot */
{
    m_47688_at(r[R_EAX], 0x0Fu, 1, 0, 0);
    *eax = 0u;
}
static void m_47688_zext(const u32 *r, u32 *eax)       /* the timer word zero-extended */
{
    m_47688_at(r[R_EAX], 0x0Fu, 0, 1, 0);
    *eax = 0u;
}
static void m_47688_eax(const u32 *r, u32 *eax)        /* 0x3B298's whole EAX tested, not AL */
{
    m_47688_at(r[R_EAX], 0x0Fu, 0, 0, 1);
    *eax = 0u;
}

/* §P3.5: 0x47874 (0x34E2C at 0x35045) and its +0x0C callback 0x47830 (0x3531C case 7), mask 0; the +0x14
 * callback 0x47798 (fn(slot), EAX = EDX = the slot; the callers test the whole EAX); the +0x18 hook 0x477A8 (the
 * whole EAX); the +0x1C callback 0x477E8 (mask 0). */
static void b_47874(const u32 *r, u32 *eax)            { fighter_47874(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_47830(const u32 *r, u32 *eax)            { fighter_47830(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_47798(const u32 *r, u32 *eax)            { *eax = fighter_47798(r[R_EAX]); }
static void b_477a8(const u32 *r, u32 *eax)            { *eax = fighter_477a8(r[R_EAX]); }
static void b_477e8(const u32 *r, u32 *eax)            { fighter_477e8(r[R_EAX]); *eax = 0u; }
#define M47874_LATE 1
#define M47874_EBX  2
#define M47874_SEXT 4
#define M47874_EARLY 8
static void m_47874_at(const u32 *r, int how)
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u32 side = (how & M47874_SEXT) ? (u32)(s32)(s8)DSB(rec + 0x51u) : (u32)DSB(rec + 0x51u);
    if (how & M47874_EARLY) {
        DSB(slot + 0x53u) = 7u;
        DSB(slot + 0x54u) = 0u;
        DSB(slot + 0x52u) = 9u;
    }
    hit_anim_start_b(rec, 0x000ED974u, 0x40000000u);
    if (!(how & M47874_EARLY)) {
        DSB(slot + 0x53u) = 7u;
        DSB(slot + 0x54u) = 0u;
        DSB(slot + 0x52u) = 9u;
    }
    if (!(how & M47874_LATE)) {
        DSD(slot + 0x0Cu) = 0x00047830u;
        DSD(slot + 0x18u) = 0x000477A8u;
        DSD(slot + 0x1Cu) = 0x000477E8u;
        DSD(slot + 0x14u) = 0x00047798u;
    }
    fighter_3c190((how & M47874_EBX) ? r[R_EBX] : side, 0x80u);
    if (how & M47874_LATE) {
        DSD(slot + 0x0Cu) = 0x00047830u;
        DSD(slot + 0x18u) = 0x000477A8u;
        DSD(slot + 0x1Cu) = 0x000477E8u;
        DSD(slot + 0x14u) = 0x00047798u;
    }
    (void)sound_voice(0x4Bu);
}
static void m_47874(const u32 *r, u32 *eax)            /* the four callbacks stored after 0x3C190 */
{
    m_47874_at(r, M47874_LATE);
    *eax = 0u;
}
static void m_47874_side(const u32 *r, u32 *eax)       /* 0x3C190 on EBX, not rec+0x51 */
{
    m_47874_at(r, M47874_EBX);
    *eax = 0u;
}
static void m_47874_sext(const u32 *r, u32 *eax)       /* rec+0x51 read sign-extended (v2's 0x80) */
{
    m_47874_at(r, M47874_SEXT);
    *eax = 0u;
}
static void m_47874_early(const u32 *r, u32 *eax)      /* +0x52/+0x53/+0x54 stored before the 0x3C4CC call */
{
    m_47874_at(r, M47874_EARLY);
    *eax = 0u;
}
static void m_47830_at(const u32 *r, int any, int by_ebx, int early, int sext)
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u32 idx = sext ? (u32)(s32)(s8)DSB(rec + 0x51u) : (u32)DSB(rec + 0x51u);
    u32 w = DSW(0x001088E0u + (by_ebx ? r[R_EBX] : idx) * 2u) & 0x0900u;
    if (any ? w != 0u : w == 0x0900u) return;
    if (early) {
        DSD(slot + 0x0Cu) = 0u;
        DSD(slot + 0x14u) = 0u;
    }
    actors_anim_begin(rec, 0x000ED9A4u, 0x40000000u);
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x14u) = 0u;
}
static void m_47830(const u32 *r, u32 *eax)            /* either bit skips */
{
    m_47830_at(r, 1, 0, 0, 0);
    *eax = 0u;
}
static void m_47830_side(const u32 *r, u32 *eax)       /* the command word by EBX, not rec+0x51 */
{
    m_47830_at(r, 0, 1, 0, 0);
    *eax = 0u;
}
static void m_47830_order(const u32 *r, u32 *eax)      /* +0x0C/+0x14 zeroed before the 0x2BC30 call */
{
    m_47830_at(r, 0, 0, 1, 0);
    *eax = 0u;
}
static void m_47830_sext(const u32 *r, u32 *eax)       /* the command word by the sign-extended rec+0x51 */
{
    m_47830_at(r, 0, 0, 0, 1);
    *eax = 0u;
}
static void m_47798(const u32 *r, u32 *eax)            /* the voice 0x4B */
{
    (void)r;
    (void)sound_voice(0x4Bu);
    *eax = 1u;
}
static void m_47798_eax(const u32 *r, u32 *eax)        /* the voice's EAX returned */
{
    (void)r;
    *eax = sound_voice(0x4Cu);
}
static void m_477a8(const u32 *r, u32 *eax)            /* flag 8 left at 2 */
{
    u32 ctx[6], k;
    u8 f[16];
    fighter_ctx_same(ctx, r[R_EAX]);
    for (k = 0; k < 16u; k++) f[k] = 2u;
    f[1] = 0;
    f[0] = 1u;
    *eax = (u32)fighter_18c14(ctx[0], f, 0u, 0u);
}
static void m_477e8_at(u32 side, int swapped, int early)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);
    if (swapped) fighter_reaction(ctx[2], ctx[3]);
    else fighter_reaction(ctx[3], ctx[2]);
    if (early) {
        DSD(ctx[2] + 0x0Cu) = 0u;
        DSD(ctx[2] + 0x14u) = 0u;
    }
    actors_anim_begin(ctx[4], 0x000ED9A4u, 0x40000000u);
    DSD(ctx[2] + 0x0Cu) = 0u;
    DSD(ctx[2] + 0x14u) = 0u;
}
static void m_477e8(const u32 *r, u32 *eax)            /* 0x3B714's two slots swapped */
{
    m_477e8_at(r[R_EAX], 1, 0);
    *eax = 0u;
}
static void m_477e8_order(const u32 *r, u32 *eax)      /* +0x0C/+0x14 zeroed before the 0x2BC30 call */
{
    m_477e8_at(r[R_EAX], 0, 1);
    *eax = 0u;
}

/* §P3.6: 0x47FCC (0x34E2C at 0x35045) and its +0x0C callback 0x47E9C (0x3531C case 7), mask 0; the +0x18 hook
 * 0x47CB0 (the whole EAX); the +0x1C callback 0x47D24 (mask 0). */
static void b_47fcc(const u32 *r, u32 *eax)            { fighter_47fcc(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_47cb0(const u32 *r, u32 *eax)            { *eax = fighter_47cb0(r[R_EAX]); }
static void b_47d24(const u32 *r, u32 *eax)            { fighter_47d24(r[R_EAX]); *eax = 0u; }
static void b_47e9c(const u32 *r, u32 *eax)            { fighter_47e9c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_47fcc_at(u32 side, int other_char, int late)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);
    if (!late) DSD(0x00108370u + ctx[0] * 4u) = 0u;
    hit_anim_start_b(ctx[4], DSD(0x000C8950u + (u32)DSB(ctx[other_char ? 3 : 2] + 0x7Au) * 4u), 0x40000000u);
    if (late) DSD(0x00108370u + ctx[0] * 4u) = 0u;
    DSB(ctx[2] + 0x53u) = 7u;
    DSB(ctx[2] + 0x52u) = 9u;
    DSB(ctx[2] + 0x54u) = 0u;
    DSD(ctx[2] + 0x0Cu) = 0x00047E9Cu;
    DSD(ctx[2] + 0x18u) = 0x00047CB0u;
    DSD(ctx[2] + 0x1Cu) = 0x00047D24u;
    DSB(ctx[2] + 0x57u) = 0u;
    DSB(ctx[2] + 0x41u) = (u8)(DSB(ctx[2] + 0x41u) | 0x80u);
}
static void m_47fcc(const u32 *r, u32 *eax)            /* the stream by the other slot's character */
{
    m_47fcc_at(r[R_EBX], 1, 0);
    *eax = 0u;
}
static void m_47fcc_order(const u32 *r, u32 *eax)      /* the dword 0x108370 zeroed after the 0x3C4CC call */
{
    m_47fcc_at(r[R_EBX], 0, 1);
    *eax = 0u;
}
static u32 m_47cb0_at(u32 side, int mode)
{
    u32 ctx[6], k;
    u8 f[16];
    s32 w;
    fighter_ctx_same(ctx, side);
    for (k = 0; k < 16u; k++) f[k] = 2u;
    f[1] = f[8] = f[4] = f[0xE] = f[7] = 0u;
    f[0xD] = mode == 1 ? 2u : 0u;
    f[5] = 1u;
    w = (s32)DSD(ctx[2] + 0x86u) >> 16;
    if (mode == 2 ? (w >= 3 || w < 1) : mode == 3 ? (w > 3 || w <= 1) : (w > 3 || w < 1)) return 1u;
    return (u32)fighter_18c14(ctx[0], f, 0x000C946Au, 0x000C9474u);
}
static void m_47cb0(const u32 *r, u32 *eax)            /* flag 0xD left at 2 */
{
    *eax = m_47cb0_at(r[R_EAX], 1);
}
static void m_47cb0_ge(const u32 *r, u32 *eax)         /* `>= 3` for `jg` */
{
    *eax = m_47cb0_at(r[R_EAX], 2);
}
static void m_47cb0_lo(const u32 *r, u32 *eax)         /* `<= 1` for the `jge` */
{
    *eax = m_47cb0_at(r[R_EAX], 3);
}
static void m_47d24_at(u32 side, int own_char, int early, int zext, int fixed_frame)
{
    u32 ctx[6], w;
    fighter_ctx_same(ctx, side);
    hit_flash_pair(ctx[0]);
    DSB(ctx[2] + 0x42u) = (u8)(DSB(ctx[2] + 0x42u) | 4u);
    actors_anim_begin(ctx[4], 0x000ED9D0u, fixed_frame ? 0x40400000u : DSD(0x00108378u + ctx[0] * 4u));
    hit_anim_start_a(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[own_char ? 2 : 3] + 0x7Au) * 4u), 0x40400000u);
    w = DSW(0x000C947Eu + (u32)DSB(ctx[3] + 0x7Au) * 2u);
    fighter_3c208(ctx[0], zext ? (s32)w : (s32)(s16)w);
    fighter_18af8();
    fighter_39834(ctx[1], (s32)(u32)DSB(ctx[2] + 0x5Fu));
    if (early) {
        DSB(ctx[2] + 0x57u) = 3u;
        DSD(0x00108378u + ctx[0] * 4u) = 0x40400000u;
        DSB(0x00108394u + ctx[0]) = 0u;
    }
    fighter_3c358(ctx[0]);
    DSB(ctx[2] + 0x57u) = 3u;
    DSD(0x00108378u + ctx[0] * 4u) = 0x40400000u;
    DSB(0x00108394u + ctx[0]) = 0u;
    fighter_39a10(ctx[4], 0x309u);
    fighter_39a10(ctx[5], 0x309u);
    (void)sound_voice(0x66u);
}
static void m_47d24(const u32 *r, u32 *eax)            /* the other record's stream by the own character */
{
    m_47d24_at(r[R_EAX], 1, 0, 0, 0);
    *eax = 0u;
}
static void m_47d24_order(const u32 *r, u32 *eax)      /* +0x57, the float and the byte before 0x3C358 */
{
    m_47d24_at(r[R_EAX], 0, 1, 0, 0);
    *eax = 0u;
}
static void m_47d24_signed(const u32 *r, u32 *eax)     /* the distance word zero-extended */
{
    m_47d24_at(r[R_EAX], 0, 0, 1, 0);
    *eax = 0u;
}
static void m_47d24_frame(const u32 *r, u32 *eax)      /* the frame 3.0, not the side's float */
{
    m_47d24_at(r[R_EAX], 0, 0, 0, 1);
    *eax = 0u;
}
static void m_47e9c_at(const u32 *r, int up, int by_ctx, int uns, int early)
{
    u32 ctx[6], side = r[R_EBX], n;
    fighter_ctx_same(ctx, side);
    n = DSD(0x00108370u + ctx[0] * 4u) + 1u;
    DSD(0x00108370u + ctx[0] * 4u) = n;
    if (uns ? n > 0x3Cu : (s32)n > 0x3C) DSB(0x00108394u + ctx[0]) = 1u;
    switch (DSB((by_ctx ? ctx[2] : r[R_EAX]) + 0x57u)) {
    case 0u:
        if ((s32)DSD(ctx[2] + 0x86u) >> 16 <= 3) return;
        DSB(ctx[2] + 0x57u) = 1u;
        return;
    case 1u:
        if (early) DSB(ctx[2] + 0x57u) = 2u;
        actors_anim_begin(ctx[4], 0x000EDA40u, 0x40000000u);
        DSB(ctx[2] + 0x57u) = 2u;
        DSB(ctx[2] + 0x8Au) = 0u;
        return;
    case 3u: {
        u32 a = 0x00108378u + ctx[0] * 4u, cmd = (u32)DSW(0x001088E0u + side * 2u);
        union { float f; u32 u; } v;
        double d;
        v.u = DSD(a);
        if ((cmd & 1u) != 0u) {
            memcpy(&d, mem + (up ? 0x00080C64u : 0x00080C6Cu), sizeof d);
            v.f = (float)((double)v.f + d);
            DSD(a) = v.u;
        } else if ((cmd & 2u) != 0u) {
            memcpy(&d, mem + 0x00080C64u, sizeof d);
            v.f = (float)((double)v.f + d);
            DSD(a) = v.u;
        }
        memcpy(&d, mem + 0x00080C74u, sizeof d);
        if (!((double)v.f >= d)) { DSD(a) = 0x3F8CCCCDu; return; }
        if (v.f > 5.0f) DSD(a) = 0x40A00000u;
        return;
    }
    default:
        return;
    }
}
static void m_47e9c(const u32 *r, u32 *eax)            /* +0.1 on the command's bit 0 */
{
    m_47e9c_at(r, 1, 0, 0, 0);
    *eax = 0u;
}
static void m_47e9c_slot(const u32 *r, u32 *eax)       /* the state read from ctx[2], not the EAX slot */
{
    m_47e9c_at(r, 0, 1, 0, 0);
    *eax = 0u;
}
static void m_47e9c_signed(const u32 *r, u32 *eax)     /* the count compared unsigned */
{
    m_47e9c_at(r, 0, 0, 1, 0);
    *eax = 0u;
}
static void m_47e9c_order(const u32 *r, u32 *eax)      /* state 1's +0x57 = 2 before the 0x2BC30 call */
{
    m_47e9c_at(r, 0, 0, 0, 1);
    *eax = 0u;
}

/* §P3.7: 0x48608 (0x34E2C at 0x35045), mask 0; the +0x18 hook 0x48054 (the whole EAX); the +0x1C callback 0x480B4
 * and its callee 0x48170 (EAX = side; 0x480FB reloads EAX), mask 0; the +0x10 handler 0x4811C as 0x3531C case 10
 * calls it (EAX = slot, EDX = rec, EBX = side), mask 0. */
static void b_48608(const u32 *r, u32 *eax)            { fighter_48608(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_48054(const u32 *r, u32 *eax)            { *eax = fighter_48054(r[R_EAX]); }
static void b_480b4(const u32 *r, u32 *eax)            { fighter_480b4(r[R_EAX]); *eax = 0u; }
static void b_48170(const u32 *r, u32 *eax)            { fighter_48170(r[R_EAX]); *eax = 0u; }
static void b_4811c(const u32 *r, u32 *eax)            { fighter_4811c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_48608_at(const u32 *r, u32 speed, int late, int by_ebx)
{
    u32 slot = r[R_EAX], rec = r[R_EDX], s = (u32)DSB(rec + 0x51u);
    hit_anim_start_b(rec, 0x000ED834u, 0x40000000u);
    if (!late) fighter_3c190(s, speed);
    DSB(rec + 0x42u) = 0x1Eu;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x54u) = 0u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x0Cu) = 0x0004844Cu;
    DSW(0x0010838Cu + (by_ebx ? r[R_EBX] : s) * 2u) = 0u;
    DSD(slot + 0x18u) = 0x00048054u;
    DSD(slot + 0x1Cu) = 0x000480B4u;
    if (late) fighter_3c190(s, speed);
}
static void m_48608(const u32 *r, u32 *eax)            /* 0x3C190 with 0x80 (0x47874's) */
{
    m_48608_at(r, 0x80u, 0, 0);
    *eax = 0u;
}
static void m_48608_order(const u32 *r, u32 *eax)      /* the stores before 0x3C190 */
{
    m_48608_at(r, 0x78u, 1, 0);
    *eax = 0u;
}
static void m_48608_side(const u32 *r, u32 *eax)       /* the word 0x10838C by EBX, not rec+0x51 */
{
    m_48608_at(r, 0x78u, 0, 1);
    *eax = 0u;
}
static u32 m_48054_at(u32 side, int mode)
{
    u32 ctx[6], k, v;
    u8 f[16];
    fighter_ctx_same(ctx, side);
    for (k = 0; k < 16u; k++) f[k] = 2u;
    f[1] = f[8] = f[4] = f[0xD] = 0u;
    f[5] = 1u;
    f[9] = mode == 1 ? 2u : 1u;
    v = (u32)fighter_18c14(ctx[0], f, 0x000C9492u, 0x000C949Cu);
    if (mode != 2 && DSB(ctx[2] + 0x57u) != 0u) v = 1u;
    return v;
}
static void m_48054(const u32 *r, u32 *eax)            /* flag 9 left at 2 */
{
    *eax = m_48054_at(r[R_EAX], 1);
}
static void m_48054_eax(const u32 *r, u32 *eax)        /* 0x18C14's result even with +0x57 set */
{
    *eax = m_48054_at(r[R_EAX], 2);
}
static void m_480b4_at(u32 side, int own_side, int zext, int own_char)
{
    u32 ctx[6], w;
    fighter_ctx_same(ctx, side);
    (void)fighter_command_dispatch(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));
    fighter_39a10(ctx[4], 0x309u);
    fighter_39a10(ctx[5], 0x309u);
    fighter_48170(ctx[0]);
    w = DSW(0x000C94A6u + (u32)DSB(ctx[own_char ? 2 : 3] + 0x7Au) * 2u);
    fighter_3c208(ctx[own_side ? 0 : 1], zext ? (s32)w : (s32)(s16)w);
}
static void m_480b4(const u32 *r, u32 *eax)            /* 0x3C208 on the own side */
{
    m_480b4_at(r[R_EAX], 1, 0, 0);
    *eax = 0u;
}
static void m_480b4_signed(const u32 *r, u32 *eax)     /* the distance word zero-extended */
{
    m_480b4_at(r[R_EAX], 0, 1, 0);
    *eax = 0u;
}
static void m_480b4_char(const u32 *r, u32 *eax)       /* the distance by the own slot's character */
{
    m_480b4_at(r[R_EAX], 0, 0, 1);
    *eax = 0u;
}
static void m_48170_at(u32 side, int own_x, int late57, int own_reset, int whole_eax)
{
    u32 ctx[6], x, other = 1u - side;
    u32 own_s = 0x001077B0u + side * 0x94u, oth_s = 0x001077B0u + other * 0x94u;
    fighter_ctx_same(ctx, side);
    DSW(0x00108388u + side * 2u) = 0u;
    fighter_3c148(side);
    fighter_3c148(other);
    hit_anim_start_a(DSD(own_s), 0x000ED850u, 0x40400000u);
    if (!late57) DSB(own_s + 0x57u) = 2u;
    if ((whole_eax ? (u32)ai_pred_468d8(other) : (u32)(u8)ai_pred_468d8(other)) != 0u)
        fighter_36d98(own_reset ? own_s : oth_s);
    if (late57) DSB(own_s + 0x57u) = 2u;
    x = DSD(ctx[own_x ? 2 : 3] + 0x2Cu);
    hit_anim_start_a(DSD(oth_s), DSD(0x000C90F8u + (u32)DSB(oth_s + 0x7Au) * 4u), 0x40400000u);
    hit_anchor_x(ctx[1], x);
    DSB(oth_s + 0x52u) = 0x10u;
    DSB(oth_s + 0x53u) = 0x0Au;
    DSB(oth_s + 0x54u) = 0u;
    DSD(oth_s + 0x10u) = 0x0004811Cu;
    DSB(oth_s + 0x58u) = 0u;
    DSB(0x00108392u + side) = (DSB(oth_s + 0x43u) & 0x30u) != 0u ? 1u : 0u;
}
static void m_48170(const u32 *r, u32 *eax)            /* 0x188DC with the own slot's +0x2C */
{
    m_48170_at(r[R_EAX], 1, 0, 0, 0);
    *eax = 0u;
}
static void m_48170_order(const u32 *r, u32 *eax)      /* the own +0x57 = 2 after 0x468D8 */
{
    m_48170_at(r[R_EAX], 0, 1, 0, 0);
    *eax = 0u;
}
static void m_48170_reset(const u32 *r, u32 *eax)      /* 0x36D98 on the own slot */
{
    m_48170_at(r[R_EAX], 0, 0, 1, 0);
    *eax = 0u;
}
static void m_48170_al(const u32 *r, u32 *eax)         /* 0x468D8's whole EAX tested, not AL */
{
    m_48170_at(r[R_EAX], 0, 0, 0, 1);
    *eax = 0u;
}
static void m_4811c_at(const u32 *r, int on_slot, int uns, int late54)
{
    u32 slot = r[R_EAX], a = 0x00108380u + r[R_EBX] * 2u;
    u8 st = DSB(slot + 0x58u);
    if (st < 1u) return;
    if (st == 1u) {
        DSW(a) = 0u;
        DSB(slot + 0x58u) = 2u;
        return;
    }
    if (st != 2u) return;
    DSW(a) = (u16)(DSW(a) + 1u);
    if (uns ? DSW(a) <= 0x0Fu : (s32)(s16)DSW(a) <= 0x0F) return;
    if (!late54) DSB(slot + 0x54u) = 0u;
    fighter_36870(on_slot ? slot : r[R_EDX]);
    if (late54) DSB(slot + 0x54u) = 0u;
}
static void m_4811c(const u32 *r, u32 *eax)            /* 0x36870 on the slot, not the record */
{
    m_4811c_at(r, 1, 0, 0);
    *eax = 0u;
}
static void m_4811c_signed(const u32 *r, u32 *eax)     /* the count compared unsigned */
{
    m_4811c_at(r, 0, 1, 0);
    *eax = 0u;
}
static void m_4811c_order(const u32 *r, u32 *eax)      /* +0x54 = 0 after the 0x36870 call */
{
    m_4811c_at(r, 0, 0, 1);
    *eax = 0u;
}

/* §P3.8: the +0x0C callback 0x4844C (0x3531C case 7), mask 0. */
static void b_4844c(const u32 *r, u32 *eax)            { fighter_4844c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_4844c_at(u32 side, int swap, int uns, int noabs, int late54, int ubound, int zext, int wide)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);
    DSW(0x0010838Cu + ctx[0] * 2u) = (u16)(DSW(0x0010838Cu + ctx[0] * 2u) + 1u);
    DSW(0x00108388u + ctx[0] * 2u) = (u16)(DSW(0x00108388u + ctx[0] * 2u) + 1u);
    switch (DSB(ctx[2] + 0x57u)) {
    case 0u: {
        s32 v = (s32)(s16)DSW(ctx[4] + 0x34u);
        u16 c = DSW(0x0010838Cu + ctx[0] * 2u);
        if (!noabs && v < 0) v = -v;
        if (v > 0x15E) DSB(ctx[4] + 0x42u) = 0u;
        if (uns ? c <= 0x1Eu : (s32)(s16)c <= 0x1E) return;
        DSB(ctx[2] + 0x54u) = 0u;
        fighter_36870(ctx[4]);
        return;
    }
    case 2u: {
        int hit = (DSB(ctx[3] + 0x43u) & 0x30u) != 0u;
        u32 t = (hit != swap) ? 0x000C94CEu : 0x000C94BAu, e;
        s32 n = (s32)(s16)DSW(0x00108388u + ctx[0] * 2u);
        for (e = t; e != t + 0x14u; e += 4u)
            if ((s32)(s16)DSW(e) == n) (void)sound_voice((u32)DSW(e + 2u));
        return;
    }
    case 3u: {
        s32 w;
        u16 x;
        if (wide) DSD(ctx[2] + 0x74u) = 0u; else DSW(ctx[2] + 0x74u) = 0u;
        DSB(ctx[4] + 0x28u) = (u8)(DSB(ctx[4] + 0x28u) | 0x20u);
        DSW(0x00108384u + ctx[0] * 2u) = DSW(ctx[2] + 0x2Cu);
        w = (s32)(s16)DSW(0x000BD884u + (u32)DSB(ctx[2] + 0x7Au) * 2u);
        if (ubound ? (u32)w <= DSD(ctx[2] + 0x30u) : w <= (s32)DSD(ctx[2] + 0x30u)) return;
        if (!late54) DSB(ctx[2] + 0x54u) = 0u;
        fighter_3c148(ctx[0]);
        if (late54) DSB(ctx[2] + 0x54u) = 0u;
        fighter_3c16c(ctx[0]);
        hit_anchor_set(ctx[0], DSD(ctx[4] + 0x18u), 0u);
        actors_anim_begin(ctx[4], DSD(0x000C8B58u + (u32)DSB(ctx[2] + 0x7Au) * 4u), 0x40400000u);
        x = DSW(0x00108384u + ctx[0] * 2u);
        hit_anchor_x(ctx[0], zext ? (u32)x : (u32)(s32)(s16)x);
        DSB(ctx[2] + 0x57u) = 4u;
        return;
    }
    default:
        return;
    }
}
static void m_4844c(const u32 *r, u32 *eax)            /* the two voice tables swapped */
{
    m_4844c_at(r[R_EBX], 1, 0, 0, 0, 0, 0, 0);
    *eax = 0u;
}
static void m_4844c_signed(const u32 *r, u32 *eax)     /* the count 0x10838C compared unsigned */
{
    m_4844c_at(r[R_EBX], 0, 1, 0, 0, 0, 0, 0);
    *eax = 0u;
}
static void m_4844c_abs(const u32 *r, u32 *eax)        /* +0x34 compared without its absolute value */
{
    m_4844c_at(r[R_EBX], 0, 0, 1, 0, 0, 0, 0);
    *eax = 0u;
}
static void m_4844c_order(const u32 *r, u32 *eax)      /* +0x54 = 0 after 0x3C148 */
{
    m_4844c_at(r[R_EBX], 0, 0, 0, 1, 0, 0, 0);
    *eax = 0u;
}
static void m_4844c_bound(const u32 *r, u32 *eax)      /* the slot's +0x30 compared unsigned */
{
    m_4844c_at(r[R_EBX], 0, 0, 0, 0, 1, 0, 0);
    *eax = 0u;
}
static void m_4844c_zext(const u32 *r, u32 *eax)       /* 0x188DC's word zero-extended */
{
    m_4844c_at(r[R_EBX], 0, 0, 0, 0, 0, 1, 0);
    *eax = 0u;
}
static void m_4844c_width(const u32 *r, u32 *eax)      /* the word +0x74 cleared as a dword (+0x76/+0x77 too) */
{
    m_4844c_at(r[R_EBX], 0, 0, 0, 0, 0, 0, 1);
    *eax = 0u;
}

static void b_18bc8(const u32 *r, u32 *eax)      { fighter_18bc8(r[R_EAX]); *eax = 0u; }
static void b_21084(const u32 *r, u32 *eax)      { fighter_21084(r[R_EAX]); *eax = 0u; }
static void b_400e0(const u32 *r, u32 *eax)      { fighter_400e0(r[R_EAX]); *eax = 0u; }
static void b_21044(const u32 *r, u32 *eax)      { fighter_21044(r[R_EAX]); *eax = 0u; }
static void b_1549c(const u32 *r, u32 *eax)      { fighter_1549c(r[R_EAX]); *eax = 0u; }
static void b_154e8(const u32 *r, u32 *eax)      { fighter_154e8(r[R_EAX]); *eax = 0u; }
static void b_229e8(const u32 *r, u32 *eax)      { fighter_229e8(r[R_EAX]); *eax = 0u; }
static void b_243f8(const u32 *r, u32 *eax)      { fighter_243f8(r[R_EAX]); *eax = 0u; }
static void m_18bc8(const u32 *r, u32 *eax)      { (void)r; DSB(0x00100C1Du) = 1u; *eax = 0u; }
static void m_21084(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u);
    if (held == 0u) return;
    DSD(rec + 0x1Cu) = 0u; DSW(rec + 0x34u) = 0u; DSB(held + 0x54u) = 0u; DSB(held + 0x57u) = 3u;
    *eax = 0u;
}
static void m_21084_byte14(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX];
    if (DSB(rec + 0x14u) == 0u) return;
    DSD(rec + 0x1Cu) = 0u; DSW(rec + 0x34u) = 0u; DSB(DSD(rec + 0x14u) + 0x54u) = 0u;
    DSB(DSD(rec + 0x14u) + 0x57u) = 2u;
    *eax = 0u;
}
static void m_400e0(const u32 *r, u32 *eax)
{
    u32 held = DSD(r[R_EAX] + 0x14u);
    if (held != 0u) DSB(held + 0x42u) &= (u8)~0x02u;
    *eax = 0u;
}
static void m_21044(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u);
    if (held == 0u) return;
    DSW(rec + 0x34u) = 0x0258u; DSW(rec + 0x36u) = 0x0097u; DSW(rec + 0x44u) = 0x000Fu;
    DSB(rec + 0x43u) = 0x0Fu;
    if (DSB(rec + 0x51u) == 0u) DSW(rec + 0x34u) = (u16)(0u - DSW(rec + 0x34u));
    DSB(held + 0x57u) = 1u;
    *eax = 0u;
}
static void m_21044_neg(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u);
    if (held == 0u) return;
    DSW(rec + 0x34u) = 0x0258u; DSW(rec + 0x36u) = 0x0096u; DSW(rec + 0x44u) = 0x000Fu;
    DSB(rec + 0x43u) = 0x0Fu;
    DSB(held + 0x57u) = 1u;
    *eax = 0u;
}
static void m_1549c(const u32 *r, u32 *eax)
{
    u32 other = (u32)DSB(r[R_EAX] + 0x51u) ^ 1u, slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    actors_anim_begin(DSD(slot), DSD(0x0009B01Cu + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
    actor_pset_palette(DSD(slot), 0u, 0x1F874590u);
    sound_voice(0x51u);
    *eax = 0u;
}
static void m_1549c_side(const u32 *r, u32 *eax)
{
    u32 other = ((u32)DSB(r[R_EAX] + 0x51u) & 1u) ^ 1u, slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    actors_anim_begin(DSD(slot), DSD(0x0009B01Cu + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
    actor_pset_palette(DSD(slot), 0u, 0x1F874590u);
    sound_voice(0x50u);
    *eax = 0u;
}
static void m_154e8(const u32 *r, u32 *eax)
{
    u32 other = (u32)DSB(r[R_EAX] + 0x51u) ^ 1u, slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    DSD(DSD(slot) + 0x24u) = 0x40C00001u;
    sound_voice(0xD2u);
    *eax = 0u;
}
static void m_154e8_side(const u32 *r, u32 *eax)
{
    u32 other = ((u32)DSB(r[R_EAX] + 0x51u) & 1u) ^ 1u, slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    DSD(DSD(slot) + 0x24u) = 0x40C00000u;
    sound_voice(0xD2u);
    *eax = 0u;
}
static void m_229e8(const u32 *r, u32 *eax)      { actors_anim_begin(r[R_EAX], 0x000E1564u, 0x40400000u); *eax = 0u; }
static void m_243f8(const u32 *r, u32 *eax)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, (u32)DSB(r[R_EAX] + 0x51u));
    actors_anim_begin(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[2] + 0x7Au) * 4u), 0x40000000u);
    DSB(ctx[3] + 0x53u) = 0x0Au; DSB(ctx[3] + 0x52u) = 9u; DSB(ctx[3] + 0x54u) = 0u;
    DSD(ctx[3] + 0x10u) = 0u;
    *eax = 0u;
}
static void m_243f8_slot(const u32 *r, u32 *eax)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, (u32)DSB(r[R_EAX] + 0x51u));
    actors_anim_begin(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40000000u);
    DSB(ctx[2] + 0x53u) = 0x0Au; DSB(ctx[2] + 0x52u) = 9u; DSB(ctx[2] + 0x54u) = 0u;
    DSD(ctx[2] + 0x10u) = 0u;
    *eax = 0u;
}

static void b_15510(const u32 *r, u32 *eax)      { fighter_15510(r[R_EAX]); *eax = 0u; }
static void b_241a8(const u32 *r, u32 *eax)      { fighter_241a8(r[R_EAX]); *eax = 0u; }
static void b_40358(const u32 *r, u32 *eax)      { fighter_40358(r[R_EAX]); *eax = 0u; }
static void b_45c54(const u32 *r, u32 *eax)      { fighter_45c54(r[R_EAX]); *eax = 0u; }
static void b_489dc(const u32 *r, u32 *eax)      { fighter_489dc(r[R_EAX]); *eax = 0u; }
static void b_400ec(const u32 *r, u32 *eax)      { fighter_400ec(r[R_EAX]); *eax = 0u; }
static void m_15510_at(const u32 *r, u32 desc, u32 side, u32 a5)
{
    u32 rec = r[R_EAX], child;
    DSD(0x0009B08Cu) = fighter_29c08(side, (u32)DSB(0x0010782Au + side * 0x94u));
    child = actor_spawn((const u32 *)(mem + desc), 0u, 0u, 0u, a5);
    DSB(child + 0x59u) = 2u;
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
}
static void m_15510(const u32 *r, u32 *eax)
{
    m_15510_at(r, 0x0009B080u, (u32)DSB(r[R_EAX] + 0x51u), (u32)(u16)(DSW(r[R_EAX] + 0x56u) | 0x0400u));
    *eax = 0u;
}
static void m_15510_side(const u32 *r, u32 *eax)
{
    m_15510_at(r, 0x0009B07Cu, (u32)DSB(r[R_EAX] + 0x51u) & 1u, (u32)(u16)(DSW(r[R_EAX] + 0x56u) | 0x0400u));
    *eax = 0u;
}
static void m_15510_pal(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], side = (u32)DSB(rec + 0x51u), child;
    DSD(0x0009B090u) = fighter_29c08(side, (u32)DSB(0x0010782Au + side * 0x94u));
    child = actor_spawn((const u32 *)(mem + 0x0009B07Cu), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(child + 0x59u) = 2u;
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void m_15510_a5(const u32 *r, u32 *eax)
{
    m_15510_at(r, 0x0009B07Cu, (u32)DSB(r[R_EAX] + 0x51u), (u32)(u16)DSW(r[R_EAX] + 0x56u));
    *eax = 0u;
}
static void m_241a8(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], side = (u32)DSB(rec + 0x51u), slot = DSD(0x001077A8u + side * 4u), child;
    if (slot == 0u) return;
    child = actor_spawn((const u32 *)(mem + DSD(0x000A84E4u + (u32)DSB(slot + 0x7Au) * 4u)),
                        0u, 0u, 0u, (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void m_241a8_sext(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], side = (u32)(s32)(s8)DSB(rec + 0x51u), slot = DSD(0x001077A8u + side * 4u), child;
    if (slot == 0u) return;
    child = actor_spawn((const u32 *)(mem + DSD(0x000A84E0u + (u32)DSB(slot + 0x7Au) * 4u)),
                        0u, 0u, 0u, (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void m_40358(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), child;
    if (held == 0u) return;
    child = actor_spawn((const u32 *)(mem + 0x000C7758u), 0xFFFFFFF8u, 0u, 0xFFFFFFF6u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSD(child + 0x14u) = held;
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void m_45c54(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), child;
    if (held == 0u) return;
    child = actor_spawn((const u32 *)(mem + 0x000C9360u), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(child + 0x59u) = 2u;
    DSD(child + 0x14u) = held;
    sound_voice(0x60u);
    *eax = 0u;
}
static void m_489dc(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), hrec, child;
    if (held == 0u) return;
    hrec = DSD(held);
    if (DSB(hrec + 0x4Bu) != 0u) return;
    child = actor_spawn((const u32 *)(mem + 0x000BB13Cu), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(child + 0x59u) = 2u;
    DSB(hrec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void m_400ec(const u32 *r, u32 *eax)
{
    u32 other = (u32)DSB(r[R_EAX] + 0x51u) ^ 1u, slot = DSD(0x001077A8u + other * 4u), ch;
    if (slot == 0u) return;
    ch = (u32)DSB(slot + 0x7Au);
    actors_anim_begin(DSD(slot), DSD(0x000C90F8u + ch * 4u), 0x40000000u);
    sound_voice((u32)DSW(0x000C75AAu + ch * 2u));
    sound_voice(0x5Au);
    DSB(0x00104AE9u) |= 0x04u;
    *eax = 0u;
}
static void m_400ec_side(const u32 *r, u32 *eax)
{
    u32 other = ((u32)DSB(r[R_EAX] + 0x51u) & 1u) ^ 1u, slot = DSD(0x001077A8u + other * 4u), ch;
    if (slot == 0u) return;
    ch = (u32)DSB(slot + 0x7Au);
    actors_anim_begin(DSD(slot), DSD(0x000C90F8u + ch * 4u), 0x40000000u);
    sound_voice((u32)DSW(0x000C75AAu + ch * 2u));
    sound_voice(0x59u);
    DSB(0x00104AE9u) |= 0x04u;
    *eax = 0u;
}
static void b_29c08(const u32 *r, u32 *eax)      { *eax = fighter_29c08(r[R_EAX], r[R_EDX]); }

/* The seven 0xD500 side-record targets: one wrong variant each (the stream, the index mask or the
 * voice), through a shared deliberately-wrong body. */
static void m45_own_anim(u32 rec, u32 stream, u32 frame, u32 voice, int has_voice, int side_mask)
{
    u32 side = (u32)DSB(rec + 0x51u);
    if (side_mask) side &= 1u;
    if (has_voice) sound_voice(voice);
    actors_anim_begin(DSD(0x001077B0u + side * 0x94u), stream, frame);
}

static void b_3427c(const u32 *r, u32 *eax)      { fighter_3427c(r[R_EAX]); *eax = 0u; }
static void b_34308(const u32 *r, u32 *eax)      { fighter_34308(r[R_EAX]); *eax = 0u; }
static void b_3438c(const u32 *r, u32 *eax)      { fighter_3438c(r[R_EAX]); *eax = 0u; }
static void b_34418(const u32 *r, u32 *eax)      { fighter_34418(r[R_EAX]); *eax = 0u; }
static void b_344a4(const u32 *r, u32 *eax)      { fighter_344a4(r[R_EAX]); *eax = 0u; }
static void b_34530(const u32 *r, u32 *eax)      { fighter_34530(r[R_EAX]); *eax = 0u; }
static void b_345bc(const u32 *r, u32 *eax)      { fighter_345bc(r[R_EAX]); *eax = 0u; }
static void m_3427c(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000E76B0u, 0x40400000u, 0x8Cu, 1, 0); *eax = 0u; }
static void m_3427c_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000E76B2u, 0x40400000u, 0x8Cu, 1, 1); *eax = 0u; }
static void m_3427c_voice(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000E76B2u, 0x40400000u, 0x8Du, 1, 0); *eax = 0u; }
static void m_34308(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000E4368u, 0x40400000u, 0u, 0, 0); *eax = 0u; }
static void m_34308_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000E436Au, 0x40400000u, 0u, 0, 1); *eax = 0u; }
static void m_3438c(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000ED354u, 0x3F800000u, 0xA6u, 1, 0); *eax = 0u; }
static void m_3438c_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000ED354u, 0x3F800000u, 0xA6u, 1, 1); *eax = 0u; }
static void m_34418(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000EAF66u, 0x40400000u, 0x85u, 1, 0); *eax = 0u; }
static void m_34418_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000EAF66u, 0x40400000u, 0x85u, 1, 1); *eax = 0u; }
static void m_344a4(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000D4618u, 0x40400000u, 0x86u, 1, 0); *eax = 0u; }
static void m_344a4_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000D461Cu, 0x40400000u, 0x86u, 1, 1); *eax = 0u; }
static void m_34530(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000D299Cu, 0x40400000u, 0x9Du, 1, 0); *eax = 0u; }
static void m_34530_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000D299Cu, 0x40400000u, 0x9Cu, 1, 1); *eax = 0u; }
static void m_345bc(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000E0F62u, 0x3F800000u, 0x9Bu, 1, 0); *eax = 0u; }
static void m_345bc_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000E0F62u, 0x40400000u, 0x9Bu, 1, 1); *eax = 0u; }

static void m_156e0_at(const u32 *r, int side_mask, int unsign, int plus, u32 voice)
{
    u32 rec = r[R_EAX];
    u32 other = (u32)DSB(rec + 0x51u) ^ 1u;
    u32 slot, srec, step;
    if (side_mask) other &= 1u;
    slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    srec = DSD(slot);
    step = unsign ? ((u32)DSD(0x000C9783u) >> 24) : (u32)((s32)DSD(0x000C9783u) >> 24);
    if (fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0) {
        DSD(srec + 0x18u) = plus ? DSD(rec + 0x18u) + step * 0x40u
                                 : DSD(rec + 0x18u) - step * 0x40u;
    } else {
        DSD(srec + 0x18u) = plus ? DSD(rec + 0x18u) - step * 0x40u
                                 : DSD(rec + 0x18u) + step * 0x40u;
    }
    DSD(srec + 0x1Cu) = DSD(rec + 0x1Cu) + (u32)((s32)DSD(0x000C9784u) >> 24) * 0x40u;
    actors_anim_begin(srec, DSD(0x0009B038u + (u32)DSB(slot + 0x7Au) * 4u), 0u);
    DSB(rec + 0x4Bu) = DSB(srec + 0x56u);
    DSB(0x000F0AFEu) = 0u;
    DSB(0x000F0AFFu) = DSB(rec + 0x51u);
    sound_voice(voice);
}
static void b_156e0(const u32 *r, u32 *eax)      { fighter_156e0(r[R_EAX]); *eax = 0u; }
static void b_22ab8(const u32 *r, u32 *eax)      { fighter_22ab8(r[R_EAX]); *eax = 0u; }
static void b_37b70(const u32 *r, u32 *eax)      { fighter_37b70(r[R_EAX]); *eax = 0u; }
static void m_156e0(const u32 *r, u32 *eax)      { m_156e0_at(r, 0, 0, 0, 0xD5u); *eax = 0u; }
static void m_156e0_side(const u32 *r, u32 *eax) { m_156e0_at(r, 1, 0, 0, 0xD6u); *eax = 0u; }
static void m_156e0_signed(const u32 *r, u32 *eax) { m_156e0_at(r, 0, 1, 0, 0xD6u); *eax = 0u; }
static void m_156e0_plus(const u32 *r, u32 *eax) { m_156e0_at(r, 0, 0, 1, 0xD6u); *eax = 0u; }
static void m_22ab8(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), hrec;
    if (held == 0u) return;
    hrec = DSD(held);
    DSD(held + 8u) = DSD(0x00104730u + (u32)DSB(hrec + 0x51u) * 4u);
    DSD(rec + 0x1Cu) += 0x1A00u;
    if (DSB(0x001077B0u + (u32)DSB(rec + 0x51u) * 0x94u) == 0u) {
        DSW(rec + 0x34u) = 0xFF6Au; DSD(rec + 0x18u) -= 0x400u;
    } else {
        DSW(rec + 0x34u) = 0x0097u; DSD(rec + 0x18u) += 0x400u;
    }
    DSW(rec + 0x36u) = 0xFFF0u;
    *eax = 0u;
}
static void m_22ab8_side(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), hrec;
    if (held == 0u) return;
    hrec = DSD(held);
    DSD(held + 8u) = DSD(0x00104730u + (u32)DSB(rec + 0x51u) * 4u);
    DSD(rec + 0x1Cu) += 0x1A00u;
    if (fighter_actor_bit15_clear((u32)DSB(hrec + 0x51u)) != 0) {
        DSW(rec + 0x34u) = 0xFF6Au; DSD(rec + 0x18u) -= 0x400u;
    } else {
        DSW(rec + 0x34u) = 0x0096u; DSD(rec + 0x18u) += 0x400u;
    }
    DSW(rec + 0x36u) = 0xFFF0u;
    *eax = 0u;
}
static void m_22ab8_sext(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), hrec;
    if (held == 0u) return;
    hrec = DSD(held);
    DSD(held + 8u) = DSD(0x00104730u + (u32)(s32)(s8)DSB(hrec + 0x51u) * 4u);
    DSD(rec + 0x1Cu) += 0x1A00u;
    if (fighter_actor_bit15_clear((u32)DSB(hrec + 0x51u)) != 0) {
        DSW(rec + 0x34u) = 0xFF6Au; DSD(rec + 0x18u) -= 0x400u;
    } else {
        DSW(rec + 0x34u) = 0x0096u; DSD(rec + 0x18u) += 0x400u;
    }
    DSW(rec + 0x36u) = 0xFFF0u;
    *eax = 0u;
}
static void m_22ab8_add(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), hrec;
    if (held == 0u) return;
    hrec = DSD(held);
    DSD(held + 8u) = DSD(0x00104730u + (u32)DSB(hrec + 0x51u) * 4u);
    DSD(rec + 0x1Cu) += 0x1A00u;
    if (DSB(0x001077B0u + (u32)DSB(rec + 0x51u) * 0x94u) == 0u) {
        DSW(rec + 0x34u) = 0xFF6Au; DSD(rec + 0x18u) += 0x400u;
    } else {
        DSW(rec + 0x34u) = 0x0096u; DSD(rec + 0x18u) -= 0x400u;
    }
    DSW(rec + 0x36u) = 0xFFF0u;
    *eax = 0u;
}
static void m_22ab8_table(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u);
    if (held == 0u) return;
    DSD(held + 8u) = DSD(0x00104734u);
    DSD(rec + 0x1Cu) += 0x1A00u;
    if (DSB(0x001077B0u + (u32)DSB(rec + 0x51u) * 0x94u) == 0u) {
        DSW(rec + 0x34u) = 0xFF6Au; DSD(rec + 0x18u) -= 0x400u;
    } else {
        DSW(rec + 0x34u) = 0x0096u; DSD(rec + 0x18u) += 0x400u;
    }
    DSW(rec + 0x36u) = 0xFFF0u;
    *eax = 0u;
}
static void m_37b70_at(const u32 *r, int path, int neg, int hrec, u32 a4)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), desc, child;
    u32 ch;
    s32 di;
    u32 a5;
    if (held == 0u) return;
    ch = (u32)DSB(held + 0x7Au);
    di = (s32)(s16)DSW(0x000BDC64u + ch * 2u);
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u) {
        if (!neg) di = (s32)(s16)(u16)(0u - (u32)(u16)di);
        a5 = 0x4000u;
    } else {
        a5 = 0u;
    }
    desc = DSD(0x000BDC48u + ch * 4u);
    if (desc == 0u) return;
    if (path ? !(ch == 0u || ch == 5u) : (ch == 0u || ch == 5u)) {
        child = actor_spawn((const u32 *)(mem + desc), (u32)di, 0u, a4,
                            (u32)(u16)(DSW(rec + 0x56u) | 0x0400u) | a5);
    } else {
        child = actor_spawn((const u32 *)(mem + desc), 0u, 0u, a4,
                            (u32)(u16)(DSW(rec + 0x56u) | 0x0400u) | a5);
        DSB((hrec ? held : DSD(held)) + 0x4Bu) = DSB(child + 0x56u);
    }
    DSB(child + 0x59u) = 1u;
}
static void m_37b70(const u32 *r, u32 *eax)      { m_37b70_at(r, 0, 0, 0, 4u); *eax = 0u; }
static void m_37b70_path(const u32 *r, u32 *eax) { m_37b70_at(r, 1, 0, 0, 0); *eax = 0u; }
static void m_37b70_neg(const u32 *r, u32 *eax)  { m_37b70_at(r, 0, 1, 0, 0); *eax = 0u; }
static void m_37b70_hrec(const u32 *r, u32 *eax) { m_37b70_at(r, 0, 0, 1, 0); *eax = 0u; }
static void m_37b70_sext(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), desc, child;
    u32 ch;
    s32 di;
    if (held == 0u) return;
    ch = (u32)(s32)(s8)DSB(held + 0x7Au);
    di = (s32)(s16)DSW(0x000BDC64u + ch * 2u);
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)
        di = (s32)(s16)(u16)(0u - (u32)(u16)di);
    desc = DSD(0x000BDC48u + ch * 4u);
    if (desc == 0u) return;
    child = actor_spawn((const u32 *)(mem + desc), (u32)di, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(child + 0x59u) = 1u;
    *eax = 0u;
}


static const binding_t k_bindings[] = {
    { "rng_next",                 b_rng_next,     0xFFFFFFFFu },
    { "fighter_slot_flag",        b_slot_flag,    0x000000FFu },
    { "config_credit_spend",      b_credit_spend, 0xFFFFFFFFu },
    { "config_codeword_len",      b_codeword_len, 0xFFFFFFFFu },
    { "rng_next@mutant",          m_rng_next,     0xFFFFFFFFu },
    { "fighter_slot_flag@mutant", m_slot_flag,    0x000000FFu },
    { "config_credit_spend@mutant", m_credit_spend, 0xFFFFFFFFu },
    { "config_codeword_len@mutant", m_codeword_len, 0xFFFFFFFFu },
    { "config_credit_spend@signed", m_credit_spend_signed, 0xFFFFFFFFu },
    { "fighter_3640c",            b_3640c,        0x00000000u },
    { "fighter_37dcc",            b_37dcc,        0x00000000u },
    { "fighter_3640c@mutant",     m_3640c,        0x00000000u },
    { "fighter_37dcc@mutant",     m_37dcc,        0x00000000u },
    { "fighter_23130",            b_23130,        0x000000FFu },
    { "fighter_23130@voice",      m_23130_voice,  0x000000FFu },
    { "fighter_23130@novoice",    m_23130_novoice, 0x000000FFu },
    { "fighter_23130@reorder",    m_23130_reorder, 0x000000FFu },
    { "fighter_45878",            b_45878,        0x00000000u },
    { "anim_10fa8",               b_10fa8,        0x00000000u },
    { "anim_3e4e4",               b_3e4e4,        0x00000000u },
    { "fighter_ctx_same",         b_ctx_same,     0x00000000u },
    { "hit_anim_ctx",             b_anim_ctx,     0x00000000u },
    { "hit_anim_start_b",         b_3c4cc,        0x00000000u },
    { "fn_resolved",              b_fn_resolved,  0xFFFFFFFFu },
    { "hit_anim_start_b@mutant",  m_3c4cc,        0x00000000u },
    { "hit_anim_start_b@set",     m_3c4cc_set,    0x00000000u },
    { "fighter_45878@mutant",     m_45878,        0x00000000u },
    { "anim_10fa8@mutant",        m_10fa8,        0x00000000u },
    { "anim_3e4e4@mutant",        m_3e4e4,        0x00000000u },
    { "fighter_ctx_same@mutant",  m_ctx_same,     0x00000000u },
    { "hit_anim_ctx@mutant",      m_anim_ctx,     0x00000000u },
    { "fighter_1567c",            b_1567c,        0x000000FFu },
    { "fighter_15908",            b_15908,        0x000000FFu },
    { "fighter_23ec0",            b_23ec0,        0x000000FFu },
    { "fighter_45d14",            b_45d14,        0x000000FFu },
    { "fighter_1567c@mutant",     m_1567c,        0x000000FFu },
    { "fighter_15908@mutant",     m_15908,        0x000000FFu },
    { "fighter_23ec0@mutant",     m_23ec0,        0x000000FFu },
    { "fighter_45d14@mutant",     m_45d14,        0x000000FFu },
    { "fighter_actor_bit15_clear", b_1a570,       0x000000FFu },
    { "fighter_23bf8",            b_23bf8,        0xFFFFFFFFu },
    { "fighter_402fc",            b_402fc,        0x000000FFu },
    { "fighter_actor_bit15_clear@mutant", m_1a570, 0x000000FFu },
    { "fighter_23bf8@mutant",     m_23bf8,        0xFFFFFFFFu },
    { "fighter_23bf8@zero",       m_23bf8_zero,   0xFFFFFFFFu },
    { "fighter_23bf8@ne",         m_23bf8_ne,     0xFFFFFFFFu },
    { "fighter_402fc@mutant",     m_402fc,        0x000000FFu },
    { "fighter_15584",            b_15584,        0x00000000u },
    { "fighter_1579c",            b_1579c,        0x00000000u },
    { "fighter_23d38",            b_23d38,        0x00000000u },
    { "fighter_15584@mutant",     m_15584,        0x00000000u },
    { "fighter_1579c@mutant",     m_1579c,        0x00000000u },
    { "fighter_23d38@mutant",     m_23d38,        0x00000000u },
    { "fighter_38034",            b_38034,        0x00000000u },
    { "fighter_23b68",            b_23b68,        0x00000000u },
    { "fighter_401d4",            b_401d4,        0x00000000u },
    { "fighter_38034@mutant",     m_38034,        0x00000000u },
    { "fighter_23b68@mutant",     m_23b68,        0x00000000u },
    { "fighter_401d4@mutant",     m_401d4,        0x00000000u },
    { "fighter_401d4@no36",       m_401d4_no36,   0x00000000u },
    { "fighter_23d38@noneg",      m_23d38_noneg,  0x00000000u },
    { "fighter_23d38@ge",         m_23d38_ge,     0x00000000u },
    { "fighter_23d38@unsigned",   m_23d38_unsigned, 0x00000000u },
    { "fighter_23d38@bit",        m_23d38_bit,    0x00000000u },
    { "fighter_15584@ge",         m_15584_ge,     0x00000000u },
    { "fighter_1579c@ge",         m_1579c_ge,     0x00000000u },
    { "fighter_15584@no42",       m_15584_no42,   0x00000000u },
    { "fighter_1579c@no42",       m_1579c_no42,   0x00000000u },
    { "fighter_15584@own4",       m_15584_own4,   0x00000000u },
    { "fighter_23d38@noand",      m_23d38_noand,  0x00000000u },
    { "fighter_156d4",            b_156d4,        0x00000000u },
    { "fighter_23ca4",            b_23ca4,        0x00000000u },
    { "fighter_23868",            b_23868,        0x00000000u },
    { "fighter_3f174",            b_3f174,        0x00000000u },
    { "fighter_156d4@mutant",     m_156d4,        0x00000000u },
    { "fighter_23ca4@mutant",     m_23ca4,        0x00000000u },
    { "fighter_23868@mutant",     m_23868,        0x00000000u },
    { "fighter_3f174@mutant",     m_3f174,        0x00000000u },
    { "fighter_237d0",            b_237d0,        0x00000000u },
    { "fighter_2381c",            b_2381c,        0x00000000u },
    { "fighter_3dadc",            b_3dadc,        0x00000000u },
    { "fighter_3db34",            b_3db34,        0x00000000u },
    { "fighter_3d10c",            b_3d10c,        0x00000000u },
    { "fighter_22a00",            b_22a00,        0x00000000u },
    { "fighter_229fc",            b_229fc,        0x00000000u },
    { "fighter_237d0@mutant",     m_237d0,        0x00000000u },
    { "fighter_237d0@guard",      m_237d0_guard,  0x00000000u },
    { "fighter_2381c@mutant",     m_2381c,        0x00000000u },
    { "fighter_3dadc@mutant",     m_3dadc,        0x00000000u },
    { "fighter_3db34@mutant",     m_3db34,        0x00000000u },
    { "fighter_3d10c@mutant",     m_3d10c,        0x00000000u },
    { "fighter_3d10c@side",       m_3d10c_side,   0x00000000u },
    { "fighter_3d10c@sext",       m_3d10c_sext,   0x00000000u },
    { "fighter_22a00@mutant",     m_22a00,        0x00000000u },
    { "fighter_229fc@mutant",     m_229fc,        0x00000000u },
    { "fighter_14ef8",            b_14ef8,        0x00000000u },
    { "fighter_14f50",            b_14f50,        0x00000000u },
    { "fighter_14fa8",            b_14fa8,        0x00000000u },
    { "fighter_14ff8",            b_14ff8,        0x00000000u },
    { "fighter_150ac",            b_150ac,        0x00000000u },
    { "fighter_14ef8@mutant",     m_14ef8,        0x00000000u },
    { "fighter_14f50@mutant",     m_14f50,        0x00000000u },
    { "fighter_14fa8@mutant",     m_14fa8,        0x00000000u },
    { "fighter_14ff8@mutant",     m_14ff8,        0x00000000u },
    { "fighter_150ac@mutant",     m_150ac,        0x00000000u },
    { "fighter_15478",            b_15478,        0x00000000u },
    { "fighter_3dcec",            b_3dcec,        0x00000000u },
    { "fighter_21114",            b_21114,        0x00000000u },
    { "fighter_15478@mutant",     m_15478,        0x00000000u },
    { "fighter_3dcec@mutant",     m_3dcec,        0x00000000u },
    { "fighter_21114@mutant",     m_21114,        0x00000000u },
    { "fighter_21114@side",       m_21114_side,   0x00000000u },
    { "fighter_21374",            b_21374,        0x00000000u },
    { "fighter_22938",            b_22938,        0x00000000u },
    { "fighter_21374@mutant",     m_21374,        0x00000000u },
    { "fighter_22938@mutant",     m_22938,        0x00000000u },
    { "fighter_22938@order",      m_22938_order,  0x00000000u },
    { "fighter_2116c",            b_2116c,        0xFFFFFFFFu },
    { "fighter_22510",            b_22510,        0xFFFFFFFFu },
    { "fighter_2116c@mutant",     m_2116c,        0xFFFFFFFFu },
    { "fighter_2116c@unsigned",   m_2116c_unsigned, 0xFFFFFFFFu },
    { "fighter_2116c@eax",        m_2116c_eax,    0xFFFFFFFFu },
    { "fighter_22510@mutant",     m_22510,        0xFFFFFFFFu },
    { "fighter_22510@ge",         m_22510_ge,     0xFFFFFFFFu },
    { "fighter_22404",            b_22404,        0x00000000u },
    { "fighter_211f0",            b_211f0,        0x00000000u },
    { "fighter_22588",            b_22588,        0x00000000u },
    { "fighter_22404@mutant",     m_22404,        0x00000000u },
    { "fighter_22404@signed",     m_22404_signed, 0x00000000u },
    { "fighter_211f0@mutant",     m_211f0,        0x00000000u },
    { "fighter_211f0@order",      m_211f0_order,  0x00000000u },
    { "fighter_211f0@zext",       m_211f0_zext,   0x00000000u },
    { "fighter_211f0@slot",       m_211f0_slot,   0x00000000u },
    { "fighter_22588@mutant",     m_22588,        0x00000000u },
    { "fighter_22588@order",      m_22588_order,  0x00000000u },
    { "fighter_212cc",            b_212cc,        0x00000000u },
    { "fighter_22638",            b_22638,        0x00000000u },
    { "fighter_212cc@mutant",     m_212cc,        0x00000000u },
    { "fighter_212cc@signed",     m_212cc_signed, 0x00000000u },
    { "fighter_212cc@side",       m_212cc_side,   0x00000000u },
    { "fighter_22638@mutant",     m_22638,        0x00000000u },
    { "fighter_22638@signed",     m_22638_signed, 0x00000000u },
    { "fighter_22638@byte5d",     m_22638_byte5d, 0x00000000u },
    { "fighter_22638@char",       m_22638_char,   0x00000000u },
    { "fighter_475ec",            b_475ec,        0x00000000u },
    { "fighter_47608",            b_47608,        0x00000000u },
    { "fighter_47624",            b_47624,        0x00000000u },
    { "fighter_48964",            b_48964,        0x00000000u },
    { "fighter_489a0",            b_489a0,        0x00000000u },
    { "fighter_475ec@mutant",     m_475ec,        0x00000000u },
    { "fighter_475ec@side",       m_475ec_side,   0x00000000u },
    { "fighter_47608@mutant",     m_47608,        0x00000000u },
    { "fighter_47624@mutant",     m_47624,        0x00000000u },
    { "fighter_48964@mutant",     m_48964,        0x00000000u },
    { "fighter_48964@late",       m_48964_late,   0x00000000u },
    { "fighter_48964@side",       m_48964_side,   0x00000000u },
    { "fighter_48964@sext",       m_48964_sext,   0x00000000u },
    { "fighter_489a0@mutant",     m_489a0,        0x00000000u },
    { "fighter_489a0@swap",       m_489a0_swap,   0x00000000u },
    { "fighter_489a0@side",       m_489a0_side,   0x00000000u },
    { "fighter_489a0@sext",       m_489a0_sext,   0x00000000u },
    { "fighter_47720",            b_47720,        0x00000000u },
    { "fighter_476fc",            b_476fc,        0x00000000u },
    { "fighter_47648",            b_47648,        0xFFFFFFFFu },
    { "fighter_47688",            b_47688,        0x00000000u },
    { "fighter_47720@mutant",     m_47720,        0x00000000u },
    { "fighter_47720@ebx",        m_47720_ebx,    0x00000000u },
    { "fighter_476fc@mutant",     m_476fc,        0x00000000u },
    { "fighter_476fc@sext",       m_476fc_sext,   0x00000000u },
    { "fighter_47648@mutant",     m_47648,        0xFFFFFFFFu },
    { "fighter_47688@mutant",     m_47688,        0x00000000u },
    { "fighter_47688@pivot",      m_47688_pivot,  0x00000000u },
    { "fighter_47688@zext",       m_47688_zext,   0x00000000u },
    { "fighter_47688@eax",        m_47688_eax,    0x00000000u },
    { "fighter_47874",            b_47874,        0x00000000u },
    { "fighter_47830",            b_47830,        0x00000000u },
    { "fighter_47798",            b_47798,        0xFFFFFFFFu },
    { "fighter_477a8",            b_477a8,        0xFFFFFFFFu },
    { "fighter_477e8",            b_477e8,        0x00000000u },
    { "fighter_47874@mutant",     m_47874,        0x00000000u },
    { "fighter_47874@side",       m_47874_side,   0x00000000u },
    { "fighter_47874@sext",       m_47874_sext,   0x00000000u },
    { "fighter_47874@early",      m_47874_early,  0x00000000u },
    { "fighter_47830@mutant",     m_47830,        0x00000000u },
    { "fighter_47830@side",       m_47830_side,   0x00000000u },
    { "fighter_47830@order",      m_47830_order,  0x00000000u },
    { "fighter_47830@sext",       m_47830_sext,   0x00000000u },
    { "fighter_47798@mutant",     m_47798,        0xFFFFFFFFu },
    { "fighter_47798@eax",        m_47798_eax,    0xFFFFFFFFu },
    { "fighter_477a8@mutant",     m_477a8,        0xFFFFFFFFu },
    { "fighter_477e8@mutant",     m_477e8,        0x00000000u },
    { "fighter_477e8@order",      m_477e8_order,  0x00000000u },
    { "fighter_47fcc",            b_47fcc,        0x00000000u },
    { "fighter_47cb0",            b_47cb0,        0xFFFFFFFFu },
    { "fighter_47d24",            b_47d24,        0x00000000u },
    { "fighter_47e9c",            b_47e9c,        0x00000000u },
    { "fighter_47fcc@mutant",     m_47fcc,        0x00000000u },
    { "fighter_47fcc@order",      m_47fcc_order,  0x00000000u },
    { "fighter_47cb0@mutant",     m_47cb0,        0xFFFFFFFFu },
    { "fighter_47cb0@ge",         m_47cb0_ge,     0xFFFFFFFFu },
    { "fighter_47cb0@lo",         m_47cb0_lo,     0xFFFFFFFFu },
    { "fighter_47d24@mutant",     m_47d24,        0x00000000u },
    { "fighter_47d24@order",      m_47d24_order,  0x00000000u },
    { "fighter_47d24@signed",     m_47d24_signed, 0x00000000u },
    { "fighter_47d24@frame",      m_47d24_frame,  0x00000000u },
    { "fighter_47e9c@mutant",     m_47e9c,        0x00000000u },
    { "fighter_47e9c@slot",       m_47e9c_slot,   0x00000000u },
    { "fighter_47e9c@signed",     m_47e9c_signed, 0x00000000u },
    { "fighter_47e9c@order",      m_47e9c_order,  0x00000000u },
    { "fighter_48608",            b_48608,        0x00000000u },
    { "fighter_48054",            b_48054,        0xFFFFFFFFu },
    { "fighter_480b4",            b_480b4,        0x00000000u },
    { "fighter_48170",            b_48170,        0x00000000u },
    { "fighter_4811c",            b_4811c,        0x00000000u },
    { "fighter_48608@mutant",     m_48608,        0x00000000u },
    { "fighter_48608@order",      m_48608_order,  0x00000000u },
    { "fighter_48608@side",       m_48608_side,   0x00000000u },
    { "fighter_48054@mutant",     m_48054,        0xFFFFFFFFu },
    { "fighter_48054@eax",        m_48054_eax,    0xFFFFFFFFu },
    { "fighter_480b4@mutant",     m_480b4,        0x00000000u },
    { "fighter_480b4@signed",     m_480b4_signed, 0x00000000u },
    { "fighter_480b4@char",       m_480b4_char,   0x00000000u },
    { "fighter_48170@mutant",     m_48170,        0x00000000u },
    { "fighter_48170@order",      m_48170_order,  0x00000000u },
    { "fighter_48170@reset",      m_48170_reset,  0x00000000u },
    { "fighter_48170@al",         m_48170_al,     0x00000000u },
    { "fighter_4811c@mutant",     m_4811c,        0x00000000u },
    { "fighter_4811c@signed",     m_4811c_signed, 0x00000000u },
    { "fighter_4811c@order",      m_4811c_order,  0x00000000u },
    { "fighter_4844c",            b_4844c,        0x00000000u },
    { "fighter_4844c@mutant",     m_4844c,        0x00000000u },
    { "fighter_4844c@signed",     m_4844c_signed, 0x00000000u },
    { "fighter_4844c@abs",        m_4844c_abs,    0x00000000u },
    { "fighter_4844c@order",      m_4844c_order,  0x00000000u },
    { "fighter_4844c@bound",      m_4844c_bound,  0x00000000u },
    { "fighter_4844c@zext",       m_4844c_zext,   0x00000000u },
    { "fighter_4844c@width",      m_4844c_width,  0x00000000u },
    { "fighter_18bc8",          b_18bc8,         0x00000000u },
    { "fighter_18bc8@mutant",   m_18bc8,         0x00000000u },
    { "fighter_21084",          b_21084,         0x00000000u },
    { "fighter_21084@mutant",   m_21084,         0x00000000u },
    { "fighter_21084@byte14",   m_21084_byte14,  0x00000000u },
    { "fighter_400e0",          b_400e0,         0x00000000u },
    { "fighter_400e0@mutant",   m_400e0,         0x00000000u },
    { "fighter_21044",          b_21044,         0x00000000u },
    { "fighter_21044@mutant",   m_21044,         0x00000000u },
    { "fighter_21044@neg",      m_21044_neg,     0x00000000u },
    { "fighter_1549c",          b_1549c,         0x00000000u },
    { "fighter_1549c@mutant",   m_1549c,         0x00000000u },
    { "fighter_1549c@side",     m_1549c_side,    0x00000000u },
    { "fighter_154e8",          b_154e8,         0x00000000u },
    { "fighter_154e8@mutant",   m_154e8,         0x00000000u },
    { "fighter_154e8@side",     m_154e8_side,    0x00000000u },
    { "fighter_229e8",          b_229e8,         0x00000000u },
    { "fighter_229e8@mutant",   m_229e8,         0x00000000u },
    { "fighter_243f8",          b_243f8,         0x00000000u },
    { "fighter_243f8@mutant",   m_243f8,         0x00000000u },
    { "fighter_243f8@slot",     m_243f8_slot,    0x00000000u },
    { "fighter_29c08",          b_29c08,         0x00000000u },
    { "fighter_15510",          b_15510,         0x00000000u },
    { "fighter_15510@mutant",   m_15510,         0x00000000u },
    { "fighter_15510@side",     m_15510_side,    0x00000000u },
    { "fighter_15510@pal",      m_15510_pal,     0x00000000u },
    { "fighter_15510@a5",       m_15510_a5,      0x00000000u },
    { "fighter_241a8",          b_241a8,         0x00000000u },
    { "fighter_241a8@mutant",   m_241a8,         0x00000000u },
    { "fighter_241a8@sext",     m_241a8_sext,    0x00000000u },
    { "fighter_40358",          b_40358,         0x00000000u },
    { "fighter_40358@mutant",   m_40358,         0x00000000u },
    { "fighter_45c54",          b_45c54,         0x00000000u },
    { "fighter_45c54@mutant",   m_45c54,         0x00000000u },
    { "fighter_489dc",          b_489dc,         0x00000000u },
    { "fighter_489dc@mutant",   m_489dc,         0x00000000u },
    { "fighter_400ec",          b_400ec,         0x00000000u },
    { "fighter_400ec@mutant",   m_400ec,         0x00000000u },
    { "fighter_400ec@side",     m_400ec_side,    0x00000000u },
    { "fighter_3427c",          b_3427c,         0x00000000u },
    { "fighter_3427c@mutant",   m_3427c,         0x00000000u },
    { "fighter_3427c@side",     m_3427c_side,    0x00000000u },
    { "fighter_3427c@voice",    m_3427c_voice,   0x00000000u },
    { "fighter_34308",          b_34308,         0x00000000u },
    { "fighter_34308@mutant",   m_34308,         0x00000000u },
    { "fighter_34308@side",     m_34308_side,    0x00000000u },
    { "fighter_3438c",          b_3438c,         0x00000000u },
    { "fighter_3438c@mutant",   m_3438c,         0x00000000u },
    { "fighter_3438c@side",     m_3438c_side,    0x00000000u },
    { "fighter_34418",          b_34418,         0x00000000u },
    { "fighter_34418@mutant",   m_34418,         0x00000000u },
    { "fighter_34418@side",     m_34418_side,    0x00000000u },
    { "fighter_344a4",          b_344a4,         0x00000000u },
    { "fighter_344a4@mutant",   m_344a4,         0x00000000u },
    { "fighter_344a4@side",     m_344a4_side,    0x00000000u },
    { "fighter_34530",          b_34530,         0x00000000u },
    { "fighter_34530@mutant",   m_34530,         0x00000000u },
    { "fighter_34530@side",     m_34530_side,    0x00000000u },
    { "fighter_345bc",          b_345bc,         0x00000000u },
    { "fighter_345bc@mutant",   m_345bc,         0x00000000u },
    { "fighter_345bc@side",     m_345bc_side,    0x00000000u },
    { "fighter_156e0",          b_156e0,         0x00000000u },
    { "fighter_156e0@mutant",   m_156e0,         0x00000000u },
    { "fighter_156e0@side",     m_156e0_side,    0x00000000u },
    { "fighter_156e0@signed",   m_156e0_signed,  0x00000000u },
    { "fighter_156e0@plus",     m_156e0_plus,    0x00000000u },
    { "fighter_22ab8",          b_22ab8,         0x00000000u },
    { "fighter_22ab8@mutant",   m_22ab8,         0x00000000u },
    { "fighter_22ab8@side",     m_22ab8_side,    0x00000000u },
    { "fighter_22ab8@sext",     m_22ab8_sext,    0x00000000u },
    { "fighter_22ab8@add",      m_22ab8_add,     0x00000000u },
    { "fighter_22ab8@table",    m_22ab8_table,   0x00000000u },
    { "fighter_37b70",          b_37b70,         0x00000000u },
    { "fighter_37b70@mutant",   m_37b70,         0x00000000u },
    { "fighter_37b70@path",     m_37b70_path,    0x00000000u },
    { "fighter_37b70@neg",      m_37b70_neg,     0x00000000u },
    { "fighter_37b70@hrec",     m_37b70_hrec,    0x00000000u },
    { "fighter_37b70@sext",     m_37b70_sext,    0x00000000u },



};

static const binding_t *find_binding(const char *name)
{
    for (size_t i = 0; i < sizeof k_bindings / sizeof k_bindings[0]; i++)
        if (strcmp(k_bindings[i].name, name) == 0) return &k_bindings[i];
    return NULL;
}

/* The image the dump covers: CODE_BASE..CODE_BASE+g_len. Everything above it is zero in mem[]
 * after the load, and a case may change it, so it is scanned for non-zero bytes and cleared. */
static u8 *g_pristine, *g_pre;
static u32 g_len;

static int load_image(const char *exe, const char *img_path)
{
    if (!mem_load_le(exe, img_path)) return 0;
    FILE *f = fopen(img_path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || (u32)n > MEM_SIZE - CODE_BASE || ((u32)n & 7u) != 0) { fclose(f); return 0; }
    g_len = (u32)n;
    g_pristine = malloc(g_len);
    g_pre = malloc(g_len);
    if (!g_pristine || !g_pre || fread(g_pristine, 1, g_len, f) != g_len) { fclose(f); return 0; }
    fclose(f);
    /* the dump is mem[] as the loader left it: a mismatch means the file is not this image */
    return memcmp(mem + CODE_BASE, g_pristine, g_len) == 0;
}

/* Every fn_register call site of port/src, so that an indirect call to any ported code pointer
 * resolves to its C function (record E3 §E3.7, decision D3: an unregistered target is reported as
 * a miss, and a registered one must not be). The sites (grep fn_register port/src): actors_init,
 * effects_init's camera_register, attract_scene_tick's attract_register, svcmenu_register.
 * actors_init validates the two pool pointers, which only game_init sets: point them at a scratch
 * range above the image for the call, then restore them. attract_scene_tick registers and then
 * walks the mask DS_00104AD0, so the mask is zeroed for the call. effects_init also builds the
 * effects pool in the image, so the image is put back from the dump after it. The other three
 * leave mem[] as the loader had it, which is checked. */
static int register_code(void)
{
    u32 pool = DSD(DS_001014F4), pset = DSD(DS_001014EC), mask = DSD(DS_00104AD0);
    DSD(DS_001014F4) = 0x2000000u;
    DSD(DS_001014EC) = 0x2100000u;
    DSD(DS_00104AD0) = 0u;
    int ok = actors_init();
    DSD(DS_001014F4) = pool;
    DSD(DS_001014EC) = pset;
    attract_scene_tick();
    DSD(DS_00104AD0) = mask;
    svcmenu_register();
    int same = memcmp(mem + CODE_BASE, g_pristine, g_len) == 0;
    effects_init();
    memcpy(mem + CODE_BASE, g_pristine, g_len);
    return ok && same;
}

typedef struct { u32 addr; u32 len; u8 b[64]; } poke_t;
/* A callee in the case's call set (record E3 §E3.4): `stub` returns at once with `eax` after its
 * writes, `real` runs the C body; both are recorded. A write lands at `off` (base -1) or at
 * argument `base` plus `off`. */
typedef struct { int base; u32 off; poke_t p; } swrite_t;
typedef struct {
    u32 addr, eax;
    int real;
    swrite_t w[4];
    int nw;
} stub_t;
typedef struct {
    char id[64], fn[64];
    u32 reg[R_N];
    poke_t poke[16];
    int npoke;
    stub_t stub[16];
    int nstub;
} case_t;

static const case_t *g_case;          /* the case running, for the seam hook */
static int g_seam_error;              /* a stub write the hook refused: 1 outside the image, 2 an argument the callee lacks */
static u32 g_seam_addr, g_seam_arg, g_seam_nargs;

/* Prints `<tag> <addr> <byte>` for every non-zero byte of mem[lo..hi), the range outside the image
 * where the load left zeros, and clears it when `clear` (the case's end) but not at a call. */
static void scan_outside(u32 lo, u32 hi, char tag, int clear)
{
    static const u8 zero[0x10000];
    for (u32 a = lo; a < hi; ) {
        u32 n = hi - a < sizeof zero ? hi - a : (u32)sizeof zero;
        if (memcmp(mem + a, zero, n) == 0) { a += n; continue; }      /* a zero block: nothing written */
        for (u32 i = 0; i < n; i++)
            if (mem[a + i] != 0) {
                printf("%c 0x%X 0x%02X\n", tag, a + i, mem[a + i]);
                if (clear) mem[a + i] = 0;
            }
        a += n;
    }
}

/* The memory at a recorded call (record E3 §E3.12): one `m` line per byte of mem[] that differs from
 * the case's start (the image against g_pre, the rest against the load's zeros), so a store the port
 * moves across a stubbed call is a difference although the bytes at return may agree. */
static void print_call_memory(void)
{
    scan_outside(0, CODE_BASE, 'm', 0);
    for (u32 i = 0; i < g_len; i++)
        if (mem[CODE_BASE + i] != g_pre[i]) printf("m 0x%X 0x%02X\n", CODE_BASE + i, mem[CODE_BASE + i]);
    scan_outside(CODE_BASE + g_len, MEM_SIZE, 'm', 0);
}

static int seam_hook(u32 addr, u32 nargs, const u32 *args, u32 *eax)
{
    const stub_t *s = NULL;
    for (int i = 0; g_case && i < g_case->nstub; i++)
        if (g_case->stub[i].addr == addr) s = &g_case->stub[i];
    if (!s) return 0;
    printf("c 0x%X", addr);
    for (u32 i = 0; i < nargs; i++) printf(" 0x%X", args[i]);
    printf("\n");
    print_call_memory();
    if (s->real) return 0;
    for (int i = 0; i < s->nw; i++) {
        const swrite_t *w = &s->w[i];
        u32 at = w->off;
        if (w->base >= 0) {
            if ((u32)w->base >= nargs) {
                g_seam_error = 2;
                g_seam_addr = addr;
                g_seam_arg = (u32)w->base;
                g_seam_nargs = nargs;
                continue;
            }
            at = args[w->base] + w->off;
        }
        if (!(at >= CODE_BASE && w->p.len <= g_len && at - CODE_BASE <= g_len - w->p.len)) {
            g_seam_error = 1;
            continue;
        }
        memcpy(mem + at, w->p.b, w->p.len);
    }
    *eax = s->eax;
    return 1;
}

static int parse_hex(const char *s, u32 *out)
{
    char *e;
    unsigned long v = strtoul(s, &e, 16);
    if (e == s || *e != '\0' || v > 0xFFFFFFFFul) return 0;
    *out = (u32)v;
    return 1;
}

/* A plain decimal (what cases_text writes for `argN`): digits only, no sign, no 0x. */
static int parse_dec(const char *s, u32 *out)
{
    u32 v = 0;
    if (*s == '\0') return 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9' || v > 100u) return 0;
        v = v * 10u + (u32)(*s - '0');
    }
    *out = v;
    return 1;
}

static int parse_bytes(const char *s, poke_t *p)
{
    size_t n = strlen(s);
    if (n == 0 || (n & 1u) || n / 2 > sizeof p->b) return 0;
    for (size_t i = 0; i < n / 2; i++) {
        char t[3] = { s[2 * i], s[2 * i + 1], 0 }, *e;
        unsigned long v = strtoul(t, &e, 16);
        if (*e != '\0') return 0;
        p->b[i] = (u8)v;
    }
    p->len = (u32)(n / 2);
    return 1;
}

static void run_case(const case_t *c)
{
    printf("case %s\n", c->id);
    const binding_t *b = find_binding(c->fn);
    if (!b) { printf("error unknown binding %s\nend\n", c->fn); return; }
    /* every poke is checked before any is applied, so an error leaves mem[] untouched and the
     * next case starts from the pristine image. The check is written so it cannot wrap in u32. */
    for (int i = 0; i < c->npoke; i++) {
        const poke_t *p = &c->poke[i];
        if (!(p->len <= g_len && p->addr >= CODE_BASE && p->addr - CODE_BASE <= g_len - p->len)) {
            printf("error poke 0x%X outside the image\nend\n", p->addr);
            return;
        }
    }
    for (int i = 0; i < c->npoke; i++) memcpy(mem + c->poke[i].addr, c->poke[i].b, c->poke[i].len);
    memcpy(g_pre, mem + CODE_BASE, g_len);

    u32 eax = 0;
    g_case = c;
    g_seam_error = 0;
    b->call(c->reg, &eax);
    g_case = NULL;
    if (g_seam_error == 2)
        printf("error a stub write names argument %u, but 0x%X reports %u arguments\n", g_seam_arg, g_seam_addr, g_seam_nargs);
    else if (g_seam_error) printf("error a stub write outside the image\n");
    else printf("ret eax 0x%X mask 0x%X\n", eax & b->eax_mask, b->eax_mask);
    for (u32 i = 0; i < g_len; i++)
        if (mem[CODE_BASE + i] != g_pre[i]) printf("w 0x%X 0x%02X\n", CODE_BASE + i, mem[CODE_BASE + i]);
    scan_outside(0, CODE_BASE, 'w', 1);              /* the case's writes outside the image, cleared */
    scan_outside(CODE_BASE + g_len, MEM_SIZE, 'w', 1);
    memcpy(mem + CODE_BASE, g_pristine, g_len);
    printf("end\n");
}

static int run_cases(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "diffrun: cannot open %s\n", path); return 0; }
    case_t c;
    int open = 0;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *t[6] = { 0 };
        int n = 0;
        for (char *s = strtok(line, " \t\r\n"); s && n < 6; s = strtok(NULL, " \t\r\n")) t[n++] = s;
        if (n == 0 || t[0][0] == '#') continue;
        if (strcmp(t[0], "case") == 0 && n == 2) {
            if (open) {
                fprintf(stderr, "diffrun: case %s opened inside unterminated case %s\n", t[1], c.id);
                fclose(f);
                return 0;
            }
            memset(&c, 0, sizeof c);
            snprintf(c.id, sizeof c.id, "%s", t[1]);
            open = 1;
        } else if (!open) {
            fprintf(stderr, "diffrun: '%s' outside a case\n", t[0]);
            fclose(f);
            return 0;
        } else if (strcmp(t[0], "fn") == 0 && n == 2) {
            snprintf(c.fn, sizeof c.fn, "%s", t[1]);
        } else if (strcmp(t[0], "reg") == 0 && n == 3) {
            int r = 0;
            while (r < R_N && strcmp(t[1], k_reg[r]) != 0) r++;
            if (r == R_N || !parse_hex(t[2], &c.reg[r])) { fprintf(stderr, "diffrun: bad reg line\n"); fclose(f); return 0; }
        } else if (strcmp(t[0], "poke") == 0 && n == 3 && c.npoke < 16) {
            poke_t *p = &c.poke[c.npoke++];
            if (!parse_hex(t[1], &p->addr) || !parse_bytes(t[2], p)) { fprintf(stderr, "diffrun: bad poke line\n"); fclose(f); return 0; }
        } else if (strcmp(t[0], "stub") == 0 && n == 4 && c.nstub < 16) {
            /* stub <addr> stub|real <eax> */
            stub_t *s = &c.stub[c.nstub++];
            s->real = strcmp(t[2], "real") == 0;
            if (!parse_hex(t[1], &s->addr) || !parse_hex(t[3], &s->eax)
                    || (!s->real && strcmp(t[2], "stub") != 0)) {
                fprintf(stderr, "diffrun: bad stub line\n"); fclose(f); return 0;
            }
        } else if (strcmp(t[0], "swrite") == 0 && n == 4 && c.nstub > 0
                   && !c.stub[c.nstub - 1].real && c.stub[c.nstub - 1].nw < 4) {
            /* swrite <abs|argN> <off> <bytes>, for the last stub line */
            stub_t *s = &c.stub[c.nstub - 1];
            swrite_t *w = &s->w[s->nw++];
            u32 k = 0;
            if (strcmp(t[1], "abs") == 0) w->base = -1;
            else if (strncmp(t[1], "arg", 3) == 0 && parse_dec(t[1] + 3, &k) && k < 8) w->base = (int)k;
            else { fprintf(stderr, "diffrun: bad swrite base\n"); fclose(f); return 0; }
            if (!parse_hex(t[2], &w->off) || !parse_bytes(t[3], &w->p)) {
                fprintf(stderr, "diffrun: bad swrite line\n"); fclose(f); return 0;
            }
        } else if (strcmp(t[0], "end") == 0) {
            run_case(&c);
            open = 0;
        } else {
            fprintf(stderr, "diffrun: cannot parse '%s'\n", t[0]);
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    if (open) fprintf(stderr, "diffrun: case %s has no end line\n", c.id);
    return !open;
}

int main(int argc, char **argv)
{
    const char *exe = NULL, *img = NULL, *cases = NULL;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            fprintf(stderr, "diffrun: option %s needs a value\n", argv[i]);
            fprintf(stderr, "usage: diffrun --exe PRAGE.EXE --image-out FILE [--cases FILE]\n");
            return 2;
        }
        if (strcmp(argv[i], "--exe") == 0) exe = argv[i + 1];
        else if (strcmp(argv[i], "--image-out") == 0) img = argv[i + 1];
        else if (strcmp(argv[i], "--cases") == 0) cases = argv[i + 1];
        else { fprintf(stderr, "diffrun: unknown option %s\n", argv[i]); return 2; }
    }
    if (!exe || !img) { fprintf(stderr, "usage: diffrun --exe PRAGE.EXE --image-out FILE [--cases FILE]\n"); return 2; }
    if (!load_image(exe, img)) { fprintf(stderr, "diffrun: cannot load %s\n", exe); return 1; }
    if (!register_code()) { fprintf(stderr, "diffrun: cannot register the port's code pointers\n"); return 1; }
    pr_seam = seam_hook;
    return cases && !run_cases(cases) ? 1 : 0;
}
