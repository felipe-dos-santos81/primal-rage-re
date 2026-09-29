/* port/src/game/nameentry.c */
#include "game/nameentry.h"

#include "game/actors.h"
#include "game/config.h"
#include "game/flow.h"

#include "../mem.h"
#include "../symbols.h"

/* Addresses symbols.h has no name for (read from the raw; record §49-T.1). */
#define NE_HISCORE_CURSOR_COL   0x001044D0u   /* word: cursor column; dword >> 16 is the row */
#define NE_HISCORE_CURSOR_ROW   0x001044D2u   /* word: cursor row */
#define NE_NAME_MAX             0x0010431Au   /* dword >> 24 is the byte at 0x1043 1D */
#define NE_NAME_COUNT           0x0010431Cu   /* byte: letters typed */
#define NE_NAME_LIMIT           0x0010431Du   /* byte: letters allowed */
#define NE_INPUT_P0             0x001088E7u   /* DS_001088E4 byte 3: side 0's pad */
#define NE_INPUT_P1             0x001088E5u   /* DS_001088E4 byte 1: side 1's pad */
#define NE_CONFIG_HI            0x00104529u   /* DS_00104528 byte 1 */
#define NE_RANK_POS             0x000A7B94u   /* 4-byte rows: +0 row, +2 column */
#define NE_LETTER_ANIM          0x000A7DF4u   /* word stream offsets, per letter code */
#define NE_ANIM_WORD_C8718      0x000C8718u   /* word: the banner actors' x */
#define NE_ANIM_WORD_A7E28      0x000A7E28u
#define NE_ANIM_WORD_A7E2A      0x000A7E2Au
#define NE_FILTER_TOL           0x000FD0F0u   /* byte: unmatched letters allowed */
#define NE_FILTER_SCRATCH       0x000FD0F2u   /* word: saved tolerance counter */
#define NE_FILTER_START         0x000FD0F4u   /* dword: saved match start */
#define NE_FILTER_WORD          0x000FD0F8u   /* dword: the word being matched */
#define NE_FILTER_TABLE         0x0009AF40u   /* bad-word pointer table */
#define NE_BLANK                0x00080974u   /* the one-space string */
#define NE_CELL_CT              0x12u
#define NE_CELL(i)              (DS_00104114 + 0x14u * (u32)(i))
/* Letter-cell layout (stride 0x14): +0 actor handle, +4 x, +8 y, +0xC vel (s16),
 * +0xE accel (s16), +0x10 target (s16), +0x12 state, +0x13 letter code. */
#define NE_SAR16(a)             (((s32)DSD(a)) >> 16)

/* 0x13F68 — record §49-T. A goto transliteration: EAX = word, EDX = str, and
 * the raw's BX/EDI (the saved DS_000FD0F2/F4) live in `bx`/`edi`. */
u32 name_filter_word(u32 eax, u32 edx)
{
    u16 bx = DSW(NE_FILTER_SCRATCH);                       /* 0x13F6C */
    u32 edi = DSD(NE_FILTER_START);                        /* 0x13F73 */
    u8 cl, ch;
    u32 esi;

L_79:
    cl = DSB(edx);                                          /* 0x13F79 */
    DSD(NE_FILTER_WORD) = eax;                              /* 0x13F7B */
    if (cl == 0u) goto L_fail;                              /* 0x13F80 */
L_98:
    cl = DSB(edx);                                          /* 0x13F98 */
    ch = DSB(eax);                                          /* 0x13F9A */
    if (cl == ch) goto L_AA;                                /* 0x13F9C */
    ch = DSB(edx + 1u);                                     /* 0x13FA0 */
    edx++;                                                  /* 0x13FA3 */
    if (ch == 0u) goto L_fail;                              /* 0x13FA4 */
    goto L_98;                                              /* 0x13FA8 */
L_fail:
    DSW(NE_FILTER_SCRATCH) = bx;                            /* 0x13F84 */
    DSD(NE_FILTER_START) = edi;                             /* 0x13F8B */
    return 0u;                                              /* 0x13F91 */
L_AA:
    cl = DSB(edx);                                          /* 0x13FAA */
    edi = edx;                                              /* 0x13FAC */
    if (cl == ch) {                                         /* 0x13FAE */
        do {
            cl = DSB(edx + 1u);                             /* 0x13FB2 */
            ch = DSB(eax);                                  /* 0x13FB5 */
            edx++;                                          /* 0x13FB7 */
        } while (cl == ch);                                 /* 0x13FB8 */
    }
L_BC:
    cl = DSB(eax);                                          /* 0x13FBC */
    if (cl != 0u && cl == DSB(eax + 1u)) {                  /* 0x13FBE..0x13FC5 */
        eax++;                                              /* 0x13FC7 */
        goto L_BC;                                          /* 0x13FC8 */
    }
L_CA:
    {
        u8 nx = DSB(eax + 1u);                              /* 0x13FCA */
        eax++;                                              /* 0x13FCE */
        if (nx == 0x20u) goto L_CA;                         /* 0x13FCF */
    }
    if (DSB(eax) == 0u) goto L_22;                          /* 0x13FD4 */
    bx = DSB(NE_FILTER_TOL);                                /* 0x13FD9/0x13FDB */
L_E1:
    {
        u8 c = DSB(edx);                                    /* 0x13FE1 */
        esi = edx + 1u;                                     /* 0x13FE4 */
        if (c == 0x20u) {                                   /* 0x13FE7 */
            edx = esi;                                      /* 0x13FEC */
            goto L_E1;                                      /* 0x13FEE */
        }
    }
    cl = DSB(edx);                                          /* 0x13FF0 */
    if (cl == DSB(eax)) goto L_11;                          /* 0x13FF2 */
    if (cl == 0u || bx == 0u) {                             /* 0x13FF6..0x13FFD */
        eax = DSD(NE_FILTER_WORD);                          /* 0x13FFF */
        edx = edi + 1u;                                     /* 0x14004 */
        goto L_79;                                          /* 0x14007 */
    }
    edx = esi;                                              /* 0x1400C */
    bx--;                                                   /* 0x1400E */
    goto L_E1;                                              /* 0x1400F */
L_11:
    cl = DSB(edx + 1u);                                     /* 0x14011 */
    ch = DSB(eax);                                          /* 0x14014 */
    edx++;                                                  /* 0x14016 */
    if (cl != ch) goto L_CA;                                /* 0x14019 */
    if (cl != DSB(eax + 1u)) goto L_11;                     /* 0x1401B */
    goto L_CA;                                              /* 0x14020 */
L_22:
    for (;;) {
        DSW(NE_FILTER_SCRATCH) = bx;                        /* 0x14022 */
        DSD(NE_FILTER_START) = edi;                         /* 0x14029 */
        if (edi >= edx) break;                              /* 0x1402F `jnc` */
        DSB(edi) = 0x2Au;                                   /* 0x14033 */
        edi++;                                              /* 0x14036 */
    }
    (void)name_filter_word(DSD(NE_FILTER_WORD), edx);       /* 0x14039/0x1403E */
    return 1u;                                              /* 0x14043 */
}

