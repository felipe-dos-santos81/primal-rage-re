/* port/src/game/attract.c
 * The small attract/boot units (0x292AC, 0x10EE4, 0x10F28, 0x10DB0, 0x10E18
 * and 0x2C8F0 with eax = -2). The raw-byte derivation of every body is in
 * docs/superpowers/plans/2026-09-19-attract-derivations.md. */
#include "game/attract.h"
#include "game/actors.h"
#include "game/config.h"
#include "game/effects.h"
#include "game/flow.h"
#include "game/movie.h"
#include "game/rng.h"
#include "mem.h"
#include "symbols.h"

#include <stdio.h>

/* 0x292AC. Iterate the mask DS_00104AD0; for each set bit call the function
 * address at DS_000A8744 + i*4, where i is the byte offset (0, 4, 8, ...). */
void attract_scene_tick(void)
{
    u32 mask = DSD(DS_00104AD0);
    for (u32 i = 0; mask != 0; i += 4u, mask >>= 1) {
        if ((mask & 1u) == 0u) continue;
        /* PORT: the raw passes eax = i to the callee (`mov eax,ebx; call
         * [eax+0x28744]`). The shipped table's two callees ignore eax: 0x5D812
         * is `xor eax,eax; ret`, and 0x4F7F4 overwrites eax from DS_000F0A48
         * before any use. So fn_resolve's no-arg form is faithful for the
         * shipped table; an unregistered (unported) entry is skipped. */
        void (*fn)(void) = fn_resolve(DSD(DS_000A8744 + i));
        if (fn) fn();
    }
}

/* 0x10EE4. The attract state reset. */
void attract_state_reset(void)
{
    /* PORT: 0x32970(eax = 0), the run-clock/tick update, is out of scope
     * (spec §7); the host clock owns wall time. */
    /* 0x4F1E4: DS_00104B15 = 0. The raw's 0x2EA30 interrupt-lock pair is inert
     * in the port's single-threaded loop; flow.c's title_input_reset() is the
     * same one-byte write. */
    DSB(DS_00104B15) = 0;
    actors_reset();                             /* 0x2BAF4(1) */
    DSW(DS_00104B00) = 3;                       /* 0x10F05: edx = 3 */
    DSW(DS_000F0A64) = 0;
    DSB(DS_000F0A71) = 0;
    DSB(DS_000F0A6F) = 0;
}

/* 0x10F28. The attract's voice/rng scheduler. */
void attract_voice_tick(void)
{
    DSW(DS_000F0A60) = (u16)(DSW(DS_000F0A60) - 1u);
    if ((s16)DSW(DS_000F0A60) <= 0) {
        /* PORT: 0x2C3FC(0xBD) voice, out of scope (spec §7). */
        DSW(DS_000F0A60) = (u16)(rng_next(0x2Du) + 0x2Du);
    }
    DSW(DS_000F0A62) = (u16)(DSW(DS_000F0A62) - 1u);
    if ((s16)DSW(DS_000F0A62) <= 0) {
        /* PORT: the raw draws rng_next(2) to pick 0xBE (nonzero) or 0xBF; the
         * 0x2C3FC call is out of scope (spec §7), but the draw must stay to
         * keep the shared rng stream faithful. */
        u32 pick = rng_next(2u);
        (void)pick;
        /* PORT: 0x2C3FC(pick ? 0xBE : 0xBF) voice, out of scope (spec §7). */
        DSW(DS_000F0A62) = (u16)(rng_next(0x3Cu) + 0x3Cu);
    }
}

/* 0x10DB0. The per-state pause tail (state 4). */
void frontend_pause_tail(void)
{
    if (DSB(DS_000F0A71) != 0u) return;
    if ((DSB(DS_001088D8 + 3u) & 0x20u) == 0u) return;
    if ((DSB(DS_001088D8 + 1u) & 0x10u) == 0u) return;
    DSB(DS_000F0A71) = 1;
    if (DSB(DS_00104B19 + 2u) != 0u) {
        DSB(DS_00104B19 + 2u) = 0;
        DSB(DS_00104B15) = 0;
        DSW(DS_000F0A6C) = 4;
        DSW(DS_000F0A64) = 4;
        /* PORT: 0x29D60 is a ret-only no-op; the raw then calls 0x2C3FC(0x100)
         * (voice, out of scope, spec §7). */
        return;
    }
    DSW(DS_000F0A64) = 4;
}

