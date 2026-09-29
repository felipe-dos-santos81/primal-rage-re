/* port/src/game/svcmenu.c */
#include "game/svcmenu.h"

#include "game/actors.h"
#include "game/config.h"
#include "game/flow.h"
#include "game/menu.h"
#include "platform/input.h"

#include "../mem.h"
#include "../symbols.h"

#include <stdio.h>
#include <string.h>

#define SVC_TABLE_START   0x000BCCCCu   /* START MENU (record §0.2) */
#define SVC_TABLE_OPTIONS 0x000BCC1Cu   /* OPTIONS MENU */
#define SVC_BACKDROP      0x0009AD84u   /* the descriptor 0x2F99C spawns */
#define SVC_BLANK_PTR     0x0002CC70u   /* code dword: 0x80A18, eight spaces (record §K11.3) */
#define SVC_CFG_FALLBACK  0x00032700u   /* 0x335A5: the table when EAX is 0 */
#define SVC_TABLE_SAMPLES 0x000A3500u   /* SOUND TEST option table, "Samples" */
#define SVC_TABLE_TUNES   0x000A3060u   /* MUSIC TEST option table, "Music Tunes" */
#define SVC_TUNE_VOICES   0x000BC938u   /* 26 voice ids read by 0x2C9D9 */
#define SVC_SAMPLE_VOICES 0x000BC9A0u   /* 143 voice ids read by 0x2C9F5 */
#define SVC_CFG_FIELD     0x29u         /* the config field CONFIG OPTIONS edits */

typedef void (*svc_text_fn)(s32 col, s32 row, const u8 *s, u32 mode);

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

/* 0x2CC74 — record §K11.3. EAX = table, EDX = bits, EBX = first record,
 * ECX = mode, [esp+4] = release (`ret 4`). */
s32 svc_option_rows(u32 table, u32 bits, s32 first, u32 mode, u32 release)
{
    if (first < 0) return 0;                                /* 0x2CC7B..0x2CC81 */
    u32 opt = table + (u32)first * 0x14u;                   /* 0x2CC83..0x2CC8D */
    if (DSD(opt) == 0u) return 0;                           /* 0x2CC8F..0x2CC99 */
    for (u32 row = 3u; (s32)(u16)row <= 0x16; row += 3u) {  /* 0x2CC9C, 0x2CCBA..0x2CCC5 */
        opt = svc_option_row(opt, bits, (s32)(u16)row, mode, release);   /* 0x2CCA3..0x2CCB1 0x2CD30 */
        if (opt == 0u) return 0;                            /* 0x2CCB6..0x2CCB8 */
    }
    return (s32)opt;                                        /* 0x2CCC7 */
}

/* 0x2CD30 — record §K11.3. EAX = the option record, EDX = bits, EBX = row
 * ([esp+8]), ECX = mode (EBP), [esp+4] = release (`ret 4`). */
