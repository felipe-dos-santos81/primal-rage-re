#include "input.h"
#include "../host.h"
#include "../mem.h"
#include "../symbols.h"

static u16 g_keys[INPUT_QUEUE_CAP];
static int g_head;  /* index of the oldest queued key */
static int g_count; /* entries currently queued, 0..INPUT_QUEUE_CAP */

void input_clear(void)
{
    g_head = 0;
    g_count = 0;
}

void input_push(u8 scan, u8 ascii)
{
    if (g_count == INPUT_QUEUE_CAP) {
        /* PORT: the full BIOS buffer stops accepting keys; the port instead
         * folds the drop onto the oldest entry so recent input survives. */
        g_head = (g_head + 1) % INPUT_QUEUE_CAP;
        g_count--;
    }
    int tail = (g_head + g_count) % INPUT_QUEUE_CAP;
    g_keys[tail] = (u16)(((u16)scan << 8) | (u16)ascii);
    g_count++;
}

int input_has_key(void)
{
    return g_count > 0;
}

u16 input_get_key(void)
{
    while (g_count == 0) {
        host_pump();
        /* PORT: a window close ends the wait with no key (0, which int 16h
         * never returns); the blocking readers (0x249F0, 0x24C5C's pause)
         * then leave for game_loop's quit. */
        if (host_quit_requested()) return 0;
    }
    u16 key = g_keys[g_head];
    g_head = (g_head + 1) % INPUT_QUEUE_CAP;
    g_count--;
    return key;
}

int input_check_key(void)
{
    return g_count > 0 ? (int)g_keys[g_head] : 0;
}

/* 0x50146 — record §49-X. EAX = mask, DX = first, BX = next. */
void input_repeat_set(u32 mask, u32 first, u32 next)
{
    PR_SEAM(0x50146u, mask, first, next);
    DSD(DS_000E1C3C) = mask;                            /* 0x50146 */
    DSW(DS_000E1C42) = (u16)first;                      /* 0x5014B */
    DSW(DS_000E1C40) = (u16)first;                      /* 0x50152 */
    DSW(DS_000E1C44) = (u16)next;                       /* 0x50159 */
}

/* 0x2D2F0. `xor eax,eax; ret`. */
u32 input_joystick_device(u32 selector)
{
    (void)selector;
    return 0u;
}

/* 0x50161. Level/latch bit selector. */
u32 input_select_bits(u32 mask)
{
    u32 level = DSD(DS_000E1C34);
    if (mask != 0u) {
        u32 latch = DSD(DS_000E1C38);
        u32 edge = (latch ^ level) & mask;      /* 0x50174/0x50176 */
        DSD(DS_000E1C38) = latch | edge;        /* 0x50178 */
        level = (level & ~mask) | edge;         /* 0x5017E/0x50180/0x50182 */
    }
    return level;
}

/* 0x500C4. The key bytes are combined as (byte[+0x2d8] << 24) |
 * (byte[+0x2d9] << 8), so the level word only ever occupies the 0xFF00FF00
 * positions. The new level keeps the previous value for every bit that changed
 * this frame - the raw's one-frame debounce. */
u32 input_pump(void)
{
    PR_SEAM_RET0(0x500C4u);
    const u8 *k = mem + DSD(DS_00101514);
    u32 cur = ((u32)k[0x2d8] << 24) | ((u32)k[0x2d9] << 8);
    u32 changed = DSD(DS_000E1C30) ^ cur;
    DSD(DS_000E1C30) = cur;
    u32 level = DSD(DS_000E1C34);
    u32 newlevel = (~changed & cur) | (changed & level);
    DSD(DS_000E1C34) = newlevel;
    DSD(DS_000E1C38) &= newlevel;

    u32 rpt = DSD(DS_000E1C3C);
    if ((changed & level & rpt) != 0u || (newlevel & rpt) == 0u) {
        DSW(DS_000E1C40) = DSW(DS_000E1C42);            /* 0x50130 */
    } else if (--DSW(DS_000E1C40) == 0u) {              /* 0x5010F */
        DSW(DS_000E1C40) = DSW(DS_000E1C44);            /* 0x50118 */
        DSD(DS_000E1C38) &= ~rpt;                       /* 0x50128 */
    }
    return DSD(DS_000E1C34);
}

