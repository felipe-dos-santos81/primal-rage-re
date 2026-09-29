/* port/src/game/svcmenu.c */
#include "game/svcmenu.h"

#include "game/actors.h"
#include "game/config.h"
#include "game/flow.h"
#include "game/menu.h"

#include "../mem.h"
#include "../symbols.h"

#define SVC_TABLE_START   0x000BCCCCu   /* START MENU (record §0.2) */
#define SVC_TABLE_OPTIONS 0x000BCC1Cu   /* OPTIONS MENU */
#define SVC_BACKDROP      0x0009AD84u   /* the descriptor 0x2F99C spawns */

/* 0x2CB74 — record §K11.2. */
u32 svc_start_menu(u32 entry)
{
    (void)entry;                                           /* EAX is reloaded at 0x2CB81 */
    return menu_step(SVC_TABLE_START, 0x10u, 0u);          /* 0x2CB77..0x2CB88 0x2FFC4 (EBX = 0xF000, unread) */
}

/* 0x2CB94 — record §K11.2. */
u32 svc_options_menu(u32 entry)
{
    (void)entry;
    u32 r = menu_run(SVC_TABLE_OPTIONS, 0x10u, 0u);        /* 0x2CB97..0x2CBA8 0x2FA40, 0x2CBAD mov edx,eax */
    /* PORT: 0x2CBAF 0x1B084, the CMOS config writer, is deferred
     * (classification row 1B084, record §50-C); the settings stay in mem[]. */
    config_credits_init();                                 /* 0x2CBB4 0x2C304 */
    return r;                                              /* 0x2CBB9 mov eax,edx */
}

/* 0x2CBC4 — record §K11.2. LEFT PLAYER ARCADE. */
u32 svc_start_arcade_left(u32 entry)
{
    DSW(DS_00104B00) = 0x2Du;                              /* 0x2CBC5 edx, 0x2CBCC word store */
    DSB(DS_00104B1D) = 0u;                                 /* 0x2CBCA xor ah,ah; 0x2CBD3 */
    return entry & 0xFFFF00FFu;                            /* EAX: the entry with AH = 0 */
}

/* 0x2CBDC — record §K11.2. RIGHT PLAYER ARCADE. */
u32 svc_start_arcade_right(u32 entry)
{
    DSW(DS_00104B00) = 0x2Eu;                              /* 0x2CBDD, 0x2CBE4 */
    DSB(DS_00104B1D) = 0u;                                 /* 0x2CBE2 xor ah,ah; 0x2CBEB */
    return entry & 0xFFFF00FFu;
}

/* 0x2CBF4 — record §K11.2. LEFT PLAYER TRAINING. */
u32 svc_start_training_left(u32 entry)
{
    DSW(DS_00104B00) = 0x28u;                              /* 0x2CBF5, 0x2CBFC */
    DSB(DS_00104B1D) = 1u;                                 /* 0x2CBFA mov ah,1; 0x2CC03 */
    return (entry & 0xFFFF00FFu) | 0x0100u;
}

/* 0x2CC0C — record §K11.2. RIGHT PLAYER TRAINING. */
u32 svc_start_training_right(u32 entry)
{
    DSW(DS_00104B00) = 0x29u;                              /* 0x2CC0D, 0x2CC14 */
    DSB(DS_00104B1D) = 1u;                                 /* 0x2CC12, 0x2CC1B */
    return (entry & 0xFFFF00FFu) | 0x0100u;
}

/* 0x2CC24 — record §K11.2. TUG OF WAR. */
u32 svc_start_tug_of_war(u32 entry)
{
    DSW(DS_00104B00) = 0x2Au;                              /* 0x2CC25, 0x2CC2C */
    DSB(DS_00104B1D) = 2u;                                 /* 0x2CC2A, 0x2CC33 */
    return (entry & 0xFFFF00FFu) | 0x0200u;
}

/* 0x2CC3C — record §K11.2. ENDURANCE. */
u32 svc_start_endurance(u32 entry)
{
    DSW(DS_00104B00) = 0x2Bu;                              /* 0x2CC3D, 0x2CC44 */
    DSB(DS_00104B1D) = 3u;                                 /* 0x2CC42, 0x2CC4B */
    return (entry & 0xFFFF00FFu) | 0x0300u;
}

/* 0x2CC54 — record §K11.2. Start 2 PLAYER HANDICAP. */
u32 svc_start_handicap(u32 entry)
{
    DSW(DS_00104B00) = 0x2Cu;                              /* 0x2CC55, 0x2CC5C */
    DSB(DS_00104B1D) = 4u;                                 /* 0x2CC5A, 0x2CC63 */
    return (entry & 0xFFFF00FFu) | 0x0400u;
}

/* 0x2F99C — record §K11.2. The screen reset of every options screen. */
void svc_screen_reset(void)
{
    frontend_input_reset();                                /* 0x2F99F..0x2F9A1 0x4F1E4 (EAX = 0, unread) */
    actors_reset();                                        /* 0x2F9A6..0x2F9AF 0x2BAF4 (EAX = 1) */
    frontend_origin_zero();                                /* 0x2F9B4..0x2F9B8 0x4F1D0 */
    frontend_spawn_row((const u32 *)(mem + SVC_BACKDROP), 0u, 0u);   /* 0x2F9BD..0x2F9C2 0x38B18 (EDX = EBX = 0) */
}

/* PORT: the menu tables 0xBCBDC/0xBCC1C/0xBCCCC hold these as code
 * addresses (record §0.2); menu_step/menu_run reach them through fn_resolve.
 * One-time: fn_register appends unconditionally. Later cycles add their roots. */
void svcmenu_register(void)
{
    static int done;
    if (done) return;
    done = 1;
    fn_register(0x2CB74u, (void (*)(void))svc_start_menu);
    fn_register(0x2CB94u, (void (*)(void))svc_options_menu);
    fn_register(0x2CBC4u, (void (*)(void))svc_start_arcade_left);
    fn_register(0x2CBDCu, (void (*)(void))svc_start_arcade_right);
    fn_register(0x2CBF4u, (void (*)(void))svc_start_training_left);
    fn_register(0x2CC0Cu, (void (*)(void))svc_start_training_right);
    fn_register(0x2CC24u, (void (*)(void))svc_start_tug_of_war);
    fn_register(0x2CC3Cu, (void (*)(void))svc_start_endurance);
    fn_register(0x2CC54u, (void (*)(void))svc_start_handicap);
}