u32 svc_option_row(u32 opt, u32 bits, s32 row, u32 mode, u32 release)
{
    const svc_text_fn text = release != 0u ? text_cells_release : text_cursor_set;   /* 0x2CD7E, 0x2CE1F, 0x2CE75, 0x2CEC3 */
    s32 span = 2;                                           /* 0x2CD3E */
    u32 nbits = 1u;                                         /* 0x2CD46 */
    if ((s32)DSD(opt + 8u) > span) {                        /* 0x2CD43..0x2CD4D */
        do {
            span += span;                                   /* 0x2CD52 */
            nbits++;                                        /* 0x2CD54 */
        } while (span < (s32)DSD(opt + 8u));                /* 0x2CD4F, 0x2CD55..0x2CD57 */
    }
    u32 v = (bits >> (DSB(opt + 4u) & 31u)) & ((1u << (nbits & 31u)) - 1u);   /* 0x2CD59..0x2CD68 */
    if (DSD(opt) == 0u) return 0u;                          /* 0x2CD6A..0x2CD6D */
    if (v + 1u > DSD(opt + 8u)) return 0u;                  /* 0x2CD6F..0x2CD77 */
    text(4, row, game_string_get(DSD(opt)), mode);          /* 0x2CD85..0x2CDB4 0x1C500, 0x2F280/0x2F198 */
    const u32 entry = DSD(opt + 0x10u) + v * 8u;            /* 0x2CDB9..0x2CDD8 */
    u32 str = DSD(entry);                                   /* 0x2CDD6..0x2CDDC */
    s32 col = 5;                                            /* 0x2CDCB..0x2CDD2 */
    if (str != 0u && (s8)DSB(str) == '*') {                 /* 0x2CDE0..0x2CDEA */
        mode = (mode & 0x8000u) | 0x1000u;                  /* 0x2CDF0..0x2CDF7 */
        str++;                                              /* 0x2CDF6..0x2CDFD */
    }
    if (DSB(opt + 0xCu) != 0u) {                            /* 0x2CE01..0x2CE05 */
        char num[12];
        /* PORT: 0x2CE11 0x655E4 is the WATCOM itoa(value + 1, buf, 10)
         * (runtime, not a target); formatted in C. */
        snprintf(num, sizeof num, "%d", (int)(v + 1u));     /* 0x2CE07..0x2CE11 */
        text(col, row + 1, (const u8 *)num, mode);          /* 0x2CE16..0x2CE3A */
        col += (s32)strlen(num) + 1;                        /* 0x2CE3F..0x2CE50 */
    }
    if (str != 0u && strlen((const char *)(mem + str)) != 0u) {   /* 0x2CE54..0x2CE6A */
        text(col, row + 1, mem + str, mode);                /* 0x2CE6C..0x2CE94 */
        col += (s32)strlen((const char *)(mem + str)) + 1;  /* 0x2CE99..0x2CEAC */
    }
    if (DSD(entry + 4u) != 0u)                              /* 0x2CEB0..0x2CEB8 */
        text(col, row + 1, game_string_get(DSD(entry + 4u)), mode);   /* 0x2CEBA..0x2CEEE */
    return opt + 0x14u;                                     /* 0x2CEF3 */
}

/* 0x2CF00 — record §K11.3. EAX = table (EBP), EDX = bits, EBX = the restore
 * key mask ([esp+0x28]), CL = Esc cancels ([esp+0x40]). Locals: [esp] the
 * "+ MORE +" copy, [esp+0x1C] the entry bits, [esp+0x20] the current record,
 * [esp+0x24] the window 6, [esp+0x2C] the lower marker row, [esp+0x30] the
 * result bits, [esp+0x34] the working bits, [esp+0x38] the scroll top,
 * [esp+0x3C] the last index; ESI the current index. Blocking. */