/* 0x4F644. The first selector uses the 0xFF00FF00 family (the bits the level
 * word can carry) and yields the newly-pressed mask; the second uses the
 * complementary family and keeps only the level half, yielding the held mask.
 * Both share the DAT_000E1C38 latch, in this order. */
void input_state_update(void)
{
    PR_SEAM0(0x4F644u);
    DSD(DS_001088E4) = input_select_bits(0xFF00FF00u);              /* 0x4F64A */
    DSD(DS_001088D8) = input_select_bits(0x00FF00FFu) & 0xFF00FF00u;/* 0x4F659 */
    DSD(DS_001088DC) = input_joystick_device(0xFFu);                /* 0x4F66D */
    DSD(DS_001088D4) = input_joystick_device(0xFFFFFF00u) & 0xFFu;  /* 0x4F67C */
    if ((DSB(DS_001088D4) & 2u) != 0u)                              /* 0x4F68B */
        DSD(DS_001088E4) |= DSD(DS_001088D8);
    DSW(DS_001088E0) = (u16)(((DSD(DS_001088E4) & 0xFF000000u) >> 24) |
                             ((DSD(DS_001088D8) & 0xFF000000u) >> 16));
    DSW(DS_001088E2) = (u16)(((DSD(DS_001088E4) & 0x0000FF00u) >> 8) |
                             (DSD(DS_001088D8) & 0x0000FF00u));
}

/* ---- the keyboard readers (record §H2) ----------------------------------
 * 0x1BBAC's device-0 arms. The raw loads the block base once into ECX, reads
 * each slot's scan byte, and indexes the key-state table at +0x254 with it;
 * the four slot readers re-store the base at the end (0x1B693/0x1B723/
 * 0x1B7B3/0x1B843) -- a no-op, since it is the value they read, kept for the
 * raw's own shape. A slot whose table byte has bit 7 clear is a make (the RM.DRV
 * handler's make byte), and that sets the game bit. */

static const u8 k_dir_bit[4] = { 0x80u, 0x40u, 0x20u, 0x10u };
static const u8 k_btn_bit[4] = { 0x01u, 0x02u, 0x04u, 0x08u };

/* The four positions from `first`: each slot's scan indexes the key-state
 * table, and a make entry sets that position's bit. */
static u8 input_slot_bits(u32 base, u32 first, const u8 bit[4])
{
    u8 al = 0u;
    for (u32 i = 0u; i < 4u; i++) {
        const u8 scan = DSB(base + first + i);          /* 0x1B61D */
        if ((DSB(base + 0x254u + scan) & 0x80u) == 0u)  /* 0x1B623 */
            al |= bit[i];                               /* 0x1B637 */
    }
    return al;
}

/* 0x1B610 — host input truth §H2. */
u8 input_dirs_p1(void)
{
    PR_SEAM_RET0(0x1B610u);
    const u32 base = DSD(DS_00101514);                  /* 0x1B613 */
    const u8 al = input_slot_bits(base, 0x2DEu, k_dir_bit);
    DSD(DS_00101514) = base;                            /* 0x1B693 */
    return al;
}

/* 0x1B6A0 — host input truth §H2. */
u8 input_dirs_p2(void)
{
    PR_SEAM_RET0(0x1B6A0u);
    const u32 base = DSD(DS_00101514);                  /* 0x1B6A3 */
    const u8 al = input_slot_bits(base, 0x2E6u, k_dir_bit);
    DSD(DS_00101514) = base;                            /* 0x1B723 */
    return al;
}

/* 0x1B730 — host input truth §H2. */
u8 input_buttons_p1(void)
{
    PR_SEAM_RET0(0x1B730u);
    const u32 base = DSD(DS_00101514);                  /* 0x1B733 */
    const u8 al = input_slot_bits(base, 0x2E2u, k_btn_bit);
    DSD(DS_00101514) = base;                            /* 0x1B7B3 */
    return al;
}