/* 0x13EF0 — record §49-T. */
u32 name_filter(u32 str)
{
    DSB(NE_FILTER_TOL) =
        (DSB(NE_CONFIG_HI) & 4u) == 0u ? 2u : 0u;           /* 0x13EF8..0x13F1B */
    u8 acc = 0u;                                            /* 0x13F20 */
    for (u32 t = NE_FILTER_TABLE; DSB(DSD(t)) != 0u; t += 4u) {   /* 0x13F37..0x13F3C */
        u8 al = (u8)name_filter_word(DSD(t), str);          /* 0x13F25/0x13F27 */
        acc = (u8)(acc | al);                               /* 0x13F2F */
    }
    u32 src = str;                                          /* 0x13F40 */
    while (DSB(src) == 0x20u) src++;                        /* 0x13F42..0x13F4B */
    u32 dst = str;                                          /* 0x13F3E */
    u8 b;
    do {
        dst++;                                              /* 0x13F4D */
        b = DSB(src);                                       /* 0x13F4E */
        src++;                                              /* 0x13F50 */
        DSB(dst - 1u) = b;                                  /* 0x13F51 */
    } while (b != 0u);                                      /* 0x13F54 */
    return (u32)(s32)(s8)acc;                               /* 0x13F58/0x13F5C */
}

/* 0x1ED2C — record §49-T. */
void nameentry_reset(void)
{
    DSW(NE_HISCORE_CURSOR_COL) = 0xBu;                      /* 0x1ED3B */
    DSW(NE_HISCORE_CURSOR_ROW) = 6u;                        /* 0x1ED42 */
    frontend_input_reset();                                 /* 0x1ED49 0x4F1E4 */
    actors_reset();                                         /* 0x1ED55 0x2BAF4 (eax=1) */
    const u32 *bd = (const u32 *)(mem + 0xA7B80u);
    frontend_spawn_row(bd, 0u, 0u);                         /* 0x1ED63 0x38B18 */
    frontend_spawn_row(bd, 0x2Au, 0u);                      /* 0x1ED76 0x38B18 */
    frontend_spawn_row(bd, 0u, 0x1Eu);                      /* 0x1ED8E 0x38B18 */
    frontend_spawn_row(bd, 0x2Au, 0x1Eu);                   /* 0x1ED9F 0x38B18 */
    u32 y = ((u32)NE_SAR16(NE_HISCORE_CURSOR_COL) << 9) + 0x200u;      /* 0x1EDA4..0x1EDAF */
    u32 x = ((u32)NE_SAR16(0x001044CEu) << 9) + 0x200u;                /* 0x1EDB5..0x1EDC2 */
    DSD(DS_001044B8) = actor_spawn((const u32 *)(mem + 0xA7E30u), x, 0xF1u, y, 0u);   /* 0x1EDD2 0x2AE14 */
    DSD(DS_001044B4) = actor_spawn((const u32 *)(mem + 0xA7E44u), 0x3A00u, 0xF2u,
                                   0x1E00u, 0u);            /* 0x1EDF2 0x2AE14 */
    actors_anim_seek(DSD(DS_001044B4), DSW(NE_ANIM_WORD_A7E2A));       /* 0x1EE0F 0x2BCF4 */
    /* 0x1EE14: ECX = 0xF2 and EBX = 0x1E00 survive 0x2BCF4 (it pushes EBX; 0x2A408
     * pushes ECX), so the third spawn reuses them. */
    DSD(DS_001044B0) = actor_spawn((const u32 *)(mem + 0xA7E44u), 0x3400u, 0xF2u,
                                   0x1E00u, 0u);            /* 0x1EE20 0x2AE14 */
    actors_anim_seek(DSD(DS_001044B0), DSW(NE_ANIM_WORD_A7E28));       /* 0x1EE33 0x2BCF4 */
    for (u32 k = 0u; k < NE_CELL_CT; k++)
        DSB(NE_CELL(k) + 0x12u) = 0u;                       /* 0x1EE3A..0x1EE4A */
    DSD(DS_001044A8) = 0u;                                  /* 0x1EE4E */
    DSD(DS_001044C8) = 0u;                                  /* 0x1EE54 */
    DSD(DS_001044BC) = 0u;                                  /* 0x1EE5A */
    DSB(DS_001044D8) = 0u;                                  /* 0x1EE62 */
}

