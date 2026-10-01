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
    { "fighter_45878",            b_45878,        0x00000000u },
    { "anim_10fa8",               b_10fa8,        0x00000000u },
    { "anim_3e4e4",               b_3e4e4,        0x00000000u },
    { "fighter_ctx_same",         b_ctx_same,     0x00000000u },
    { "hit_anim_ctx",             b_anim_ctx,     0x00000000u },
    { "hit_anim_start_b",         b_3c4cc,        0x00000000u },
    { "fn_resolved",              b_fn_resolved,  0xFFFFFFFFu },
    { "hit_anim_start_b@mutant",  m_3c4cc,        0x00000000u },
    { "fighter_45878@mutant",     m_45878,        0x00000000u },
    { "anim_10fa8@mutant",        m_10fa8,        0x00000000u },
    { "anim_3e4e4@mutant",        m_3e4e4,        0x00000000u },
    { "fighter_ctx_same@mutant",  m_ctx_same,     0x00000000u },
    { "hit_anim_ctx@mutant",      m_anim_ctx,     0x00000000u },
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

static int seam_hook(u32 addr, u32 nargs, const u32 *args, u32 *eax)
{
    const stub_t *s = NULL;
    for (int i = 0; g_case && i < g_case->nstub; i++)
        if (g_case->stub[i].addr == addr) s = &g_case->stub[i];
    if (!s) return 0;
    printf("c 0x%X", addr);
    for (u32 i = 0; i < nargs; i++) printf(" 0x%X", args[i]);
    printf("\n");
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

/* Prints, then clears, every non-zero byte of mem[lo..hi): the bytes the case wrote outside the
 * image, where the load left zeros. */
static void flush_outside(u32 lo, u32 hi)
{
    for (u32 a = lo; a < hi; a += 8) {
        uint64_t w;
        memcpy(&w, mem + a, 8);
        if (w == 0) continue;
        for (u32 i = 0; i < 8; i++)
            if (mem[a + i] != 0) { printf("w 0x%X 0x%02X\n", a + i, mem[a + i]); mem[a + i] = 0; }
    }
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
    flush_outside(0, CODE_BASE);
    flush_outside(CODE_BASE + g_len, MEM_SIZE);
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