/* 0x10E18. The per-state continue tail (state 5). */
void frontend_continue_tail(void)
{
    if (DSB(DS_000F0A71) != 0u) return;
    if ((DSB(DS_001088D8 + 3u) & 0x10u) == 0u) return;
    if ((DSB(DS_001088D8 + 1u) & 0x20u) == 0u) return;
    DSB(DS_000F0A71) = 1;
    if (DSB(DS_00104B19 + 2u) != 0u) {
        DSB(DS_00104B19 + 2u) = 0;
        DSB(DS_00104B15) = 0;
        DSW(DS_000F0A6C) = 5;
        DSW(DS_000F0A64) = 5;
        /* PORT: 0x29D60 is a ret-only no-op; the raw then calls 0x2C3FC(0x100)
         * (voice, out of scope, spec §7). */
        return;
    }
    DSW(DS_000F0A64) = 5;
}

/* 0x2C8F0 with eax = -2. */
void attract_config_volumes(void)
{
    u32 scale = config_field_get(0x2Au) & 3u;
    u32 m = config_field_get(0x35u);
    u32 s = config_field_get(0x37u);
    /* PORT: 0x1CAB8 (music) and 0x1CED4 (SFX) store the value into
     * DS_000A2CB8/DS_000A2CB4 and push it to the AIL device. The port's
     * game_audio_service applies both globals every frame (flow.c:683/706), so
     * writing the globals here is the port's realization of the two setters.
     * The raw multiplies with imul, divides by 3 with idiv (truncation toward
     * zero) and halves with sar 1. */
    DSD(DS_000A2CB8) = (m == 0xFFFFFFFFu) ? 8u
                      : (u32)(((s32)(m * scale) / 3) >> 1);
    DSD(DS_000A2CB4) = (s == 0xFFFFFFFFu) ? 0x10u
                      : (u32)(((s32)(s * scale) / 3) >> 1);
}

/* ---- the 0x11000 attract sub-machine ------------------------------------ */

static const char *s_media_dir;

void attract_set_media_dir(const char *dir) { s_media_dir = dir; }