/* 0x1FFD0 — record §49-T. */
void nameentry_cells_step(void)
{
    u8 tmp[2];
    tmp[0] = (u8)DSW(DS_0001E820);                          /* 0x1FFD9/0x1FFE5 */
    tmp[1] = (u8)(DSW(DS_0001E820) >> 8);
    DSD(DS_001044AC) = 0u;                                  /* 0x1FFF0 */
    for (u32 i = 0u; i < NE_CELL_CT; i++) {                 /* 0x204DA..0x204E2 */
        u32 c = NE_CELL(i);
        u8 st = DSB(c + 0x12u);                             /* 0x1FFFA */
        if (st == 0u) continue;                             /* 0x20002 */
        DSD(DS_001044AC) = 1u;                              /* 0x20011 */
        if ((u8)(st - 1u) > 8u) continue;                   /* 0x20017 `ja` */
        u32 ebx = (u32)DSW(DS_001044C4) << 9;               /* 0x2001F..0x2002B */
        switch (st) {
        case 1u: {                                          /* 0x20036 */
            DSW(c + 0x10u) = (u16)ebx;                      /* 0x20040 */
            DSD(c + 4u) = 0x4C00u;                          /* 0x2004C */
            DSD(c + 8u) = 0x200u;                           /* 0x20054 */
            DSW(c + 0xEu) = 0x20u;                          /* 0x2005C */
            DSB(c + 0x12u) = 2u;                            /* 0x2006E */
            DSW(c + 0xCu) = 0u;                             /* 0x2007C */
            DSD(c) = actor_spawn((const u32 *)(mem + 0xA7E44u), DSD(c + 4u), 0xFFu,
                                 DSD(c + 8u), 0u);          /* 0x20088 0x2AE14 */
            actors_anim_seek(DSD(c), DSW(NE_LETTER_ANIM + 2u * DSB(c + 0x13u)));   /* 0x200AB 0x2BCF4 */
            /* PORT: 0x200B5 0x2C3FC(0xB0) and 0x200BF 0x2C3FC(0x7B) voices, not
             * wired (record §45-A). */
            break;
        }
        case 2u: {                                          /* 0x200C9 */
            DSW(c + 0xCu) = (u16)(DSW(c + 0xCu) + DSW(c + 0xEu));   /* 0x200C9..0x200D0 */
            s32 v = NE_SAR16(c + 0xAu);                     /* 0x200D7..0x200E3 */
            s32 y = (s32)DSD(c + 8u) + v;                   /* 0x200E6 */
            s32 tgt = NE_SAR16(c + 0xEu);                   /* 0x200E8..0x200EE */
            DSD(c + 8u) = (u32)y;                           /* 0x200F1 */
            if (tgt < y) {                                  /* 0x200F7 `jge` */
                DSD(c + 8u) = (u32)tgt;                     /* 0x200FD */
                DSB(c + 0x12u) = 3u;                        /* 0x20103 */
                s32 nv = -NE_SAR16(c + 0xAu);               /* 0x20109..0x20112 */
                DSW(c + 0xCu) = (u16)(nv / 4);              /* 0x20114..0x20121 (sbb rounds toward zero) */
                /* PORT: 0x20128 0x2C3FC(0x71) voice, not wired (record §45-A). */
            }
            DSD(DSD(c) + 0x1Cu) = DSD(c + 8u);              /* 0x20132..0x2013E */
            break;
        }
        case 3u: {                                          /* 0x20146 */
            s32 v = NE_SAR16(c + 0xAu);                     /* 0x20146..0x20152 */
            u16 vel = DSW(c + 0xCu);                        /* 0x20155 */
            s32 y = (s32)DSD(c + 8u) + v;                   /* 0x2015C */
            DSD(c + 8u) = (u32)y;                           /* 0x20165 */
            vel = (u16)(vel + DSW(c + 0xEu));               /* 0x2016B */
            s32 tgt = NE_SAR16(c + 0xEu);                   /* 0x2016D..0x20179 */
            DSW(c + 0xCu) = vel;                            /* 0x2017C */
            if (tgt < (s32)DSD(c + 8u)) {                   /* 0x20183 `jge` */
                DSB(c + 0x12u) = 4u;                        /* 0x20194 */
                DSW(c + 0x10u) = 0x4A00u;                   /* 0x2019D */
                DSD(c + 8u) = (u32)tgt;                     /* 0x2018E/0x2019A/0x201A9 (the old target, read before 0x2019D) */
                DSW(c + 0xEu) = 0xFFC0u;                    /* 0x201B1 */
                DSW(c + 0xCu) = 0u;                         /* 0x201B8 */
                /* PORT: 0x201C6/0x201CD 0x2C3FC(0xE7 for an odd cell, else 0xE8)
                 * voice, not wired (record §45-A). */
            }
            DSD(DSD(c) + 0x1Cu) = DSD(c + 8u);              /* 0x201D7..0x201E3 */
            break;
        }
        case 4u: {                                          /* 0x201EB */
            DSW(c + 0xCu) = (u16)(DSW(c + 0xCu) + DSW(c + 0xEu));   /* 0x201EB..0x201F2 */
            s32 v = NE_SAR16(c + 0xAu);                     /* 0x201F9..0x20208 */
            s32 x = (s32)DSD(c + 4u) + v;                   /* 0x20205/0x20208 */
            s32 tgt = NE_SAR16(c + 0xEu);                   /* 0x2020A..0x20210 */
            DSD(c + 4u) = (u32)x;                           /* 0x20213 */
            if (tgt > x) {                                  /* 0x2021B `jle` */
                DSB(c + 0x12u) = 5u;                        /* 0x20223 */
                DSW(c + 0x10u) = (u16)((((u32)DSW(DS_001044CC) + 2u * i)) << 9);   /* 0x20229..0x20240 */
            }
            DSD(DSD(c) + 0x18u) = DSD(c + 4u);              /* 0x20247..0x20253 */
            break;
        }
        case 5u: {                                          /* 0x2025B */
            s32 v = NE_SAR16(c + 0xAu);                     /* 0x2025B..0x20267 */
            s32 x = (s32)DSD(c + 4u) + v;                   /* 0x2026A */
            s32 tgt = NE_SAR16(c + 0xEu);                   /* 0x2026C..0x20272 */
            DSD(c + 4u) = (u32)x;                           /* 0x20275 */
            if (tgt > x) {                                  /* 0x2027D `jle` */
                DSB(c + 0x12u) = 6u;                        /* 0x2027F */
                DSD(c + 4u) = (u32)NE_SAR16(c + 0xEu);      /* 0x20286..0x20295 */
                DSW(c + 0xCu) = (u16)((s16)DSW(c + 0xCu) / 3);   /* 0x20295..0x202B2 cwd; idiv cx */
                DSW(c + 0x10u) = (u16)ebx;                  /* 0x202A6 */
                DSW(c + 0xEu) = 0x40u;                      /* 0x202BE */
                /* PORT: 0x202C5 0x2C3FC(0x70) and 0x202CF 0x2C3FC(0x4D) voices,
                 * not wired (record §45-A). */
            }
            DSD(DSD(c) + 0x18u) = DSD(c + 4u);              /* 0x202D4..0x202E0 */
            break;
        }
        case 6u: {                                          /* 0x202E8 */
            s32 v = NE_SAR16(c + 0xAu);                     /* 0x202E8..0x202F7 */
            s32 y = (s32)DSD(c + 8u) + v;                   /* 0x202FE */
            u16 vel = (u16)(DSW(c + 0xCu) + DSW(c + 0xEu)); /* 0x202F7/0x20300/0x2030D */
            DSD(c + 8u) = (u32)y;                           /* 0x20307 */
            DSW(c + 0xCu) = vel;                            /* 0x20315 */
            if (NE_SAR16(c + 0xEu) < (s32)DSD(c + 8u)) {    /* 0x2030F..0x20325 `jge` */
                DSB(c + 0x12u) = 7u;                        /* 0x20332 */
                DSD(c + 8u) = (u32)NE_SAR16(c + 0xEu);      /* 0x20338 */
            }
            DSD(DSD(c) + 0x1Cu) = DSD(c + 8u);              /* 0x2033E..0x2034A */
            break;
        }
        case 7u:                                            /* 0x20352 */
            DSB(c + 0x12u) = 8u;                            /* 0x2035A */
            actor_set_dead(DSD(c));                         /* 0x20360 0x2B150 */
            break;
        case 8u: {                                          /* 0x2036A */
            s32 code = (s32)DSB(c + 0x13u);                 /* 0x2036A/0x2036C */
            if (code < 0x1A) {                              /* 0x20375 `jge` */
                u8 ch = (u8)(code + 0x41);                  /* 0x20380 */
                tmp[0] = ch;                                /* 0x20385 */
                DSB(0x00104343u + i) = ch;                  /* 0x20389 */
                s32 row = 3 * (code / 7) + 6;               /* 0x2038F..0x2039E */
                s32 col = 3 * (code % 7) + 0xB;             /* 0x203A3..0x203B4 */
                text_cursor_hold_font2(col, row, tmp, 0x4000u);   /* 0x203B7 0x2F510 */
            } else if (code == 0x1A) {                      /* 0x203C1 `jnz` */
                DSB(0x00104343u + i) = 0x20u;               /* 0x203D3 */
                actors_anim_seek(DSD(DS_001044B0), DSW(NE_ANIM_WORD_A7E28));   /* 0x203D9 0x2BCF4 */
            } else if (code == 0x1B) {                      /* 0x203E0 */
                DSB(0x00104343u + i) = 0u;                  /* 0x203F8 */
                text_cursor_hold_font2((s32)(DSD(DS_001044CC) + 2u * i),
                                       (s32)DSD(DS_001044C4),
                                       mem + NE_BLANK, 0x4000u);   /* 0x20404 0x2F510 */
                (void)actor_spawn((const u32 *)(mem + 0xA7E58u),
                                  ((DSD(DS_001044CC) + 2u * i) << 9) + 0x200u, 0xFFu,
                                  (DSD(DS_001044C4) << 9) + 0x200u, 0u);   /* 0x20435 0x2AE14 */
                /* PORT: 0x2043F 0x2C3FC(0xE9) voice, not wired (record §45-A). */
            }
            text_cursor_hold_font2((s32)DSD(DS_001044CC), (s32)DSD(DS_001044C4),
                                   mem + 0x00104343u, 0x4000u);        /* 0x20459 0x2F510 */
            text_cursor_hold_font2((s32)(DSD(DS_001044CC) + 2u * i + 2u),
                                   (s32)DSD(DS_001044C4), mem + NE_BLANK, 0x4000u);   /* 0x20478 0x2F510 */
            DSB(c + 0x12u) = 0u;                            /* 0x2047F */
            break;
        }
        case 9u:                                            /* 0x20487 */
            (void)actor_spawn((const u32 *)(mem + 0xA7E58u), DSD(c + 4u) + 0x200u,
                              0xFFu, DSD(c + 8u) + 0x200u, 0u);   /* 0x204AB 0x2AE14 */
            /* PORT: 0x204B5 0x2C3FC(0xE9) voice, not wired (record §45-A). */
            actor_set_dead(DSD(c));                         /* 0x204C2 0x2B150 */
            DSB(c + 0x12u) = 0u;                            /* 0x204C7 (DL = 0) */
            break;
        default:
            break;
        }
    }
}