/* 0x1B7C0 — host input truth §H2. */
u8 input_buttons_p2(void)
{
    PR_SEAM_RET0(0x1B7C0u);
    const u32 base = DSD(DS_00101514);                  /* 0x1B7C3 */
    const u8 al = input_slot_bits(base, 0x2EAu, k_btn_bit);
    DSD(DS_00101514) = base;                            /* 0x1B843 */
    return al;
}

/* 0x1B850 — host input truth §H2. */
u8 input_f1(void)
{
    PR_SEAM_RET0(0x1B850u);
    const u32 base = DSD(DS_00101514);                  /* 0x1B851 */
    const u8 ah = DSB(base + 0x28Fu) & 0x80u;           /* 0x1B859 */
    return ah == 0u ? 1u : 0u;                          /* 0x1B86A */
}

/* 0x1B870 — host input truth §H2. */
u8 input_f2(void)
{
    PR_SEAM_RET0(0x1B870u);
    const u32 base = DSD(DS_00101514);                  /* 0x1B871 */
    const u8 ah = DSB(base + 0x290u) & 0x80u;           /* 0x1B879 */
    return ah == 0u ? 1u : 0u;                          /* 0x1B88A */
}

/* ---- the game-port button readers (record §H3) ---------------------------
 * 0x1BBAC's device 2/4/6 arms. The raw reads the PC game port's button byte
 * once (`mov edx,0x201; in al,dx`) and keeps it in AH while it tests the bits.
 * The port's four button inputs live in bits 4..7 and are ACTIVE LOW -- a
 * pressed button grounds its bit -- so an input bit CLEAR sets the game bit.
 * 0x1B890 maps bits 4..7 -> 1/2/4/8 (both players); 0x1B8DC bits 4/5 -> 1/4
 * (P1); 0x1B908 bits 6/7 -> 1/4 (P2). Bits 0..3 and each reader's other
 * player's bits are not read. The byte comes from the host. */

/* 0x1B890 — host input truth §H3. */
u8 input_port_buttons_both(void)
{
    PR_SEAM_RET0(0x1B890u);
    const u8 port = host_port_buttons();                /* 0x1B899 */
    u8 al = 0u;
    if ((port & 0x10u) == 0u) al |= 1u;                 /* 0x1B8A4/0x1B8A8 */
    if ((port & 0x20u) == 0u) al |= 2u;                 /* 0x1B8B2/0x1B8B6 */
    if ((port & 0x40u) == 0u) al |= 4u;                 /* 0x1B8C1/0x1B8C5 */
    if ((port & 0x80u) == 0u) al |= 8u;                 /* 0x1B8D1/0x1B8D3 */
    return al;                                          /* 0x1B8D6 */
}

/* 0x1B8DC — host input truth §H3. */
u8 input_port_buttons_p1(void)
{
    PR_SEAM_RET0(0x1B8DCu);
    const u8 port = host_port_buttons();                /* 0x1B8E5 */
    u8 al = 0u;
    if ((port & 0x10u) == 0u) al |= 1u;                 /* 0x1B8EA/0x1B8F3 */
    if ((port & 0x20u) == 0u) al |= 4u;                 /* 0x1B8F7/0x1B900 */
    return al;                                          /* 0x1B903 */
}

/* 0x1B908 — host input truth §H3. */
u8 input_port_buttons_p2(void)
{
    PR_SEAM_RET0(0x1B908u);
    const u8 port = host_port_buttons();                /* 0x1B911 */
    u8 al = 0u;
    if ((port & 0x40u) == 0u) al |= 1u;                 /* 0x1B916/0x1B91F */
    if ((port & 0x80u) == 0u) al |= 4u;                 /* 0x1B923/0x1B92C */
    return al;                                          /* 0x1B92F */
}

/* ---- the axis readers (record §H4) --------------------------------------
 * The raw's `mov cx, word; sub cx, word` is a 16-bit subtraction, so a
 * normalized value below its center wraps to a small negative displacement
 * (and back). Both compares are signed: `cmp cx,0x1e; jl` and
 * `cmp cx,-0x1e; jg`, so +0x1D/-0x1D are still in the dead band. */