void attract_step(void)
{
    switch (DSB(DS_000F0A6F)) {
    case 0:
        /* PORT: 0x2C3FC(0x100) voice, out of scope (spec §7). */
        DSB(DS_0009AD58) = 1;                       /* 0x11029 */
        /* 0x11035: 0x4F1E4 (title_input_reset) ignores eax. */
        DSB(DS_00104B15) = 0;                       /* 0x4F1E4 */
        actors_reset();                             /* 0x1103F (0x2BAF4) */
        frontend_origin_zero();                     /* 0x11046 (0x4F1D0) */
        /* PORT: 0x1C740(mem + 0x80038) / 0x1C740(mem + 0x80044) play the two
         * boot logos; those data pointers hold the fixed names "twi5.smk" and
         * "twg.smk". A missing or rejected movie is skipped, never fatal. */
        if (!movie_play(s_media_dir, "twi5.smk"))
            fprintf(stderr, "attract: twi5.smk playback failed\n");
        if (!movie_play(s_media_dir, "twg.smk"))
            fprintf(stderr, "attract: twg.smk playback failed\n");
        DSB(DS_000F0A6F) = 1;                       /* 0x1105F */
        break;

    case 1:
        actors_reset();                             /* 0x11070 (0x2BAF4) */
        frontend_origin_zero();                     /* 0x11079 (0x4F1D0) */
        DSB(DS_000F0A6F) = 2;                       /* 0x1107E (ch = 2) */
        break;

    case 2: {
        /* 0x11089: DS_000F0A5C = old + 1, wrapped to 0 at >= 4. */
        u8 c = (u8)(DSB(DS_000F0A5C) + 1u);
        DSB(DS_0009AD58) = 0;                       /* 0x11092 */
        if (c >= 4u) c = 0u;                        /* 0x110A1 (jl) */
        DSB(DS_000F0A5C) = c;                       /* 0x11098/0x110A5 */
        attract_config_volumes();                   /* 0x110B0 (0x2C8F0, eax = -2) */
        /* 0x110BA: 0x4F1E4 (title_input_reset) ignores eax. */
        DSB(DS_00104B15) = 0;                       /* 0x4F1E4 */
        actors_reset();                             /* 0x110C4 (0x2BAF4) */
        config_set_credit_row(1u);                  /* 0x110CE (0x2C06C, eax = 1) */
        DSD(DS_000F0A48) = palette_acquire(0x396ED28u);   /* 0x110D8 */
        palette_acquire(0x396ECE8u);                /* 0x110E7 */
        palette_acquire(0x396EAE8u);                /* 0x110F1 */
        palette_acquire(0x396EAE8u);                /* 0x110FB */
        palette_acquire(0x396E9E8u);                /* 0x11105 */
        palette_acquire(0x396E9E8u);                /* 0x11111 */
        palette_acquire(0x396EC68u);                /* 0x1111D */
        palette_acquire(0x105FE30u);                /* 0x11129 */
        /* 0x11138: eax = desc, edx = a2 = 0, ebx = a3 = 0. The raw's
         * `mov esi, 0x3C` is a live value for DS_000F0A62, not an argument. */
        frontend_spawn_row((const u32 *)(mem + 0x9AC08u), 0u, 0u);
        /* PORT: 0x13C70 also receives edi = 0xB4 in the raw, a register the
         * port's 3-argument effects_spawn does not model; the same value is the
         * DS_000F0A68 countdown stored below. */
        effects_spawn(DSD(DS_000F0A48), 4u, 0x396ED28u);   /* 0x11151 */
        /* PORT: 0x2C3FC(0x40) and 0x2C3FC(0x42) voices, out of scope (spec
         * §7). The raw keeps ecx = 0x2D and bh = 3 live across them (item 2);
         * those are the DS_000F0A60 / DS_000F0A70 values stored below. */
        DSW(DS_000F0A60) = 0x2Du;                   /* 0x11171 */
        DSW(DS_000F0A62) = 0x3Cu;                   /* 0x11178 */
        DSW(DS_000F0A68) = 0xB4u;                   /* 0x1117F */
        DSB(DS_000F0A70) = 3u;                      /* 0x11188 */
        DSB(DS_000F0A6F) = 0xCu;                    /* 0x1118E */
        break;
    }

    case 3:
        DSD(DS_000F0A50) = actor_spawn((const u32 *)(mem + 0x9ACCCu),
                                       0x2A00u, 0xE0u, 0x1E00u, 0u);   /* 0x111AF */
        DSB(DS_000F0A6F) = 4;                       /* 0x111BB */
        break;

    case 4: {
        u32 rec = DSD(DS_000F0A50);
        u16 v = (u16)(DSW(rec + 0x2Cu) - 0x100u);   /* 0x111D2 */
        DSW(rec + 0x2Cu) = v;
        if (v < 0x1001u) {                          /* 0x111E4 (jg) */
            /* PORT: 0x2C3FC(DS_000F0A5C == 0 ? 0x54 : 0x56) voice, out of
             * scope (spec §7). */
            DSB(DS_000F0A6F) = 5;                   /* 0x1120B */
            DSW(rec + 0x2Cu) = 0x1000u;             /* 0x11211 */
        }
        break;
    }

    case 5:
        if ((DSD(DSD(DS_000F0A50) + 0x24u) & 0x7FFFFFFFu) == 0u) {   /* 0x11221 */
            DSD(DS_000F0A50) = actor_spawn((const u32 *)(mem + 0x9ACE0u),
                                           0u, 0u, 0xE2u, 0u);   /* 0x1123E */
            DSB(DS_000F0A6F) = 6;                   /* 0x11248 */
        }
        break;

    case 6:
        if ((DSD(DSD(DS_000F0A50) + 0x24u) & 0x7FFFFFFFu) == 0u) {   /* 0x11259 */
            DSD(DS_000F0A50) = actor_spawn((const u32 *)(mem + 0x9ACF4u),
                                           0u, 0u, 0xE6u, 0u);   /* 0x11276 */
            DSB(DS_000F0A6F) = 7;                   /* 0x11280 */
        }
        break;

    case 7:
        if ((DSD(DSD(DS_000F0A50) + 0x24u) & 0x7FFFFFFFu) == 0u) {   /* 0x11291 */
            actor_set_dead(DSD(DS_000F0A50));       /* 0x1129E */
            DSD(DS_000F0A4C) = actor_spawn((const u32 *)(mem + 0x9AD1Cu),
                                           0u, 0u, 0xE8u, 0u);   /* 0x112B3 */
            DSB(DS_000F0A6F) = 8;                   /* 0x112BF */
        }
        break;

    case 8:
        if ((DSD(DSD(DS_000F0A4C) + 0x24u) & 0x7FFFFFFFu) == 0u) {   /* 0x112CF */
            DSW(DS_000F0A68) = 0x40u;               /* 0x112E5 */
            DSB(DS_000F0A70) = 9u;                  /* 0x112EC */
            DSB(DS_000F0A6F) = 0xCu;                /* 0x112F2 */
        }
        break;

    case 9: {
        if ((DSB(DS_00104528 + 1u) & 2u) == 0u)                          /* 0x11304 */
            text_cursor_set(-1, 0x15, game_string_get(3u), 0x1000u);     /* 0x11321 */
        /* 0x11335: eax = 0x1EB's width; the raw's `sar edx,31; sub; sar 1`
         * is a signed truncating /2, and esi = 0x15 - width/2 is the column
         * both entry strings center on. */
        s32 col = 0x15 - (text_width(game_string_get(0x1EBu), 0x2000u) / 2);
        text_cursor_set(col, 0x17, game_string_get(0x1EBu), 0x2000u);    /* 0x11362 */
        /* 0x1136C: ebx = mem + 0x9ACC4, the data pointer to the string "@". */
        text_cursor_set(col + 5, 0x17, (const u8 *)(mem + 0x9ACC4u),
                        0x2000u);                                        /* 0x11379 */
        text_cursor_set(-1, 0x18, game_string_get(0x1ECu), 0x2000u);     /* 0x11399 */
        text_cursor_set(-1, 0x19, game_string_get(4u), 0x2000u);         /* 0x113B9 */
        text_cursor_set(-1, 0x1B, game_string_get(0x1EDu), 0x3000u);     /* 0x113D9 */
        /* PORT: 0x113DE's ebx = 0x4C is an LE fixup to mem + 0x8004C, the
         * fixed string "16 Meg Release"; ecx = 0 is the mode. */
        text_cursor_set(-1, 0x1D, (const u8 *)(mem + 0x8004Cu), 0u);     /* 0x113EF */
        if ((DSB(DS_00104528 + 1u) & 2u) != 0u) {                       /* 0x113FB */
            actor_spawn((const u32 *)(mem + 0x9AD44u),
                        0x2A00u, 0xFFu, 0x780u, 0u);                     /* 0x11413 */
        } else {
            text_cursor_set(-1, 4, game_string_get(0x1EAu), 0x4003u);    /* 0x11435 */
        }
        if (DSB(DS_000F0A5C) == 0u) {                                   /* 0x11441 */
            DSW(DS_000F0A68) = 0xF0u;                                    /* 0x1144A */
            DSB(DS_000F0A70) = 0xBu;                                     /* 0x11450 */
        } else {
            DSW(DS_000F0A68) = 0x78u;                                    /* 0x1145F */
            DSB(DS_000F0A70) = 10u;                                      /* 0x11466 */
        }
        DSB(DS_000F0A6F) = 0xCu;                                        /* 0x1146C */
        break;
    }

    case 0xA:
        /* 0x11478: the actor_spawn return value is discarded in the raw. a2
         * and a4 are the high words of the data dwords at 0x9ACC4/0x9ACC6. */
        actor_spawn((const u32 *)(mem + 0x9AD30u),
                    (u32)((s32)DSD(0x9ACC4u) >> 16), 0xD0u,
                    (u32)((s32)DSD(0x9ACC6u) >> 16), 0u);
        DSB(DS_000F0A70) = 0xBu;                    /* 0x114A2 */
        DSW(DS_000F0A68) = 0xF0u;                   /* 0x114AA */
        DSB(DS_000F0A6F) = 0xCu;                    /* 0x114B1 */
        break;

    case 0xB:
        DSD(DS_000F0A48) = 0u;                      /* 0x114C0 */
        DSB(DS_000F0A6F) = 0u;                      /* 0x114CC */
        if (DSB(DS_00108173) != 0u) {               /* 0x114D2 */
            DSW(DS_000F0A64) = 8u;                  /* 0x114D6 */
        } else if (DSB(DS_000F0A5C) == 0u) {        /* 0x114E4 */
            DSW(DS_000F0A64) = 1u;                  /* 0x114EE */
        } else {
            if (DSB(DS_000F0A5C) == 1u)      DSB(DS_000F0A72) = 5u;   /* 0x1150E */
            else if (DSB(DS_000F0A5C) == 2u) DSB(DS_000F0A72) = 0u;   /* 0x11517 */
            else if (DSB(DS_000F0A5C) == 3u) DSB(DS_000F0A72) = 4u;   /* 0x1151F */
            DSW(DS_000F0A64) = 6u;                  /* 0x11526 */
        }
        break;

    case 0xC: {
        u16 v = DSW(DS_000F0A68);                   /* 0x11531 */
        DSW(DS_000F0A68) = (u16)(v - 1u);           /* 0x1153A */
        if ((s16)v <= 0)                            /* 0x11544 (jg on the original) */
            DSB(DS_000F0A6F) = DSB(DS_000F0A70);    /* 0x1154B */
        break;
    }

    default:
        break;      /* > 0xC: the tail only (0x1100C `ja 0x11550`) */
    }

    /* 0x11550 tail: the raw sets up no arguments for 0x10F28, which reads only
     * its two countdown globals. */
    if (DSB(DS_0009AD58) == 0u)
        attract_voice_tick();                       /* 0x11559 (0x10F28) */
}