/* 0x204F4 — record §51-A. EAX = `rank` (only its low word is read), EDX =
 * `score`; the sole caller is 0x1EC38 at 0x1ECA7, after its own `rank < 10`
 * test. The raw first builds a ten-record 0x2C-stride table in its 0x1C0-byte
 * frame (records rank..8 moved to slots rank+1..9, records 0..rank-1 copied to
 * slots 0..rank-1, slot rank = score + 0x24 spaces) that nothing reads back
 * before 0x20702 frees the frame; the port builds it in `tbl` the same way,
 * because each 0x2DBC4 read also decodes into DS_00105EFC, which is the one
 * observable effect of that part. The rest arms the name-entry screen. */
void nameentry_arm(u32 rank, u32 score)
{
    u8 tbl[0x1C0];
    u16 si;
    for (si = (u16)rank; (s32)(u32)si < 9; si++) {          /* 0x2050C, 0x2054E..0x20556 `jl` */
        u32 r = hiscore_read(si, 0u);                       /* 0x20510..0x20517 0x2DBC4 */
        u32 slot = ((u32)si + 1u) * 0x2Cu;                  /* 0x2051C/0x2051F */
        u32 v = DSD(r);                                     /* 0x20524 */
        tbl[slot] = (u8)v; tbl[slot + 1u] = (u8)(v >> 8);
        tbl[slot + 2u] = (u8)(v >> 16); tbl[slot + 3u] = (u8)(v >> 24);   /* 0x20526 */
        for (u32 k = 0u; k < 0x24u; k++)
            tbl[slot + 4u + k] = DSB(r + 4u + k);           /* 0x2052D..0x2054B */
    }
    for (si = 0u; si < (u16)rank; si++) {                   /* 0x2055F..0x20564, 0x205A9..0x205AD `jc` */
        u32 r = hiscore_read(si, 0u);                       /* 0x20566..0x2056D 0x2DBC4 */
        u32 slot = (u32)si * 0x2Cu;                         /* 0x20574/0x20577 */
        u32 v = DSD(r);                                     /* 0x2057C */
        tbl[slot] = (u8)v; tbl[slot + 1u] = (u8)(v >> 8);
        tbl[slot + 2u] = (u8)(v >> 16); tbl[slot + 3u] = (u8)(v >> 24);   /* 0x2057E */
        for (u32 k = 0u; k < 0x24u; k++)
            tbl[slot + 4u + k] = DSB(r + 4u + k);           /* 0x20585..0x205A0 */
    }
    {
        u32 slot = (u32)(u16)rank * 0x2Cu;                  /* 0x205AF..0x205C5 */
        tbl[slot] = (u8)score; tbl[slot + 1u] = (u8)(score >> 8);
        tbl[slot + 2u] = (u8)(score >> 16); tbl[slot + 3u] = (u8)(score >> 24);   /* 0x205CE */
        for (u32 k = 0u; k < 0x24u; k++)
            tbl[slot + 4u + k] = 0x20u;                     /* 0x205D1..0x205F3 */
    }
    (void)tbl;

    u8 blank = 1u;                                          /* 0x205F5 AH */
    for (u32 i = 0u; i < 0x24u && blank != 0u; i++)         /* 0x20612..0x2061D */
        if (DSB(DS_00104367 + i) != 0x20u) blank = 0u;      /* 0x205FD..0x2060E */
    if (blank != 0u) {                                      /* 0x20621/0x20623 */
        u32 dl = 0u;
        for (;;) {
            u8 dh = DSB(DS_000A7BC0 + (u32)(u16)rank * 0x2Cu + dl);   /* 0x20625..0x20638 */
            if (dh == 0u || dl >= 0x24u) break;             /* 0x2063E..0x20645 */
            DSB(DS_00104367 + dl) = dh;                     /* 0x20647 */
            dl++;                                           /* 0x2064D */
        }
        DSB(DS_00104367 + dl) = 0u;                         /* 0x20651..0x20657 */
        DSB(DS_0010431E) = 1u;                              /* 0x2065D */
    }
    for (u32 i = 0u; i < 0x24u; i++) {                      /* 0x20664..0x20687 */
        DSB(DS_0010431F + i) = 0u;                          /* 0x2066C */
        DSB(DS_00104343 + i) = 0u;                          /* 0x20672 */
        DSB(DS_00104394 + i) = 0x20u;                       /* 0x20678 */
    }
    DSB(NE_NAME_COUNT) = 0u;                                /* 0x20699 */
    DSD(DS_00104390) = score;                               /* 0x2069F */
    DSB(DS_0010431F) = 0x20u;                               /* 0x206A8 */
    DSB(DS_001044D4) = 0u;                                  /* 0x206B3 (byte store of AL) */
    DSW(DS_0010438C) = 0x2EEu;                              /* 0x206B8 */
    if ((u16)rank == 0u) {                                  /* 0x206BF/0x206C2 */
        DSD(DS_001044CC) = 2u;                              /* 0x206D0 */
        DSD(DS_001044C4) = 0x16u;                           /* 0x206D6 */
        DSB(NE_NAME_LIMIT) = 0x12u;                         /* 0x206DC */
    } else {
        DSD(DS_001044CC) = 0x12u;                           /* 0x206F0 */
        DSD(DS_001044C4) = 0x16u;                           /* 0x206F6 */
        DSB(NE_NAME_LIMIT) = 3u;                            /* 0x206FC */
    }
}