/* 0x4FF8F — host input truth §H4. */
u8 input_axes_a(void)
{
    PR_SEAM_RET0(0x4FF8Fu);
    u8 al = 0u;                                                     /* 0x4FF90 */
    s16 cx = (s16)(u16)(DSW(DS_000E1C1E) - DSW(DS_000E1C22));       /* 0x4FF92/0x4FF99 */
    if (cx >= 0x1E) al |= 0x10u;                                    /* 0x4FFA2..0x4FFA8 */
    else if (cx <= -0x1E) al |= 0x20u;                              /* 0x4FFAC..0x4FFB2 */
    cx = (s16)(u16)(DSW(DS_000E1C20) - DSW(DS_000E1C24));           /* 0x4FFB4/0x4FFBB */
    if (cx >= 0x1E) al |= 0x40u;                                    /* 0x4FFC4..0x4FFCA */
    else if (cx <= -0x1E) al |= 0x80u;                              /* 0x4FFCE..0x4FFD4 */
    return al;                                                      /* 0x4FFD6 */
}

/* 0x4FFD8 — host input truth §H4. */
u8 input_axes_b(void)
{
    PR_SEAM_RET0(0x4FFD8u);
    u8 al = 0u;                                                     /* 0x4FFD9 */
    s16 cx = (s16)(u16)(DSW(DS_000E1C26) - DSW(DS_000E1C2A));       /* 0x4FFDB/0x4FFE2 */
    if (cx >= 0x1E) al |= 0x10u;                                    /* 0x4FFEB..0x4FFF1 */
    else if (cx <= -0x1E) al |= 0x20u;                              /* 0x4FFF5..0x4FFFB */
    cx = (s16)(u16)(DSW(DS_000E1C28) - DSW(DS_000E1C2C));           /* 0x4FFFD/0x50004 */
    if (cx >= 0x1E) al |= 0x40u;                                    /* 0x5000D..0x50013 */
    else if (cx <= -0x1E) al |= 0x80u;                              /* 0x50017..0x5001D */
    return al;                                                      /* 0x5001F */
}

/* ---- the ISR input sampler (record §H4) ---------------------------------
 * 0x1BBAC builds both players' bytes. The device words are 16-bit reads of
 * the key-config block (`mov dx, word [edx+0x2d4]`); the presence block
 * re-reads the block after each clear, and the two switches read the words
 * again (0x1BCF4/0x1BD66), so a clear is what the arms act on. Each arm's
 * readers run in the raw's order (the call seam records them), and the two
 * writes happen after that player's readers. */

