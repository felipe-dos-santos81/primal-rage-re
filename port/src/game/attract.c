/* port/src/game/attract.c
 * The small attract/boot units (0x292AC, 0x10EE4, 0x10F28, 0x10DB0, 0x10E18
 * and 0x2C8F0 with eax = -2). The raw-byte derivation of every body is in
 * docs/superpowers/plans/2026-09-19-attract-derivations.md. */
#include "game/attract.h"
#include "game/actors.h"
#include "game/config.h"
#include "game/rng.h"
#include "mem.h"
#include "symbols.h"

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