/* 0x20710 — record §49-T. */
void nameentry_finish(u8 side)
{
    (void)name_filter(0x00104343u);                         /* 0x2071A 0x13EF0 */
    s32 count;
    u32 ecx = 0u;
    for (;;) {
        count = (s32)DSD(NE_NAME_MAX) >> 24;                /* 0x20758..0x20760 */
        if (!((s32)(u16)ecx < count)) break;                /* 0x20763..0x20768 `jl` */
        u32 a = 0x00104343u + (u16)ecx;
        u8 b = DSB(a);                                      /* 0x20725 */
        if (b == 8u || b == 0u) DSB(a) = 0x20u;             /* 0x2072B..0x20739 */
        u8 dl = DSB(a);                                     /* 0x2073F..0x20745 */
        DSB(0x00104394u + (u16)ecx) = dl;                   /* 0x2074B */
        DSB(0x00104367u + (u16)ecx) = dl;                   /* 0x20752 */
        ecx++;                                              /* 0x20751 */
    }
    u32 rec = (u32)side * 0x94u;                            /* 0x2076A..0x2077E */
    u32 charid = DSB(DS_0010782A + rec);                    /* 0x20781 */
    DSB(0x00104394u + (u32)count) = DSB(DSD(DS_000A7DA0 + charid * 4u));   /* 0x20789..0x20792 */
    if (((s32)DSD(NE_NAME_MAX) >> 24) > 3) {                /* 0x20798..0x207A3 `jle` */
        (void)hiscore_insert(0u, 0x00104390u, 1u);          /* 0x207B1 0x2DCA0 */
        DSB(0x00104397u) = DSB(DSD(DS_000A7DA0 + charid * 4u));   /* 0x207B8..0x207C7 */
    }
    for (ecx = 0u; (s32)(u16)ecx < 0xA; ecx++) {            /* 0x207CC..0x207F6 */
        u16 dx = DSW(DS_001044D6);                          /* 0x207D0 */
        if ((u16)ecx == dx)                                 /* 0x207D7 */
            (void)hiscore_insert(dx, 0x00104390u, 0u);      /* 0x207E8 0x2DCA0 */
    }
    text_cursor_hold_font2(0x13, 0x1C, mem + 0x00080978u, 0x4003u);    /* 0x207F8..0x2080C 0x2F510 */
    u32 rank = DSW(DS_001044D6);                            /* 0x20813 */
    text_cursor_hold((s32)DSB(NE_RANK_POS + rank * 4u + 2u),
                     (s32)DSB(NE_RANK_POS + rank * 4u) + 1,
                     mem + 0x0008097Cu, 0x1000u);           /* 0x20819..0x20839 0x2F4BC */
    actor_set_dead(DSD(DS_001044B8));                       /* 0x20843 0x2B150 */
    actor_set_dead(DSD(DS_001044B4));                       /* 0x2084D 0x2B150 */
    actor_set_dead(DSD(DS_001044B0));                       /* 0x20857 0x2B150 */
}

/* 0x1F458 — record §49-T. The cursor grid: columns 0xB + 3k, rows 6 + 3j; row
 * 0xF also has the DEL (column 0x1D) and END (column 0x20) cells. */
static const u32 ne_grid[28][3] = {   /* 0x1F7E0..0x1FA97: {col, row, string} */
    {0xB, 0x6, 0x8092Cu}, {0xE, 0x6, 0x80930u}, {0x11, 0x6, 0x80918u}, {0x14, 0x6, 0x80920u},
    {0x17, 0x6, 0x80934u}, {0x1A, 0x6, 0x80938u}, {0x1D, 0x6, 0x8093Cu}, {0xB, 0x9, 0x80924u},
    {0xE, 0x9, 0x80940u}, {0x11, 0x9, 0x80944u}, {0x14, 0x9, 0x80910u}, {0x17, 0x9, 0x80948u},
    {0x1A, 0x9, 0x8094Cu}, {0x1D, 0x9, 0x80950u}, {0xB, 0xC, 0x80954u}, {0xE, 0xC, 0x80958u},
    {0x11, 0xC, 0x8095Cu}, {0x14, 0xC, 0x8090Cu}, {0x17, 0xC, 0x8091Cu}, {0x1A, 0xC, 0x80914u},
    {0x1D, 0xC, 0x80960u}, {0xB, 0xF, 0x80964u}, {0xE, 0xF, 0x80968u}, {0x11, 0xF, 0x80928u},
    {0x14, 0xF, 0x8096Cu}, {0x17, 0xF, 0x80970u}, {0x1A, 0xF, 0x80974u}, {0x20, 0xF, 0xA7DC8u},
};