/* 0x1BBAC — host input truth §H4. */
void input_sampler(void)
{
    PR_SEAM0(0x1BBACu);
    u32 base = DSD(DS_00101514);                            /* 0x1BBAE */
    /* PORT: no key-config block yet -- the drivers' diffrun cases, and a
     * headless run before 0x1BEC4 has built it -- so there is nothing to
     * sample. The raw's ISR is installed only after that block exists
     * (0x1BF4C/0x1BF6D); the port's game_loop folds the ISR in, so it must not
     * read mem[0x254..] through a zero base. */
    if (base == 0u || base > MEM_SIZE - 0x2EEu) return;
    u16 dev1 = DSW(base + 0x2D4u);                          /* 0x1BBB6 */
    int p1 = (dev1 == 2u || dev1 == 4u || dev1 == 6u);      /* 0x1BBC5..0x1BBD4 */
    const u16 dev2 = DSW(base + 0x2D6u);                    /* 0x1BBDC */
    int p2 = 0;
    if (dev2 == 2u || dev2 == 4u || dev2 == 6u) {           /* 0x1BBE9 */
        /* A P2 device 2 arm reads the same both-player game-port bits as P1's,
         * so a P2 device 2 forces P1's arm to a port too (0x1BC35). */
        if (p1 || dev2 != 2u) p2 = 1;                       /* 0x1BBF8..0x1BC3B */
        else { p1 = 1; p2 = 0; }
    }
    if (p1 && p2) {                                         /* 0x1BC3D/0x1BC46 */
        if (host_port_poll_both() == 0) {                   /* 0x1BC4F (0x4FDF5) */
            if (host_port_probe(3u) == 0) {                 /* 0x1BC5C (0x4FBBB) */
                base = DSD(DS_00101514);                    /* 0x1BC6A */
                DSW(base + 0x2D4u) = 0u;                    /* 0x1BC6F */
            }
            if (host_port_probe(0xCu) == 0) {               /* 0x1BC78 */
                base = DSD(DS_00101514);                    /* 0x1BCE1 */
                DSW(base + 0x2D6u) = 0u;
            }
        }
    } else if (!p1 || p2) {
        if (!p1 && p2) {                                    /* P2 is the only port */
            if (host_port_poll_p2() == 0) {                 /* 0x1BCD8 (0x4FCFD) */
                base = DSD(DS_00101514);                    /* 0x1BCE1 */
                DSW(base + 0x2D6u) = 0u;                    /* 0x1BCE6 */
            }
        }
    } else {                                                /* P1 is the only port */
        if (host_port_poll_p1() == 0) {                     /* 0x1BC99 (0x4FC05) */
            base = DSD(DS_00101514);                        /* 0x1BCA2 */
            if (DSW(base + 0x2D6u) == 0u)                   /* 0x1BCA8/0x1BCAF */
                DSW(base + 0x2D4u) = 0u;                    /* 0x1BCBF */
            else
                DSW(base + 0x2D6u) = 0u;                    /* 0x1BCB4 */
        }
    }
    base = DSD(DS_00101514);                                /* 0x1BCEF */
    switch (DSW(base + 0x2D4u)) {                           /* 0x1BCF4 */
    case 0: {                                               /* 0x1BD0E */
        const u8 d = input_dirs_p1();
        const u8 b = input_buttons_p1();
        const u8 f = input_f1();
        DSB(base + 0x2d8u) = (u8)(d | b | f);               /* 0x1BD53 */
        break;
    }
    case 2: {                                               /* 0x1BD15 */
        const u8 a = input_axes_a();
        const u8 b = input_port_buttons_both();
        const u8 f = input_f1();
        DSB(base + 0x2d8u) = (u8)(a | b | f);
        break;
    }
    case 4: {                                               /* 0x1BD2E */
        const u8 a = input_axes_a();
        const u8 b = input_port_buttons_p1();
        const u8 f = input_f1();
        DSB(base + 0x2d8u) = (u8)(a | b | f);
        break;
    }
    case 6: {                                               /* 0x1BD3C/0x1BD41 */
        const u8 a = input_axes_a();
        const u8 b = input_buttons_p1();
        const u8 f = input_f1();
        DSB(base + 0x2d8u) = (u8)(a | b | f);
        break;
    }
    default:                                                /* 0x1BD61: not written */
        break;
    }
    base = DSD(DS_00101514);                                /* 0x1BD61/0x1BD77 */
    switch (DSW(base + 0x2D6u)) {                           /* 0x1BD66 */
    case 0: {                                               /* 0x1BD84 */
        const u8 d = input_dirs_p2();
        const u8 b = input_buttons_p2();
        const u8 f = input_f2();
        DSB(base + 0x2d9u) = (u8)(d | b | f);               /* 0x1BDE0 */
        break;
    }
    case 2: {                                               /* 0x1BD8B */
        const u8 a = input_axes_a();
        const u8 b = input_port_buttons_both();
        const u8 f = input_f2();
        DSB(base + 0x2d9u) = (u8)(a | b | f);
        break;
    }
    case 4: {                                               /* 0x1BDA4 */
        const u8 a = input_axes_b();
        const u8 b = input_port_buttons_p2();
        const u8 f = input_f2();
        DSB(base + 0x2d9u) = (u8)(a | b | f);
        break;
    }
    case 6: {                                               /* 0x1BDC9 */
        const u8 a = input_axes_b();
        const u8 b = input_buttons_p2();
        const u8 f = input_f2();
        DSB(base + 0x2d9u) = (u8)(a | b | f);
        break;
    }
    default:                                                /* 0x1BDEE: not written */
        break;
    }
}