u32 svc_option_edit(u32 table, u32 bits, u32 mask, u8 esc_cancels)
{
    u8 more[0x1C];
    const u8 *blank = mem + DSD(SVC_BLANK_PTR);             /* 0x2D12F/0x2D17A [0x2CC70] = 0x80A18 */
    u32 result = bits, work = bits;                         /* 0x2CF08, 0x2CF18, 0x2CF21 */
    strcpy((char *)more, (const char *)game_string_get(0x72u));   /* 0x2CF1C..0x2CF4A 0x1C500 */
    const s32 window = 6;                                   /* 0x2CF2C, 0x2CF4B */
    u32 n = 0u;
    for (u32 r = table; DSD(r) != 0u; r += 0x14u) n++;      /* 0x2CF4F..0x2CF61 */
    u32 cur = table;                                        /* 0x2CF6E */
    (void)svc_option_rows(table, work, 0, 0xF000u, 0u);     /* 0x2CF63..0x2CF77 0x2CC74 */
    const u32 last = n - 1u;                                /* 0x2CF76, 0x2CF8E */
    (void)svc_option_row(table, work, 3, 0x2000u, 0u);      /* 0x2CF7C..0x2CF92 0x2CD30 */
    u32 idx = 0u, top = 0u;                                 /* 0x2CF9D..0x2CFA4 */
    if ((s32)(u16)last > window) {                          /* 0x2CF9F..0x2CFAA */
        more[0] = 0x3Bu;                                    /* 0x2CFAC..0x2CFB2 */
        more[strlen((const char *)more) - 1u] = 0x3Bu;      /* 0x2CFB5..0x2CFBF */
        text_cursor_set(9, window * 3 + 5, more, 0x1000u);  /* 0x2CFC3..0x2CFDF 0x2F198 */
    }
    input_repeat_set(0xF000F000u, 0x1Eu, 0xFu);             /* 0x2CFE4..0x2CFF3 0x50146 */
    const s32 lower = window * 3 + 5;                       /* 0x2CFF8..0x2D008 */
    u32 keys;
    for (;;) {
        config_screen_wait_zero();                          /* 0x2D00C 0x2EA74 */
        keys = config_input_poll(0xF300F000u, 1u);          /* 0x2D011..0x2D020 0x2EDE0 */
        if ((keys & 0x2000000u) != 0u) break;               /* 0x2D022..0x2D027 */
        if ((keys & 0x1000000u) != 0u) break;               /* 0x2D02D..0x2D032 */
        u32 redraw = 0u;                                    /* 0x2D03C */
        if ((keys & mask) != 0u) {                          /* 0x2D038..0x2D040 */
            work = bits;                                    /* 0x2D042, 0x2D058 */
            idx = 0u;                                       /* 0x2D05E */
            (void)svc_option_rows(table, result, (s32)(u16)top, 0xF000u, 1u);   /* 0x2D046..0x2D060 0x2CC74 */
            redraw = 1u;                                    /* 0x2D065 */
        }
        result = work;                                      /* 0x2D06A..0x2D06E */
        if ((keys & 0x40004000u) != 0u && (u16)idx < (u16)last)   /* 0x2D072..0x2D07F */
            idx++;                                          /* 0x2D081 */
        else if ((keys & 0x80008000u) != 0u && (u16)idx != 0u)    /* 0x2D084..0x2D08F */
            idx--;                                          /* 0x2D091 */
        if ((keys & 0xC000C000u) != 0u || redraw != 0u) {   /* 0x2D092..0x2D09D */
            const u32 old_top = (u16)top;                   /* 0x2D0AA..0x2D0B0 */
            (void)svc_option_rows(table, result, (s32)old_top, 0xF000u, 1u);   /* 0x2D0A3..0x2D0B9 0x2CC74 */
            if ((u16)idx < (u16)old_top)                    /* 0x2D0BE..0x2D0C1 */
                top = idx;                                  /* 0x2D0C3 */
            else if ((s32)(old_top + (u32)window) < (s32)(u16)idx)   /* 0x2D0C9..0x2D0D4 */
                top = (u16)idx - (u32)window;               /* 0x2D0D6..0x2D0DC */
            (void)svc_option_rows(table, work, (s32)(u16)top, 0xF000u, 0u);    /* 0x2D0E0..0x2D0F4 0x2CC74 */
            cur = table + (u16)idx * 0x14u;                 /* 0x2D0F9..0x2D106 */
            more[0] = 0x19u;                                /* 0x2D10A..0x2D110 */
            more[strlen((const char *)more) - 1u] = 0x19u;  /* 0x2D113..0x2D11D */
            text_cursor_set(9, 2, (u16)top != 0u ? more : blank, 0x1000u);     /* 0x2D121..0x2D13F 0x2F198 */
            more[0] = 0x3Bu;                                /* 0x2D144..0x2D148 */
            more[strlen((const char *)more) - 1u] = 0x3Bu;  /* 0x2D14B..0x2D157 */
            text_cursor_set(9, lower,
                            (s32)((u32)(u16)last - (u16)top) > window ? more : blank,
                            0x1000u);                       /* 0x2D15B..0x2D189 0x2F198 */
            (void)svc_option_row(table + (u16)idx * 0x14u, work,
                                 (s32)(u16)((idx - top + 1u) * 3u), 0x2000u, 0u);   /* 0x2D18E..0x2D1B4 0x2CD30 */
            continue;                                       /* 0x2D1B9 */
        }
        if ((config_input_poll(0u, 1u) & 0xC000C000u) != 0u) continue;   /* 0x2D1BE..0x2D1CF 0x2EDE0 */
        if ((keys & 0x30003000u) == 0u) continue;           /* 0x2D1D5..0x2D1DB */
        u32 w = 1u;                                         /* 0x2D1E1 */
        for (u32 span = 2u; (s32)(u16)span < (s32)DSD(cur + 8u); span += span)   /* 0x2D1E6, 0x2D1F4..0x2D1FC */
            w++;                                            /* 0x2D1F1 */
        const u32 sh = DSB(cur + 4u) & 31u;                 /* 0x2D208 */
        const u32 field = ((1u << (w & 31u)) - 1u) << sh;   /* 0x2D1FE..0x2D20B */
        if ((keys & 0x20002000u) != 0u) {                   /* 0x2D20D Left */
            if ((field & work) == 0u)                       /* 0x2D215..0x2D21B */
                work |= (DSD(cur + 8u) - 1u) << sh;         /* 0x2D21D..0x2D227: 0 wraps to count - 1 */
            else
                work -= 1u << sh;                           /* 0x2D229..0x2D234 */
        } else if ((keys & 0x10001000u) != 0u) {            /* 0x2D23A Right */
            if (((work & field) >> sh) == DSD(cur + 8u) - 1u)   /* 0x2D242..0x2D252 */
                work &= ~field;                             /* 0x2D254..0x2D25C: count - 1 wraps to 0 */
            else
                work += 1u << sh;                           /* 0x2D262..0x2D270 */
        }
        const s32 row = (s32)(u16)((idx - top + 1u) * 3u);  /* 0x2D276..0x2D28F */
        (void)svc_option_row(cur, result, row, 0xF000u, 1u);   /* 0x2D282..0x2D298 0x2CD30 */
        (void)svc_option_row(cur, work, row, 0x2000u, 0u);     /* 0x2D29D..0x2D2AE 0x2CD30 */
        result = work;                                      /* 0x2D2B3..0x2D2B7 */
    }
    if ((keys & 0x2000000u) != 0u && esc_cancels != 0u)    /* 0x2D2C0..0x2D2CD */
        return 0xFFFFFFFFu;                                 /* 0x2D2CF */
    return result;                                          /* 0x2D2D6..0x2D2DA */
}