/* A held/pressed direction test: the side's own pad byte has `mask` set. */
static int ne_pad(u8 side, u8 mask)
{
    if ((DSB(NE_INPUT_P0) & mask) != 0u && side == 0u) return 1;   /* 0x1F4DF..0x1F4ED */
    if ((DSB(NE_INPUT_P1) & mask) != 0u && side == 1u) return 1;   /* 0x1F4EF..0x1F501 */
    return 0;
}

/* The autorepeat test: counter `ctr` above 0x1E and a multiple of 5. Its two
 * counters, DS_001044E0 and DS_001044DC, have no writer in the image (record
 * §53-A.4: the only fixups naming them are these five reads), so in the
 * original they hold the loader's BSS zero and the test is always false. */
static int ne_repeat(u32 ctr)
{
    return ctr > 0x1Eu && (ctr % 5u) == 0u;                 /* 0x1F503..0x1F520 */
}

u32 nameentry_step(u8 side)
{
    nameentry_cells_step();                                 /* 0x1F464 0x1FFD0 */
    u16 t = DSW(DS_0010438C);                               /* 0x1F469 */
    if (t != 0u) {                                          /* 0x1F473 */
        t = (u16)(t - 1u);                                  /* 0x1F477 */
        DSW(DS_0010438C) = t;                               /* 0x1F47A */
        text_number_set(0x13, 0x1C, (s32)((u32)t / 0x1Eu), 2, 1u, 0x4003u);   /* 0x1F4A8 0x2F434 */
    }
    if (DSW(DS_0010438C) == 0u && DSD(DS_001044AC) == 0u) {   /* 0x1F4AD..0x1F4BE */
        nameentry_finish(side);                             /* 0x1F4C6 0x20710 */
        return 0u;                                          /* 0x1F4CB */
    }
    if (DSB(DS_001044D8) == 0u) {                           /* 0x1F4D2 */
        if (ne_pad(side, 0x10u) || ne_repeat(DSD(DS_001044E0))) {   /* 0x1F4DF..0x1F520 */
            /* PORT: 0x1F526 0x2C3FC(0xE6) voice, not wired (record §45-A). */
            u32 eax = DSD(NE_HISCORE_CURSOR_COL);           /* 0x1F537 */
            DSW(NE_HISCORE_CURSOR_COL) = (u16)(DSW(NE_HISCORE_CURSOR_COL) + 3u);   /* 0x1F530..0x1F542 */
            s32 row = (s32)eax >> 16;                       /* 0x1F53F */
            s32 col = NE_SAR16(0x001044CEu);                /* 0x1F54E/0x1F553 */
            if (col > (row == 0xF ? 0x20 : 0x1D)) {         /* 0x1F549..0x1F559, 0x1F576..0x1F579 */
                DSW(NE_HISCORE_CURSOR_COL) = 0xBu;          /* 0x1F565, 0x1F585 */
                /* PORT: 0x1F560/0x1F580 0x2C3FC(0x39) voice, not wired (record §45-A). */
            } else {
                /* PORT: 0x1F58E 0x2C3FC(0x34) voice, not wired (record §45-A). */
            }
            DSD(DSD(DS_001044B8) + 0x18u) =
                ((u32)NE_SAR16(0x001044CEu) << 9) + 0x200u; /* 0x1F598..0x1F5AE */
        } else if (ne_pad(side, 0x20u) || ne_repeat(DSD(DS_001044DC))) {   /* 0x1F5B6..0x1F5F6 */
            /* PORT: 0x1F5F8 0x2C3FC(0xE6) voice, not wired (record §45-A). */
            DSW(NE_HISCORE_CURSOR_COL) = (u16)(DSW(NE_HISCORE_CURSOR_COL) - 3u);   /* 0x1F602 */
            if (NE_SAR16(0x001044CEu) < 0xB) {              /* 0x1F60A..0x1F615 `jge` */
                DSW(NE_HISCORE_CURSOR_COL) = NE_SAR16(NE_HISCORE_CURSOR_COL) == 0xF ? 0x20u : 0x1Du;   /* 0x1F617..0x1F62F */
                /* PORT: 0x1F638 0x2C3FC(0x35) voice, not wired (record §45-A). */
            } else {
                /* PORT: 0x1F63F 0x2C3FC(0x38) voice, not wired (record §45-A). */
            }
            DSD(DSD(DS_001044B8) + 0x18u) =
                ((u32)NE_SAR16(0x001044CEu) << 9) + 0x200u; /* 0x1F649..0x1F65F */
        } else if (ne_pad(side, 0x80u) || ne_repeat(DSD(DS_001044E0))) {   /* 0x1F667..0x1F6A7 */
            /* PORT: 0x1F6A9 0x2C3FC(0xE6) voice, not wired (record §45-A). */
            DSW(NE_HISCORE_CURSOR_ROW) = (u16)(DSW(NE_HISCORE_CURSOR_ROW) - 3u);   /* 0x1F6B3 */
            if (NE_SAR16(NE_HISCORE_CURSOR_COL) < 6)        /* 0x1F6BB..0x1F6C6 `jge` */
                DSW(NE_HISCORE_CURSOR_ROW) = 0xFu;          /* 0x1F6C8 */
            if (NE_SAR16(0x001044CEu) == 0x20) {            /* 0x1F6D1..0x1F6DC */
                /* PORT: 0x1F6DE 0x2C3FC(0x39) voice, not wired (record §45-A). */
                DSW(NE_HISCORE_CURSOR_COL) = 0x1Du;         /* 0x1F6F2 */
                DSD(DSD(DS_001044B8) + 0x18u) = 0x3C00u;    /* 0x1F6F9 */
            } else {
                /* PORT: 0x1F705 0x2C3FC(0x37) voice, not wired (record §45-A). */
            }
            DSD(DSD(DS_001044B8) + 0x1Cu) =
                ((u32)NE_SAR16(NE_HISCORE_CURSOR_COL) << 9) + 0x200u;   /* 0x1F7BA..0x1F7D0 */
        } else if (ne_pad(side, 0x40u) || ne_repeat(DSD(DS_001044DC))) {   /* 0x1F714..0x1F754 */
            /* PORT: 0x1F75A 0x2C3FC(0xE6) voice, not wired (record §45-A). */
            DSW(NE_HISCORE_CURSOR_ROW) = (u16)(DSW(NE_HISCORE_CURSOR_ROW) + 3u);   /* 0x1F764 */
            if (NE_SAR16(NE_HISCORE_CURSOR_COL) > 0xF)      /* 0x1F76C..0x1F777 `jle` */
                DSW(NE_HISCORE_CURSOR_ROW) = 6u;            /* 0x1F779 */
            if (NE_SAR16(0x001044CEu) == 0x20) {            /* 0x1F782..0x1F78D */
                /* PORT: 0x1F78F 0x2C3FC(0x38) voice, not wired (record §45-A). */
                DSW(NE_HISCORE_CURSOR_COL) = 0x1Du;         /* 0x1F799 */
                DSD(DSD(DS_001044B8) + 0x18u) = 0x3C00u;    /* 0x1F7A7 */
            } else {
                /* PORT: 0x1F7B0 0x2C3FC(0x36) voice, not wired (record §45-A). */
            }
            DSD(DSD(DS_001044B8) + 0x1Cu) =
                ((u32)NE_SAR16(NE_HISCORE_CURSOR_COL) << 9) + 0x200u;   /* 0x1F7BA..0x1F7D0 */
        }
    }
    if (DSD(DS_001044AC) == 0u) {                           /* 0x1F7D3 */
        for (u32 g = 0u; g < 28u; g++)
            text_cursor_hold_font2((s32)ne_grid[g][0], (s32)ne_grid[g][1],
                                   mem + ne_grid[g][2], 0x4000u);   /* 0x1F7F4..0x1FA97 0x2F510 */
    }
    text_cursor_hold(1, 1, game_string_get(0x18u), 0x1000u);   /* 0x1FA9C..0x1FAB4 0x1C500, 0x2F4BC */
    text_number_draw(1, 2, (s32)DSD(0x001077ECu + (u32)side * 0x94u), 7, 3u, 0x4000u);   /* 0x1FAB9..0x1FAEA 0x2F4D0 */
    text_cursor_hold(0x23, 1, game_string_get(0x19u), 0x1000u);   /* 0x1FAEF..0x1FB0A */
    text_number_draw(0x24, 2, (s32)DSW(DS_001044D6) + 1, 2, 1u, 0x4000u);   /* 0x1FB0F..0x1FB2C 0x2F4D0 */
    if ((DSW(DS_000EF6DC) & 0x1Fu) == 0u) {                 /* 0x1FB31..0x1FB40 */
        u32 mode = (DSW(DS_000EF6DC) & 0x20u) != 0u ? 0x1000u : 0x4000u;   /* 0x1FB46..0x1FB55 */
        if (side != 0u)                                     /* 0x1FB57/0x1FB87 */
            text_cursor_hold(0x19, 0x1C, game_string_get(0x16u), mode);   /* 0x1FB5E..0x1FBC6 */
        else
            text_cursor_hold(3, 0x1C, game_string_get(0x17u), mode);      /* 0x1FB7B..0x1FBC6 */
    }
    if (DSW(DS_001044D6) != 0u) {                           /* 0x1FBCB */
        if ((DSB(NE_CONFIG_HI) & 2u) != 0u) {               /* 0x1FBD9 */
            if (DSD(DS_001044A8) == 0u) {                   /* 0x1FBE2..0x1FBEA */
                DSD(DS_001044A8) = actor_spawn((const u32 *)(mem + 0xA7E6Cu),
                                               DSW(NE_ANIM_WORD_C8718), 0xFFu, 0x400u, 0u);   /* 0x1FC07 0x2AE14 */
                (void)actor_spawn((const u32 *)(mem + 0xA7E80u),
                                  DSW(NE_ANIM_WORD_C8718), 0xFFu, 0x880u, 0u);   /* 0x1FC2B 0x2AE14 */
            }
        } else {
            text_cursor_hold(-1, 1, game_string_get(0x1Au), 0x2000u);   /* 0x1FC35..0x1FC50 */
            text_cursor_hold(-1, 3, game_string_get(0x1Bu), 0x3000u);   /* 0x1FCDE..0x1FCF4 */
        }
    } else {
        if ((DSB(NE_CONFIG_HI) & 2u) != 0u) {               /* 0x1FC5F */
            if (DSD(DS_001044A8) == 0u) {                   /* 0x1FC68..0x1FC6F */
                DSD(DS_001044A8) = actor_spawn((const u32 *)(mem + 0xA7E94u),
                                               DSW(NE_ANIM_WORD_C8718), 0xFFu, 0x400u, 0u);   /* 0x1FC8E 0x2AE14 */
                (void)actor_spawn((const u32 *)(mem + 0xA7EA8u),
                                  DSW(NE_ANIM_WORD_C8718), 0xFFu, 0x880u, 0u);   /* 0x1FCB2 0x2AE14 */
            }
        } else {
            text_cursor_hold(-1, 1, game_string_get(0x1Cu), 0x2000u);   /* 0x1FCB9..0x1FCD4 */
            text_cursor_hold(-1, 3, game_string_get(0x1Du), 0x3000u);   /* 0x1FCDE..0x1FCF4 */
        }
    }
    /* 0x1FCF9: a face-button press by this side, or a queued letter, takes one. */
    if (!(ne_pad(side, 0x0Fu) || DSD(DS_001044C8) != DSD(DS_001044BC)))   /* 0x1FCF9..0x1FD28 */
        return 1u;                                          /* 0x1FF9B */
    s32 letter;
    if (DSD(DS_001044C8) == DSD(DS_001044BC)) {             /* 0x1FD2E..0x1FD39 */
        s32 row = NE_SAR16(NE_HISCORE_CURSOR_COL);          /* 0x1FD3B..0x1FD41 */
        s32 col = NE_SAR16(0x001044CEu);                    /* 0x1FD53..0x1FD5E */
        letter = 7 * ((row - 6) / 3) + (col - 0xB) / 3;     /* 0x1FD44..0x1FD76 */
    } else {
        u32 q = DSD(DS_001044C8) + 1u;                      /* 0x1FD7E */
        letter = (s32)DSD(DS_00104458 + q * 4u);            /* 0x1FD7F */
        DSD(DS_001044C8) = q & 0xFu;                        /* 0x1FD88..0x1FD93 */
        if (letter != 0x1B && letter != 0x1C) {             /* 0x1FD99..0x1FDA1 */
            DSW(NE_HISCORE_CURSOR_COL) = (u16)(3 * (letter % 7) + 0xB);   /* 0x1FDA3..0x1FDBB */
            DSW(NE_HISCORE_CURSOR_ROW) = (u16)(3 * (letter / 7) + 6);     /* 0x1FDC1..0x1FDDE */
            u32 rec = DSD(DS_001044B8);                     /* 0x1FDE7 */
            DSD(rec + 0x18u) = ((u32)NE_SAR16(0x001044CEu) << 9) + 0x200u;   /* 0x1FDD2..0x1FDF2 */
            DSD(rec + 0x1Cu) = ((u32)NE_SAR16(NE_HISCORE_CURSOR_COL) << 9) + 0x200u;   /* 0x1FDF5..0x1FE07 */
        }
    }
    if (letter == 0x1B) {                                   /* 0x1FE0A..0x1FE10 DEL */
        s8 n = (s8)DSB(NE_NAME_COUNT);                      /* 0x1FE12 */
        if (n <= 0) return 1u;                              /* 0x1FE1A */
        DSB(NE_NAME_COUNT) = (u8)(n - 1);                   /* 0x1FE22..0x1FE24 */
        u32 cell = NE_CELL((u32)(s32)((s32)DSD(0x00104319u) >> 24));   /* 0x1FE2A..0x1FE3C */
        u8 old = DSB(cell + 0x12u);                         /* 0x1FE44 */
        DSW(DS_0010438C) = 0x1C2u;                          /* 0x1FE4A */
        if (old != 0u) {                                    /* 0x1FE51 */
            DSB(cell + 0x12u) = 9u;                         /* 0x1FE55 */
        } else {
            DSB(cell + 0x12u) = 8u;                         /* 0x1FE66 */
            DSB(cell + 0x13u) = (u8)letter;                 /* 0x1FE6C */
        }
        return 1u;                                          /* 0x1FE72 */
    }
    if (letter == 0x1C) {                                   /* 0x1FE77 END */
        DSB(0x00104343u + (u32)((s32)DSD(0x00104319u) >> 24)) = 0u;   /* 0x1FE7C..0x1FE88 */
        DSW(DS_0010438C) = 0u;                              /* 0x1FE8E */
        return 1u;                                          /* 0x1FE95 */
    }
    if ((s8)DSB(NE_NAME_COUNT) >= (s8)DSB(NE_NAME_LIMIT))   /* 0x1FEA3..0x1FEAE `jge` */
        return 1u;
    DSW(DS_0010438C) = 0x1C2u;                              /* 0x1FEBC */
    if (letter == 0x1A) {                                   /* 0x1FEC3 */
        actors_anim_seek(DSD(DS_001044B0), 0x1E1u);         /* 0x1FED2 0x2BCF4 */
    } else {
        text_cursor_hold_font2(NE_SAR16(0x001044CEu), NE_SAR16(NE_HISCORE_CURSOR_COL),
                               mem + NE_BLANK, 0x4000u);    /* 0x1FEF4 0x2F510 */
    }
    (void)actor_spawn((const u32 *)(mem + 0xA7E58u),
                      ((u32)NE_SAR16(0x001044CEu) << 9) + 0x200u, 0xFFu,
                      ((u32)NE_SAR16(NE_HISCORE_CURSOR_COL) << 9) + 0x200u, 0u);   /* 0x1FF27 0x2AE14 */
    /* PORT: 0x1FF31 0x2C3FC(0xE9) voice, not wired (record §45-A). */
    u32 cell = NE_CELL((u32)(s32)((s32)DSD(0x00104319u) >> 24));   /* 0x1FF36..0x1FF46 */
    DSB(cell + 0x12u) = 1u;                                 /* 0x1FF4D */
    DSB(cell + 0x13u) = (u8)letter;                         /* 0x1FF5A */
    u8 n = (u8)(DSB(NE_NAME_COUNT) + 1u);                   /* 0x1FF54..0x1FF61 */
    DSB(NE_NAME_COUNT) = n;                                 /* 0x1FF69 */
    if (n == DSB(NE_NAME_LIMIT)) {                          /* 0x1FF63..0x1FF71 */
        DSW(NE_HISCORE_CURSOR_COL) = 0x20u;                 /* 0x1FF73 */
        DSD(DSD(DS_001044B8) + 0x18u) = 0x4200u;            /* 0x1FF86 */
        DSW(NE_HISCORE_CURSOR_ROW) = 0xFu;                  /* 0x1FF8D */
        DSD(DSD(DS_001044B8) + 0x1Cu) = 0x2000u;            /* 0x1FF94 */
    }
    return 1u;                                              /* 0x1FF9B */
}