/* 0x2CACC — record §K11.3. OPTIONS MENU "CONFIG OPTIONS". */
u32 svc_config_options_entry(u32 entry)
{
    (void)entry;                                            /* EAX is reloaded at 0x2CACC */
    return svc_config_options(DSD(DSD(DS_0010740C) + 4u));  /* [0x10740C], then +4 (0x2CAD1); 0x2CAD4 jmp 0x33578 */
}

/* 0x33578 — record §K11.3. EAX = the option table (ESI). */
u32 svc_config_options(u32 table)
{
    svc_screen_reset();                                     /* 0x3357F 0x2F99C */
    text_cursor_set(-1, 0, game_string_get(0x76u), 0x5002u);   /* 0x33584..0x3359C 0x1C500, 0x2F198 */
    /* PORT: 0x335A1..0x335AC: a zero table becomes the code-object 0x32700;
     * the `je` at 0x335AC after it is never taken. No raw caller passes 0. */
    if (table == 0u) table = SVC_CFG_FALLBACK;
    u32 old = config_field_get(SVC_CFG_FIELD);              /* 0x335B2..0x335BC 0x2D974 */
    if ((old & 0x8000u) != 0u) {                            /* 0x335BE..0x335C1 */
        old = config_menu_default_bits(table);              /* 0x335C3..0x335CA 0x2CCD0 */
        config_field_set(SVC_CFG_FIELD, old);               /* 0x335CC..0x335D3 0x2DA0C */
    }
    text_cursor_set(-1, 0x19, game_string_get(0x6Eu), 0x1000u);    /* 0x335D8..0x335F3 */
    text_cursor_set(-1, 0x1A, game_string_get(0x71u), 0x1000u);    /* 0x335F8..0x33613 */
    text_cursor_set(-1, 0x1B, game_string_get(0x209u), 0x1000u);   /* 0x33618..0x33633 */
    text_cursor_set(-1, 0x1C, game_string_get(0x70u), 0x1000u);    /* 0x33638..0x33653 */
    const u32 r = svc_option_edit(table, old, 0x4000000u, 0u);     /* 0x33658..0x33663 0x2CF00 */
    config_field_set(SVC_CFG_FIELD, r);                     /* 0x33668..0x3366F 0x2DA0C */
    u32 now = config_field_get(SVC_CFG_FIELD);              /* 0x33674..0x3367E 0x2D974 */
    if ((now & 0x8000u) != 0u) {                            /* 0x33680..0x33683 */
        now = config_menu_default_bits(table);              /* 0x33685..0x3368C 0x2CCD0 */
        config_field_set(SVC_CFG_FIELD, now);               /* 0x3368E..0x33695 0x2DA0C */
    }
    if ((old & 0x7000000u) != (now & 0x7000000u)) {         /* 0x3369A..0x336A9 */
        /* PORT: 0x336AE 0x47370(new >> 24) reloads the language file; the
         * port's game_string_table_load is idempotent and English-only (host
         * file I/O), so a changed language is stored in field 0x29 but not
         * shown (named gap, record §K11.3). EAX would be 0x47370's result;
         * the only caller, 0x2CACC through menu_run (0x2FD98), discards it. */
    }
    return now & 0x7000000u;                                /* 0x336A2 */
}

/* 0x30EB4 — record §K11.3. OPTIONS MENU "SOUND TEST". */
u32 svc_sound_test(u32 entry)
{
    (void)entry;
    svc_screen_reset();                                     /* 0x30EB8 0x2F99C */
    text_cursor_set(-1, 0, game_string_get(0x20Fu), 0x5002u);      /* 0x30EBD..0x30ED5 */
    text_cursor_set(-1, 0x1B, game_string_get(0x209u), 0x1000u);   /* 0x30EDA..0x30EF5 */
    text_cursor_set(-1, 0x1C, game_string_get(0x20Au), 0x1000u);   /* 0x30EFA..0x30F17 */
    u32 prev = 0u;                                          /* 0x30F15 */
    for (;;) {
        const u32 r = svc_option_edit(SVC_TABLE_SAMPLES, prev, 0x4000000u, 1u);   /* 0x30F1C..0x30F2D 0x2CF00 */
        if (r == 0xFFFFFFFFu) break;                        /* 0x30F32..0x30F37 */
        (void)svc_play_sample(r);                           /* 0x30F39 0x2C9E8 */
        prev = r;                                           /* 0x30F3E */
    }
    return sound_voice(0x100u);                             /* 0x30F42..0x30F47 0x2C3FC */
}

/* 0x30F54 — record §K11.3. OPTIONS MENU "MUSIC TEST". */
u32 svc_music_test(u32 entry)
{
    (void)entry;
    svc_screen_reset();                                     /* 0x30F58 0x2F99C */
    text_cursor_set(-1, 0, game_string_get(0x214u), 0x5002u);      /* 0x30F5D..0x30F75 */
    text_cursor_set(-1, 0x1B, game_string_get(0x209u), 0x1000u);   /* 0x30F7A..0x30F95 */
    text_cursor_set(-1, 0x1C, game_string_get(0x20Au), 0x1000u);   /* 0x30F9A..0x30FB7 */
    u32 prev = 0u;                                          /* 0x30FB5 */
    for (;;) {
        const u32 r = svc_option_edit(SVC_TABLE_TUNES, prev, 0x4000000u, 1u);     /* 0x30FBC..0x30FCD 0x2CF00 */
        if (r == 0xFFFFFFFFu) break;                        /* 0x30FD2..0x30FD7 */
        (void)svc_play_tune(r);                             /* 0x30FDD 0x2C9CC */
        prev = r;                                           /* 0x30FE2 */
    }
    /* 0x30FD7 je 0x30F42: 0x30EB4's tail, repeated */
    return sound_voice(0x100u);                             /* 0x30F42..0x30F47 0x2C3FC */
}

/* 0x2C9CC — record §K11.3. EAX = the tune index. */
u32 svc_play_tune(u32 i)
{
    (void)sound_voice(0x100u);                              /* 0x2C9CF..0x2C9D4 0x2C3FC */
    return sound_voice(DSD(SVC_TUNE_VOICES + i * 4u));      /* 0x2C9D9..0x2C9E0 0x2C3FC */
}

/* 0x2C9E8 — record §K11.3. EAX = the sample index. */
u32 svc_play_sample(u32 i)
{
    (void)sound_voice(0x100u);                              /* 0x2C9EB..0x2C9F0 0x2C3FC */
    return sound_voice(DSD(SVC_SAMPLE_VOICES + i * 4u));    /* 0x2C9F5..0x2C9FC 0x2C3FC */
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
    fn_register(0x2CACCu, (void (*)(void))svc_config_options_entry);
    fn_register(0x30EB4u, (void (*)(void))svc_sound_test);
    fn_register(0x30F54u, (void (*)(void))svc_music_test);
}