/* 0x20860 — record §53-A. EAX = the key's ascii byte (0x24D63 `xor eax,eax;
 * mov al,bl`, never 0 there). When the queue is not full (the masked
 * write index + 1 differs from the read index DS_001044C8), backspace,
 * Enter and space become DEL (0x1B), END (0x1C) and the space letter (0x1A);
 * any other byte is kept only when the C runtime's class byte for it (the
 * byte at DS_00081C84 + (u8)(c + 1), read as the dword at DS_00081C81 + idx
 * shifted right by 24) has bit 6 (upper) or 7 (lower) set, and then becomes
 * its upper-case letter minus 'A' (0x653ED: 'a'..'z' minus 0x20, signed
 * compares). The entry is stored at (write + 1) * 4 before the & 0xF, like the
 * reader 0x1FD7F, so entry 16 (0x104498) is live (record §49-T.3 #2). A full ring stores nothing
 * but still sets DS_001044D8; a rejected byte returns before it (0x208B0 jumps
 * to the pops at 0x208E0). */
void nameentry_key(u32 c)
{
    u32 edx = c;                                            /* 0x20863 */
    if (((DSD(DS_001044BC) + 1u) & 0xFu) != DSD(DS_001044C8)) {   /* 0x20865..0x20876 */
        if (edx == 8u) {                                    /* 0x20878 */
            edx = 0x1Bu;                                    /* 0x2087D */
        } else if (edx == 0xDu) {                           /* 0x20884 */
            edx = 0x1Cu;                                    /* 0x20889 */
        } else if (edx == 0x20u) {                          /* 0x20890 */
            edx = 0x1Au;                                    /* 0x20895 */
        } else {
            u32 idx = (u8)(edx + 1u);                       /* 0x2089C..0x208A0 */
            s32 cls = (s32)DSD(DS_00081C81 + idx) >> 24;    /* 0x208A5/0x208AB */
            if ((cls & 0xC0) == 0) return;                  /* 0x208AE/0x208B0 */
            s32 up = (s32)edx;                              /* 0x208B2 */
            if (up >= 0x61 && up <= 0x7A) up -= 0x20;       /* 0x208B4 0x653ED */
            edx = (u32)(up - 0x41);                         /* 0x208B9 */
        }
        /* 0x208C4 stores the unmasked index first; 0x208D3 overwrites it. */
        u32 w = DSD(DS_001044BC) + 1u;                      /* 0x208BC/0x208C1 */
        DSD(DS_00104458 + w * 4u) = edx;                    /* 0x208CC */
        DSD(DS_001044BC) = w & 0xFu;                        /* 0x208C9/0x208D3 */
    }
    DSB(DS_001044D8) = 1u;                                  /* 0x208D9 */
}
