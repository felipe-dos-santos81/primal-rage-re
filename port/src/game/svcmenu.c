/* port/src/game/svcmenu.c */
#include "game/svcmenu.h"

#include "game/actors.h"
#include "game/attract.h"
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
#define SVC_VOL_MUTED     0x000BD458u   /* byte: set by 0x30E25, cleared by 0x30E49 */
#define SVC_VOL_LABELS    0x000BD44Cu   /* three string ids 0x77, 0x78, 0x79 (dwords) */
#define SVC_SLASH         0x00080B60u   /* "/" */
#define SVC_BLANK4        0x00080B64u   /* "    " */
#define SVC_HCP_LABELS    0x00030720u   /* code: {0x17, 0x16}, copied by 0x31145..0x3114B */
#define SVC_KEYREC_TMP    0x03900040u   /* PORT: 0x31138's 0x28-byte stack record, see there */
#define SVC_NAME_TMP      0x03900080u   /* PORT: 0x31A78/0x31C78's 8-byte name buffer, see there */
#define SVC_HEX_DIGITS    0x0002EF10u   /* code: "0123456789ABCDEF" */
#define SVC_CTRL_DIAG     0x00031410u   /* code: the 12-byte marker entries, 3 diagnostic ones first */
#define SVC_CTRL_TABLE    0x00031434u   /* 0x31410 + 0x24: the eight button markers, ended by {0, .., 0} */
#define SVC_DIAG_HEAD     0x00080BACu   /* "ADDRESS    RAW DATA" */
#define SVC_DIAG_RULE     0x00080BC0u   /* nineteen '^' */
#define SVC_DIAG_NAME     0x00080BD4u   /* "DIAGS" */
#define SVC_DIAG_ADDR     0xFFE80000u   /* 0x323A8 `mov ebx,0xffe80000` */
#define SVC_KEYS_REC      0x00100CACu   /* the key-config record 0x19DF0 packs and applies */
#define SVC_KEYS_NAME     0x00100CD4u   /* 0x19DF0's key-name buffer */
#define SVC_KEY_LABELS    0x000A2C2Cu   /* eight string ids 0x231 0x234 0x232 0x233 0x21C..0x21F */
#define SVC_BLANK7        0x00080590u   /* "       " (seven spaces) */
/* The layout bytes are signed chars that the raw reads as the dword three
 * bytes below, `sar 0x18` (0x30932 `mov edx,[0xBD441]` gives the byte at
 * 0xBD444). */
#define SVC_HI8(a)        ((s32)DSD(a) >> 24)

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
    /* PORT: the raw's inline copy 0x2CF32..0x2CF48 is unbounded into the 0x1C
     * bytes below [esp+0x1C]; the port keeps that stack layout and the
     * unbounded copy (string 0x72 "+ MORE +" is 8 characters). */
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
    /* 0x335A1..0x335AC, as the raw: a zero table becomes the code-object
     * 0x32700 and the `je` at 0x335AC after it is never taken. No raw caller
     * passes 0. */
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

/* 0x2F464 — record §K11.4. EAX = value, EDX = width, EBX = pad, ECX = mode
 * (kept in ESI): 0x2EFD4 into a 0x14-byte stack buffer, then 0x2F198 at
 * col 0, row -1 (the cursor DS_00105F34, which moves past the number). */
void text_number_cont(s32 value, s32 width, u32 pad, u32 mode)
{
    /* PORT: the original's buffer is uninitialised stack; the port zeroes it,
     * which only a pad above 3 could observe. */
    u8 buf[0x14] = {0};
    (void)text_number_format(value, buf, width, pad);       /* 0x2F468..0x2F470 0x2EFD4 */
    text_cursor_set(0, -1, buf, mode);                      /* 0x2F475..0x2F480 0x2F198 */
}

/* 0x30728 — record §K11.4. String 0x7A centred on row 6 in 0xB000; each pass
 * presents a frame until 0x2EDE0(0, 1) has Esc, then the cells are
 * released. Its only caller is 0x30864's arm at 0x308CC. */
void svc_volume_error(void)
{
    text_cursor_set(-1, 6, game_string_get(0x7Au), 0xB000u);       /* 0x3072B..0x30746 0x2F198 */
    do {
        config_screen_wait_zero();                          /* 0x3074B 0x2EA74 */
    } while ((config_input_poll(0u, 1u) & 0x2000000u) == 0u);      /* 0x30750..0x30761 0x2EDE0 */
    text_cells_release(-1, 6, game_string_get(0x7Au), 0xB000u);    /* 0x30763..0x3077E 0x2F280 */
}

/* 0x30864 — record §K11.4. OPTIONS MENU "ADJUST VOLUME". Locals: [esp] the
 * music level (config field 0x35), [esp+4] the effects level (0x37), [esp+8]
 * the voice level (field 0x2A & 3), [esp+0x1C..0x1E] = 0xFF (the bars' label
 * rows, read back as [esp+0x19..0x1B] `sar 0x18` = -1, so no number is
 * drawn); ESI the selected row, EDI the redraw flag, EBP the voice bar
 * max(music, effects) * voice / 3. */
u32 svc_adjust_volume(u32 entry)
{
    (void)entry;
    const s32 no_label = -1;                                /* 0x30872..0x30881 */
    svc_screen_reset();                                     /* 0x3086D 0x2F99C */
    text_cursor_set(4, 9, game_string_get(0x7Bu), 0xF000u); /* 0x30874..0x3089B "0  1  2 .. 11" */
    s32 v[3];
    v[2] = (s32)(config_field_get(0x2Au) & 3u);             /* 0x308A0..0x308B3 0x2D974 */
    u32 dirty = 1u;                                         /* 0x308BC */
    const u32 music = attract_config_volumes_unscaled();    /* 0x308B7..0x308C6 0x2C8F0(-1) */
    if ((s32)music < 0) {                                   /* 0x308C8..0x308CA `jge` */
        /* Field 0x35 is 16 bits (descriptor 0xE0C0), so 0x2D974 never reads
         * it negative and this arm is not reached (record §K11.4). */
        svc_volume_error();                                 /* 0x308CC 0x30728 */
        return 0xFFFFFFFFu;                                 /* 0x308D1..0x308D6 */
    }
    s32 sfx = (s32)config_field_get(0x37u);                 /* 0x308DB..0x308E0 0x2D974 */
    if (sfx >= 0 && sfx > 0xFF) sfx = 0xFF;                 /* 0x308E7..0x308F2 */
    v[0] = (s32)music;                                      /* 0x308FB, 0x30903 */
    v[1] = sfx;                                             /* 0x308FF, 0x30906 */
    s32 bar = (v[0] > v[1] ? v[0] * v[2] : v[2] * v[1]) / 3;   /* 0x3090E..0x30928 */
    config_bar_draw(v[0], SVC_HI8(0x000BD441u), no_label);  /* 0x3092A..0x3093E 0x30788 */
    config_bar_draw(v[1], SVC_HI8(0x000BD442u), no_label);  /* 0x30943..0x30957 0x30788 */
    config_bar_draw(bar, SVC_HI8(0x000BD443u), no_label);   /* 0x3095C..0x3096E 0x30788 */
    text_cells_release_count(5, SVC_HI8(0x000BD446u), 6);   /* 0x30973..0x30986 0x2F388 */
    if (v[2] == 0 || v[2] == 3) {                           /* 0x3098B..0x30996 */
        text_cursor_set(5, SVC_HI8(0x000BD446u) + 5,
                        game_string_get(bar != 0 ? 0x7Du : 0x7Eu), 0xF000u);   /* 0x30998..0x309C7 "FULL"/"MUTE" */
    } else {
        text_number_set(5, SVC_HI8(0x000BD446u) + 5, v[2], 2, 3u, 0xF000u);   /* 0x309CE..0x309ED 0x2F434 */
        text_cursor_next_line(mem + SVC_SLASH, 0xF000u);    /* 0x309F2..0x30A06 0x2F41C */
        text_number_cont(3, 2, 3u, 0xF000u);                /* 0x30A0B..0x30A12 0x2F464 (EBX = 3) */
    }
    for (u32 i = 0u; i < 3u; i++)                           /* 0x30A17..0x30A7E */
        text_cursor_set(-1, SVC_HI8(0x000BD444u + i),
                        game_string_get(DSD(SVC_VOL_LABELS + i * 4u)), 0xF000u);
    text_cursor_set(-1, 0x18, game_string_get(0x6Fu), 0x1000u);    /* 0x30A83..0x30A9E */
    text_cursor_set(-1, 0x19, game_string_get(0x7Cu), 0x1000u);    /* 0x30AA3..0x30ABE */
    text_cursor_set(-1, 0x1B, game_string_get(0x209u), 0x1000u);   /* 0x30AC3..0x30ADE */
    text_cursor_set(-1, 0x1C, game_string_get(0x70u), 0x1000u);    /* 0x30AE3..0x30AFE */
    text_cursor_set(-1, 0, game_string_get(0x20Cu), 0xF002u);      /* 0x30B03..0x30B1B */
    config_screen_wait_zero();                              /* 0x30B20 0x2EA74 */
    (void)sound_voice(3u);                                  /* 0x30B25..0x30B34 0x2C3FC */
    u32 sel = 0u;                                           /* 0x30B3E */
    input_repeat_set(0xF000F000u, 0x1Eu, 0xFu);             /* 0x30B39..0x30B40 0x50146 */
    for (;;) {
        config_screen_wait_zero();                          /* 0x30B45 0x2EA74 */
        const u32 keys = config_input_poll(0xF300F000u, 1u);   /* 0x30B4A..0x30B54 0x2EDE0 */
        if ((keys & 0x2000000u) != 0u) break;               /* 0x30B5B..0x30B60 */
        if ((keys & 0x40004000u) != 0u) {                   /* 0x30B66 Down */
            sel = (s32)sel < 2 ? sel + 1u : 0u;             /* 0x30B6D..0x30B75 */
            dirty = 1u;                                     /* 0x30B77 */
        }
        if ((keys & 0x80008000u) != 0u) {                   /* 0x30B7C Up */
            sel = (s32)sel > 0 ? sel - 1u : 2u;             /* 0x30B84..0x30B8B */
            dirty = 1u;                                     /* 0x30B90 */
        }
        if ((keys & 0x20002000u) != 0u) {                   /* 0x30B95 Left */
            if (sel == 2u) {                                /* 0x30BA4 */
                v[2]--;                                     /* 0x30BA9..0x30BAD */
                if (v[2] < 0) v[2] = 0;                     /* 0x30BB0..0x30BC8 */
            } else {
                v[sel] = v[sel] >= 8 ? v[sel] - 8 : 0;      /* 0x30BB6..0x30BC8 */
            }
            dirty = 1u;                                     /* 0x30BCB */
        }
        if ((keys & 0x10001000u) != 0u) {                   /* 0x30BD0 Right */
            if (sel == 2u) {                                /* 0x30BDF */
                v[2]++;                                     /* 0x30BE4..0x30BE8 */
                if (v[2] > 3) v[2] = 3;                     /* 0x30BEB..0x30BF0 */
            } else {
                v[sel] = v[sel] <= 0xF7 ? v[sel] + 8 : 0xFF;   /* 0x30BF9..0x30C0C */
            }
            dirty = 1u;                                     /* 0x30C13 */
        }
        bar = (v[0] > v[1] ? v[0] * v[2] : v[2] * v[1]) / 3;   /* 0x30C18..0x30C48 */
        if (dirty == 0u) continue;                          /* 0x30C4A..0x30C4C */
        for (u32 i = 0u; i < 3u; i++)                       /* 0x30C52..0x30CB2 */
            text_cursor_set(-1, SVC_HI8(0x000BD444u + i),
                            game_string_get(DSD(SVC_VOL_LABELS + i * 4u)),
                            i == sel ? 0x3000u : 0x4000u);  /* 0x30C66 / 0x30C84 */
        if (sel == 2u) {                                    /* 0x30CB4 */
            text_cells_release_count(5, SVC_HI8(0x000BD446u) + 5, 0x14);   /* 0x30CBD..0x30CD3 0x2F388 */
            if (v[2] == 0 || v[2] == 3) {                   /* 0x30CD8..0x30CE3 */
                text_cursor_set(5, SVC_HI8(0x000BD444u + sel) + 5,
                                game_string_get(bar != 0 ? 0x7Du : 0x7Eu), 0xF000u);   /* 0x30CE5..0x30D12 */
            } else {
                text_number_set(5, SVC_HI8(0x000BD444u + sel) + 5, v[2], (s32)sel,
                                3u, 0xF000u);               /* 0x30D19..0x30D37 0x2F434 (ECX = ESI) */
                text_cursor_next_line(mem + SVC_SLASH, 0xF000u);   /* 0x30D3C..0x30D50 0x2F41C */
                text_number_cont(3, (s32)sel, 3u, 0xF000u); /* 0x30D55..0x30D59 0x2F464 (EDX = ESI) */
            }
        }
        config_bar_draw(v[0], SVC_HI8(0x000BD441u), no_label);    /* 0x30D5E..0x30D71 0x30788 */
        config_bar_draw(v[1], SVC_HI8(0x000BD442u), no_label);    /* 0x30D76..0x30D8A 0x30788 */
        config_bar_draw(bar, SVC_HI8(0x000BD443u), no_label);     /* 0x30D8F..0x30DA3 0x30788 */
        dirty = 0u;                                         /* 0x30DA1 */
        if (sel == 0u) {                                    /* 0x30DA8 */
            sound_music_volume((u32)(v[0] >> 1));           /* 0x30DAC..0x30DB1 0x1CAB8 */
        } else if (sel == 1u) {                             /* 0x30DB8 */
            sound_sfx_volume((u32)(v[1] >> 1));             /* 0x30DBD, 0x30E00..0x30E02 0x1CED4 */
        } else if (sel == 2u) {                             /* 0x30DC3 */
            const s32 m = v[0] * v[2] / 3;                  /* 0x30DC8..0x30DDC */
            const s32 s = v[1] * v[2] / 3;                  /* 0x30DDE..0x30DF1 */
            sound_music_volume((u32)(m >> 1));              /* 0x30DF5..0x30DF9 0x1CAB8 */
            sound_sfx_volume((u32)(s >> 1));                /* 0x30DFE..0x30E02 0x1CED4 */
        }
        if (v[0] == 0 || (sel == 2u && v[2] == 0)) {        /* 0x30E07..0x30E17 */
            (void)sound_voice(0x22u);                       /* 0x30E19..0x30E20 0x2C3FC */
            DSB(SVC_VOL_MUTED) = 1u;                        /* 0x30E25 (CL = 1) */
        } else if (DSB(SVC_VOL_MUTED) != 0u) {              /* 0x30E30..0x30E37 */
            (void)sound_voice(3u);                          /* 0x30E3D..0x30E44 0x2C3FC */
            DSB(SVC_VOL_MUTED) = 0u;                        /* 0x30E49 (BH = 0) */
        }
    }
    (void)sound_voice(0x100u);                              /* 0x30E54..0x30E59 0x2C3FC */
    config_screen_wait_zero();                              /* 0x30E5E 0x2EA74 */
    (void)config_voice_gate(-1);                            /* 0x30E63..0x30E68 0x2C9B8 */
    config_field_set(0x35u, (u32)v[0]);                     /* 0x30E6D..0x30E7D 0x2DA0C */
    config_field_set(0x37u, (u32)v[1]);                     /* 0x30E82..0x30E89 0x2DA0C */
    const u32 f = config_field_get(0x2Au);                  /* 0x30E8E..0x30E93 0x2D974 */
    config_field_set(0x2Au, (u32)v[2] | (f & 0xFFFFFFFCu)); /* 0x30E98..0x30EA3 0x2DA0C (`and al,0xfc`) */
    return 0u;                                              /* 0x30EA8 */
}

/* 0x30FE8 — record §K11.4. EAX = the value ([esp+0xC], clamped signed to
 * 0x32..0x96), EDX = the row ([esp+8]). The number (width 3, pad 2, mode
 * 0xC002) goes on row + 4 at column 0x12 from 100 up, else at 0x14, after
 * "    " (0x80B64) is released from two columns left on rows + 4 and + 5.
 * Then glyph 0x13 on rows row..row + 2 for i = 0x32, 0x37, .. 0x96 from
 * column 0xB: 0x3000 while i < value, 0xF000 at the value, 0x1000 above. */
void svc_handicap_row(u32 value, s32 row)
{
    s32 v = (s32)value;
    if (v < 0x32) v = 0x32;                                 /* 0x30FF8..0x30FFD `jge` */
    if (v > 0x96) v = 0x96;                                 /* 0x31005..0x3100F `jle` */
    const u8 *blank = mem + SVC_BLANK4;
    if (v >= 0x64) {                                        /* 0x31029 `jl` */
        text_cells_release(0x10, row + 4, blank, 0xC000u);  /* 0x3102E..0x3103F 0x2F280 */
        text_cells_release(0x10, row + 5, blank, 0xC000u);  /* 0x31044..0x31055 0x2F280 */
        text_number_set(0x12, row + 4, v, 3, 2u, 0xC002u);  /* 0x3105A..0x310AC 0x2F434 */
    } else {
        text_cells_release(0x12, row + 4, blank, 0xC000u);  /* 0x3106B..0x3107C 0x2F280 */
        text_cells_release(0x12, row + 5, blank, 0xC000u);  /* 0x31081..0x31092 0x2F280 */
        text_number_set(0x14, row + 4, v, 3, 2u, 0xC002u);  /* 0x31097..0x310AC 0x2F434 */
    }
    s32 col = 0xB;                                          /* 0x310C6 */
    for (s32 i = 0x32; i <= 0x96; i += 5) {                 /* 0x310BE, 0x3111B..0x3112A */
        const u32 mode = i < v ? 0x3000u : (i == v ? 0xF000u : 0x1000u);   /* 0x310CE..0x310E6 */
        text_glyph_at(col, 0x13, row, mode);                /* 0x310EB..0x310F8 0x2F174 */
        text_glyph_at(col, 0x13, row + 1, mode);            /* 0x310FD..0x3110A 0x2F174 */
        text_glyph_at(col, 0x13, row + 2, mode);            /* 0x3110F..0x3111E 0x2F174 */
        col++;                                              /* 0x31123 */
    }
}

/* 0x31138 — record §K11.4. OPTIONS MENU "2 PLAYER HANDICAP". Locals: [esp]
 * the 0x28-byte key-config record, [esp+0x28]/[esp+0x2C] the left and right
 * values (its +0x24/+0x26), [esp+0x30] the two label ids; EDI the selected
 * side, EBP the redraw flag. EAX is 0x2EA78's -1 (record §K5); menu_run
 * discards it (0x2FDA1). */
u32 svc_handicap(u32 entry)
{
    (void)entry;
    const u32 label[2] = { DSD(SVC_HCP_LABELS), DSD(SVC_HCP_LABELS + 4u) };   /* 0x31141..0x3114B */
    svc_screen_reset();                                     /* 0x3114C 0x2F99C */
    /* PORT: the raw packs into its stack record (0x31151 `mov eax,esp`) and
     * 0x1AEE0/0x1AE28 read and write it in mem[]; the record lives at the
     * port scratch SVC_KEYREC_TMP instead. */
    const u32 rec = SVC_KEYREC_TMP;
    config_keys_pack(rec);                                  /* 0x31153 0x1AEE0 */
    s32 v[2];
    v[0] = (s32)DSW(rec + 0x24u);                           /* 0x31158..0x3115F */
    v[1] = (s32)DSW(rec + 0x26u);                           /* 0x31163..0x31173 */
    svc_handicap_row((u32)v[0], SVC_HI8(0x000BD456u));      /* 0x31165..0x3117B 0x30FE8 */
    svc_handicap_row((u32)v[1], SVC_HI8(0x000BD457u));      /* 0x31180..0x3118D 0x30FE8 */
    text_cursor_set(-1, 0, game_string_get(0x1F2u), 0xF002u);      /* 0x31192..0x311AA */
    text_cursor_set(-1, SVC_HI8(0x000BD458u), game_string_get(label[0]), 0xF000u);   /* 0x311AF..0x311CD */
    text_cursor_set(-1, SVC_HI8(0x000BD459u), game_string_get(label[1]), 0xF000u);   /* 0x311D2..0x311F0 */
    text_cursor_set(-1, 0x17, game_string_get(0x6Cu), 0x1000u);    /* 0x311F5..0x31210 */
    text_cursor_set(-1, 0x18, game_string_get(0x71u), 0x1000u);    /* 0x31215..0x31230 */
    text_cursor_set(-1, 0x1A, game_string_get(0x209u), 0x1000u);   /* 0x31235..0x31250 */
    text_cursor_set(-1, 0x1B, game_string_get(0x70u), 0x1000u);    /* 0x31255..0x31270 */
    config_screen_wait_zero();                              /* 0x31275 0x2EA74 */
    u32 sel = 0u;                                           /* 0x31289 */
    input_repeat_set(0xF000F000u, 0x1Eu, 0xFu);             /* 0x3127A..0x3128B 0x50146 */
    /* EBP is the caller's: menu_run's stride 0x10 (0x2FA48 `mov ebp,edx`,
     * unchanged up to its 0x2FD9E call), so the first pass redraws. */
    u32 dirty = 1u;
    for (;;) {
        config_screen_wait_zero();                          /* 0x31290 0x2EA74 */
        const u32 keys = config_input_poll(0xF300F000u, 1u);   /* 0x31295..0x3129F 0x2EDE0 */
        if ((keys & 0x2000000u) != 0u) break;               /* 0x312A6..0x312AB */
        if ((keys & 0x1000000u) != 0u) {                    /* 0x312B1..0x312B6 */
            v[0] = (s32)DSW(rec + 0x24u);                   /* 0x312B8..0x312BF */
            v[1] = (s32)DSW(rec + 0x26u);                   /* 0x312C3..0x312CF */
            dirty = 1u;                                     /* 0x312CA */
        }
        if ((keys & 0x40004000u) != 0u || (keys & 0x80008000u) != 0u) {   /* 0x312D3..0x312E1 */
            dirty = 1u;                                     /* 0x312E3 */
            sel ^= 1u;                                      /* 0x312E8 `xor di,1` */
        }
        if ((keys & 0x20002000u) != 0u) {                   /* 0x312EC Left */
            v[sel] = v[sel] >= 0x37 ? v[sel] - 5 : 0x32;    /* 0x312F4..0x3130D */
            dirty = 1u;                                     /* 0x31315 */
        }
        if ((keys & 0x10001000u) != 0u) {                   /* 0x3131A Right */
            v[sel] = v[sel] <= 0x91 ? v[sel] + 5 : 0x96;    /* 0x31322..0x3133E */
            dirty = 1u;                                     /* 0x31346 */
        }
        if (dirty == 0u) continue;                          /* 0x3134B..0x3134D */
        for (u32 i = 0u; i < 2u; i++)                       /* 0x31353..0x3139D */
            text_cursor_set(-1, SVC_HI8(0x000BD458u + i), game_string_get(label[i]),
                            i == sel ? 0x3000u : 0x4000u);  /* 0x3135F / 0x31377 */
        svc_handicap_row((u32)v[0], SVC_HI8(0x000BD456u));  /* 0x3139F..0x313AC 0x30FE8 */
        dirty = 0u;                                         /* 0x313BE */
        svc_handicap_row((u32)v[1], SVC_HI8(0x000BD457u));  /* 0x313B1..0x313C0 0x30FE8 */
    }
    DSD(DS_00107468) = (u32)v[0];                           /* 0x313CA..0x313CE */
    DSD(DS_0010746C) = (u32)v[1];                           /* 0x313D3..0x313D7 */
    DSW(rec + 0x24u) = (u16)v[0];                           /* 0x313DC..0x313E0 */
    DSW(rec + 0x26u) = (u16)v[1];                           /* 0x313E5..0x313E9 */
    config_keys_apply(rec);                                 /* 0x313EE..0x313F0 0x1AE28 */
    config_screen_wait_zero();                              /* 0x313F5 0x2EA74 */
    return 0xFFFFFFFFu;                                     /* EAX from 0x2EA78 */
}

/* 0x2EF48 — record §K11.5. EAX = value (ESI), EDX = buf, EBX = width
 * ([esp+4]), ECX = the pad flag ([esp]). The index is the 16-bit AX
 * (`movsx`); the NUL goes at buf[(s16)width] (0x2EF56 reads [esp+2], whose
 * high word is the width's low word, then `sar 0x10`). */
s32 text_hex_format(u32 value, u8 *buf, s32 width, u32 space_pad)
{
    buf[(s16)width] = 0u;                                   /* 0x2EF56..0x2EF61 */
    const u8 pad = space_pad != 0u ? 0x20u : 0x30u;         /* 0x2EF65..0x2EF75 (all of ECX) */
    u32 ax = (u32)width;                                    /* 0x2EF5D */
    do {
        ax--;                                               /* 0x2EF78 */
        buf[(s16)ax] = DSB(SVC_HEX_DIGITS + (value & 0xFu));   /* 0x2EF79..0x2EF8D */
        value >>= 4;                                        /* 0x2EF8A */
    } while ((s16)ax > 0 && value != 0u);                   /* 0x2EF8F..0x2EF96 */
    const s32 n = width - (s16)ax;                          /* 0x2EF98..0x2EFA1 */
    while ((s16)ax > 0) {                                   /* 0x2EFA5..0x2EFA8, 0x2EFB4..0x2EFB7 */
        ax--;                                               /* 0x2EFAD */
        buf[(s16)ax] = pad;                                 /* 0x2EFAA..0x2EFB1 */
    }
    return n;                                               /* 0x2EFB9 */
}

/* 0x2F48C — record §K11.5. EAX = col, EDX = row, EBX = value, ECX = width,
 * [esp+4] = pad, [esp+8] = mode (`ret 8`). The 0x14-byte stack buffer is
 * fully written by 0x2EF48 for a width of 1..0x13; the callers pass 8. */
void text_hex_set(s32 col, s32 row, u32 value, s32 width, u32 pad, u32 mode)
{
    u8 buf[0x14];
    (void)text_hex_format(value, buf, width, pad);          /* 0x2F491..0x2F49F 0x2EF48 */
    text_cursor_set(col, row, buf, mode);                   /* 0x2F4A4..0x2F4AE 0x2F198 */
}

/* 0x314A0 — record §K11.5. EAX = col, EDX = row, EBX = the direction bits.
 * EDI holds one bit per cell, row-major from the top left; bit 4 is the
 * centre. Each row releases 5 cells from col - 2 first. */
void svc_stick_draw(s32 col, s32 row, u32 bits)
{
    s32 lit = 0x10;                                         /* 0x314AB */
    if ((bits & 0x20002000u) != 0u)                         /* 0x314B0 Left */
        lit = 8;                                            /* 0x314B8 */
    else if ((bits & 0x10001000u) != 0u)                    /* 0x314BF Right */
        lit = 0x20;                                         /* 0x314C7 */
    if ((bits & 0x80008000u) != 0u)                         /* 0x314CC Up */
        lit >>= 3;                                          /* 0x314D4 `sar` */
    else if ((bits & 0x40004000u) != 0u)                    /* 0x314D9 Down */
        lit <<= 3;                                          /* 0x314E1 */
    for (s32 r = row - 2; r != row + 4; r += 2) {           /* 0x314E8..0x314FD, 0x31556..0x31571 */
        text_cells_release_count(col - 2, r, 5);            /* 0x31500..0x31511 0x2F388 */
        for (s32 c = col - 2; c != col + 4; c += 2) {       /* 0x3151A..0x31524, 0x3154D..0x31554 */
            text_glyph_at(c, (lit & 1) != 0 ? 0x2B : 0x2E, r, 0x4000u);   /* 0x31528..0x31544 0x2F174 */
            lit >>= 1;                                      /* 0x31550 `sar` */
        }
    }
}

/* 0x319B0 — record §K11.5. EBX = col, ECX = row. EAX and EDX are loaded by
 * both callers (the side and the record) but not read: 0x319B6 overwrites
 * EAX and 0x319B9 EDX. Jump table 0x319A0; each case fetches string 0x22C
 * (nine blanks) and releases it in mode 0x4000. */
void svc_buttons_clear(s32 col, s32 row)
{
    for (u32 i = 0u; i < 4u; i++) {                         /* 0x319D5..0x319E0, 0x31A44..0x31A5B */
        switch (i) {
        case 0u:                                            /* [0x319A0] = 0x319E7 */
            text_cells_release(col - 8, row + 8, game_string_get(0x22Cu), 0x4000u);
            break;
        case 1u:                                            /* [0x319A4] = 0x319FF */
            text_cells_release(col + 2, row + 8, game_string_get(0x22Cu), 0x4000u);
            break;
        case 2u:                                            /* [0x319A8] = 0x31A13 */
            text_cells_release(col - 8, row + 0xC, game_string_get(0x22Cu), 0x4000u);
            break;
        default:                                            /* [0x319AC] = 0x31A2A */
            text_cells_release(col + 2, row + 0xC, game_string_get(0x22Cu), 0x4000u);
            break;
        }
    }
}

/* 0x31A78 — record §K11.5. EAX = side, EDX = the key-config record, EBX =
 * col, ECX = row. Jump table 0x31A68. The name buffer is the 8 bytes at
 * [esp], below the locals [esp+8] (col + 5) and [esp+0xC] (col - 5); the
 * English names fit it ("<HOME>" is the longest, 7 bytes). 0x3157C writes
 * nothing for a key with no name, so the buffer keeps the key before's.
 * PORT: the buffer is the scratch SVC_NAME_TMP, zeroed on entry; the raw's
 * first-key buffer is uninitialised stack. */
void svc_buttons_draw(u32 side, u32 rec, s32 col, s32 row)
{
    const u32 name = SVC_NAME_TMP;
    DSD(name) = 0u; DSD(name + 4u) = 0u;
    u32 p = rec + (side == 0u ? 0xAu : 0x1Cu);              /* 0x31A7E..0x31A8A */
    const s32 left = col - 5, right = col + 5;              /* 0x31A8E, 0x31A9E */
    for (u32 i = 0u; i < 4u; i++) {                         /* 0x31AAC..0x31AD6, 0x31B5D..0x31B74 */
        (void)config_key_name(DSW(p), 0u, name);            /* 0x31AB0..0x31AC4 0x3157C (EDX = 0: "<name>") */
        p += 2u;                                            /* 0x31AB8, 0x31AC0 */
        const s32 half = (s32)(strlen((const char *)(mem + name)) >> 1);   /* `repne scasb`, `shr eax,1` */
        switch (i) {
        case 0u:                                            /* [0x31A68] = 0x31ADD */
            text_cursor_set(left - half, row + 8, mem + name, 0x4000u);
            break;
        case 1u:                                            /* [0x31A6C] = 0x31B00 */
            text_cursor_set(right - half, row + 8, mem + name, 0x4000u);
            break;
        case 2u:                                            /* [0x31A70] = 0x31B23 */
            text_cursor_set(left - half, row + 0xC, mem + name, 0x4000u);
            break;
        default:                                            /* [0x31A74] = 0x31B37 */
            text_cursor_set(right - half, row + 0xC, mem + name, 0x4000u);
            break;
        }
    }
}

/* 0x31B94 — record §K11.5. EAX = side; EDX (the record, loaded by the
 * callers) is not read. Centre column c = 0xA or 0x1E (ESI), row 0xB (EDI).
 * Jump table 0x31B84; string 0x22C (nine blanks) each time. */
void svc_dirs_clear(u32 side)
{
    const s32 c = side == 0u ? 0xA : 0x1E;                  /* 0x31B9C..0x31BA7 */
    const s32 r = 0xB;                                      /* 0x31BAC */
    for (u32 i = 0u; i < 4u; i++) {                         /* 0x31BCA..0x31BD7, 0x31C4A..0x31C59 */
        switch (i) {
        case 0u:                                            /* [0x31B84] = 0x31BDE */
            text_cells_release(c - 3, r - 3, game_string_get(0x22Cu), 0x4000u);
            break;
        case 1u:                                            /* [0x31B88] = 0x31BFA */
            text_cells_release(c - 3, r + 3, game_string_get(0x22Cu), 0x4000u);
            break;
        case 2u:                                            /* [0x31B8C] = 0x31C17, 0x2F314 */
            text_cells_release_vertical(c - 3, r - 3, game_string_get(0x22Cu));
            break;
        default:                                            /* [0x31B90] = 0x31C2E, 0x2F314 */
            text_cells_release_vertical(c + 3, r - 3, game_string_get(0x22Cu));
            break;
        }
    }
}

/* 0x31C78 — record §K11.5. EAX = side, EDX = the key-config record. Centre
 * (c, r) = (0xA or 0x1E, 0xB); the words from +2 or +0x14 (ESI). Jump table
 * 0x31C68. Up and down are named wrapped ("<name>", EDX = 0) and centred on
 * c; left and right unwrapped (EDX = 1) and drawn down a column through
 * 0x2F20C, centred on r. The name buffer is [esp] as in 0x31A78, with the
 * locals from [esp+8]. PORT: SVC_NAME_TMP, zeroed on entry (see 0x31A78). */
void svc_dirs_draw(u32 side, u32 rec)
{
    const u32 name = SVC_NAME_TMP;
    DSD(name) = 0u; DSD(name + 4u) = 0u;
    const s32 c = side == 0u ? 0xA : 0x1E;                  /* 0x31C84 / 0x31C9B, [esp+0x1C] */
    const s32 r = 0xB;                                      /* 0x31C89 / 0x31CA0, [esp+0x18] */
    u32 p = rec + (side == 0u ? 2u : 0x14u);                /* 0x31C96 / 0x31CA5 */
    for (u32 i = 0u; i < 4u; i++) {                         /* 0x31CE2..0x31CF2, 0x31DEB..0x31DFA */
        const u8 raw = i >= 2u ? 1u : 0u;                   /* 0x31CFD / 0x31D78 `mov edx,1` */
        (void)config_key_name(DSW(p), raw, name);           /* 0x31CFF..0x31D08 0x3157C */
        p += 2u;                                            /* 0x31CEF `lea eax,[esi+2]`, 0x31D02 */
        const s32 half = (s32)(strlen((const char *)(mem + name)) >> 1);
        switch (i) {
        case 0u:                                            /* [0x31C68] = 0x31CF9 */
            text_cursor_set(c - half, r - 3, mem + name, 0x4000u);
            break;
        case 1u:                                            /* [0x31C6C] = 0x31D38 */
            text_cursor_set(c - half, r + 3, mem + name, 0x4000u);
            break;
        case 2u:                                            /* [0x31C70] = 0x31D74, 0x2F20C */
            text_vertical_set(c - 3, r - half, mem + name, 0x4000u);
            break;
        default:                                            /* [0x31C74] = 0x31DAE, 0x2F20C */
            text_vertical_set(c + 3, r - half, mem + name, 0x4000u);
            break;
        }
    }
}

/* PORT: one helper for two inline sequences, 0x31F54..0x31F85 (in 0x31F24)
 * and 0x323FE..0x3242D (in 0x32358), which are the same instructions: the
 * BIOS record's device words +0x2D4/+0x2D6 replace the packed record's +0 and
 * +0x12 when they differ. No original function exists at this address. */
static void svc_keyrec_devices(u32 rec)
{
    u32 kb = DSD(DS_00101514);
    if (DSW(kb + 0x2D4u) != DSW(rec)) DSW(rec) = DSW(kb + 0x2D4u);
    kb = DSD(DS_00101514);
    if (DSW(kb + 0x2D6u) != DSW(rec + 0x12u)) DSW(rec + 0x12u) = DSW(kb + 0x2D6u);
}

/* 0x31F24 — record §K11.5. OPTIONS MENU "MODIFY CONTROLS". Locals: [esp] the
 * 0x28-byte key-config record, [esp+0x28] the marker table 0x31434,
 * [esp+0x2C] player 2's device, [esp+0x34] the tick before which Up and Down
 * are ignored, byte [esp+0x38] the redraw flag; EBP player 1's device, EDI
 * the selected player, ESI its device. There is no 0x2F99C reset: the screen
 * is drawn over the menu's. */
u32 svc_modify_controls(u32 entry)
{
    (void)entry;
    text_cursor_set(-1, 0x1B, game_string_get(0x6Bu), 0x1000u);   /* 0x31F2D..0x31F48 "PRESS ESCAPE KEY" */
    /* PORT: the raw packs into its stack record (0x31F4D `mov eax,esp`);
     * 0x1AEE0, 0x31E28, 0x31A78, 0x31C78 and 0x1AE28 read and write it in
     * mem[], so it lives at the port scratch SVC_KEYREC_TMP instead. */
    const u32 rec = SVC_KEYREC_TMP;
    config_keys_pack(rec);                                  /* 0x31F4F 0x1AEE0 */
    svc_keyrec_devices(rec);                                /* 0x31F54..0x31F85 */
    u32 sel = 0u;                                           /* 0x31F64 EDI */
    u32 dev2 = DSW(rec + 0x12u);                            /* 0x31F8A..0x31F91 */
    u32 dev1 = DSW(rec);                                    /* 0x31F97 */
    u8 redraw = 1u;                                         /* 0x31F95, 0x31F9B */
    u32 until = DSD(DS_00101500) - 1u;                      /* 0x31F9F 0x500BB, 0x31FA4 */
    const u32 table = SVC_CTRL_TABLE;                       /* 0x31FA9..0x31FB1 */
    for (;;) {
        config_screen_wait(1);                              /* 0x31FB5..0x31FBF 0x2EA78 */
        u32 keys = config_input_poll(0u, 1u);               /* 0x31FC4..0x31FC6 0x2EDE0 (EDX = 1, kept by 0x2EA78) */
        if ((keys & 0x2000000u) != 0u) break;               /* 0x31FCF..0x31FD4 */
        if (DSD(DS_00101500) < until)                       /* 0x31FDE 0x500BB, 0x31FE3 `jae` */
            keys &= 0x3FFF3FFFu;                            /* 0x31FE7: Up/Down ignored */
        u32 dev;
        if (sel == 0u) {                                    /* 0x31FED */
            dev = DSW(rec);                                 /* 0x31FF1..0x31FF3 */
            dev1 = dev;                                     /* 0x31FF7 */
        } else {
            dev = DSW(rec + 0x12u);                         /* 0x31FFB..0x31FFD */
            dev2 = dev;                                     /* 0x32002 */
        }
        if ((keys & 0x20002000u) != 0u) {                   /* 0x32006 Left */
            if (sel != 0u) {                                /* 0x3200E..0x32010 */
                redraw = 1u;                                /* 0x32020 */
                sel = 0u;                                   /* 0x3201A */
                dev = DSW(rec);                             /* 0x3201C */
            }
        } else if ((keys & 0x10001000u) != 0u) {            /* 0x32029 Right */
            if (sel != 1u) {                                /* 0x32031..0x32034 */
                sel = 1u;                                   /* 0x3203A */
                dev = DSW(rec + 0x12u);                     /* 0x32043 */
                redraw = 1u;                                /* 0x32048 */
            }
        } else if ((keys & 0x80008000u) != 0u) {            /* 0x32051 Up */
            until = DSD(DS_00101500) + 0xCu;                /* 0x3205D 0x500BB, 0x32062..0x32067 */
            redraw = 1u;                                    /* 0x3206B */
            dev = dev == 0u ? 6u : dev - 2u;                /* 0x3206F..0x3207A */
            if (sel == 0u) {                                /* 0x3207D */
                if (dev2 == 2u)                             /* 0x32085 */
                    dev = 0u;                               /* 0x3209C */
                else if (dev2 == 4u && dev == 2u)           /* 0x3208A..0x32096 */
                    dev = 0u;                               /* 0x3209C */
            } else if (dev1 == 2u && (dev == dev1 || dev == 4u)) {   /* 0x320A3..0x320AF */
                dev = 0u;                                   /* 0x320C8 */
            } else if ((dev1 == 4u || dev1 == 6u) && dev == 2u) {    /* 0x320B1..0x320C2 */
                dev = 0u;                                   /* 0x320C8 */
            }
        } else if ((keys & 0x40004000u) != 0u) {            /* 0x320CF Down */
            until = DSD(DS_00101500) + 0xCu;                /* 0x320DB 0x500BB, 0x320E0..0x320E5 */
            redraw = 1u;                                    /* 0x320E9 */
            dev = dev == 6u ? 0u : dev + 2u;                /* 0x320ED..0x320F6 */
            if (sel == 0u) {                                /* 0x320F9 */
                if (dev2 == 2u)                             /* 0x32101 */
                    dev = 0u;                               /* 0x32106 */
                else if (dev2 == 4u && dev == 2u)           /* 0x3210A..0x32112 */
                    dev = dev2;                             /* 0x32114 `mov esi,ecx` = 4 */
            } else if (dev1 == 2u && (dev == dev1 || dev == 4u)) {   /* 0x32118..0x32124 */
                dev = 6u;                                   /* 0x32126 */
            } else if ((dev1 == 4u || dev1 == 6u) && dev == 2u) {    /* 0x3212D..0x3213A */
                dev = 4u;                                   /* 0x3213C */
            }
        }
        if (redraw == 0u) continue;                         /* 0x32141..0x32146 */
        redraw = 0u;                                        /* 0x3214C..0x3214E */
        if (sel == 0u) DSW(rec) = (u16)dev;                 /* 0x32152..0x32156 */
        else DSW(rec + 0x12u) = (u16)dev;                   /* 0x3215C */
        config_option_row(0u, rec, sel == 0u ? 1u : 0u);    /* 0x32163..0x3216C / 0x3217C..0x32182 0x31E28 */
        config_option_row(1u, rec, sel == 0u ? 0u : 1u);    /* 0x32171..0x32178 / 0x32187..0x3218E, 0x32190 */
        if (DSW(rec) == 0u) svc_dirs_draw(0u, rec);         /* 0x32195..0x321A0 0x31C78 */
        else svc_dirs_clear(0u);                            /* 0x321A7..0x321AB 0x31B94 */
        if (DSW(rec + 0x12u) == 0u) svc_dirs_draw(1u, rec); /* 0x321B0..0x321BF 0x31C78 */
        else svc_dirs_clear(1u);                            /* 0x321C6..0x321CD 0x31B94 */
        const u16 d1 = DSW(rec);                            /* 0x321D2 */
        if (d1 == 0u || d1 == 6u) svc_buttons_draw(0u, rec, 0xA, 0xB);   /* 0x321D5..0x321F2 0x31A78 */
        else svc_buttons_clear(0xA, 0xB);                   /* 0x321F9..0x32207 0x319B0 */
        const u16 d2 = DSW(rec + 0x12u);                    /* 0x3220C */
        if (d2 == 0u || d2 == 6u) svc_buttons_draw(1u, rec, 0x1E, 0xB);  /* 0x32210..0x32230 0x31A78 */
        else svc_buttons_clear(0x1E, 0xB);                  /* 0x32237..0x32248 0x319B0 */
        svc_stick_draw(0xA, 0xB, 0u);                       /* 0x3224D..0x32259 0x314A0 */
        svc_stick_draw(0x1E, 0xB, 0u);                      /* 0x3225E..0x3226E 0x314A0 */
        for (u32 e = table; DSD(e) != 0u || DSD(e + 8u) != 0u; e += 0xCu) {   /* 0x3226A, 0x3231B..0x3232B */
            const s32 col = (s32)DSB(e + 4u), row = (s32)DSB(e + 5u);
            if (DSD(e + 8u) != 0u)                          /* 0x32278..0x3227D */
                text_cursor_set(col - 3, row - 1, game_string_get(DSD(e + 8u)), 0x4000u);   /* 0x3227F..0x322A3 */
            const u32 f = DSD(e);
            const int gone = ((f == 0x2000000u || f == 0x8000000u) && DSW(rec) == 4u)
                          || ((f == 0x200u || f == 0x800u) && DSW(rec + 0x12u) == 4u);   /* 0x322A8..0x322F4 */
            text_glyph_at(col, gone ? 0x20 : 0x58, row, 0x4000u);   /* 0x322F6..0x32316 0x2F174 */
        }
    }
    (void)config_input_poll(0xF300F000u, 1u);               /* 0x32336..0x32340 0x2EDE0 */
    config_keys_apply(rec);                                 /* 0x32345..0x32347 0x1AE28 */
    return 0u;                                              /* 0x3234C */
}

/* 0x32358 — record §K11.5. OPTIONS MENU "TEST CONTROLS". EBP = DS_00107410
 * & 0x10 (the diagnostic flag, 0x32361..0x32367); [esp] the key-config
 * record, [esp+0x28] the marker table 0x31434. The labels are drawn once;
 * each pass polls the pad (0x2EDE0(0, 0), no latched key), leaves on the
 * latched Esc and draws the sticks and the 'O'/'X' markers. */
u32 svc_test_controls(u32 entry)
{
    (void)entry;
    const u32 diag = DSD(DS_00107410) & 0x10u;              /* 0x32361..0x32367 */
    if (diag != 0u) {                                       /* 0x3236A */
        text_cursor_set(3, 4, mem + SVC_DIAG_HEAD, 0x4000u);   /* 0x3236C..0x32380 0x2F198 */
        text_cursor_set(3, 5, mem + SVC_DIAG_RULE, 0x4000u);   /* 0x32385..0x32399 0x2F198 */
        text_hex_set(3, 6, SVC_DIAG_ADDR, 8, 0u, 0x4000u);  /* 0x3239E..0x323B9 0x2F48C */
        text_cursor_set(3, 7, mem + SVC_DIAG_NAME, 0x4000u);   /* 0x323BE..0x323D2 0x2F198 */
    }
    text_cursor_set(-1, 0x1B, game_string_get(0x6Bu), 0x1000u);   /* 0x323D7..0x323F2 "PRESS ESCAPE KEY" */
    /* PORT: the raw's stack record (0x323F7 `mov eax,esp`) is SVC_KEYREC_TMP,
     * as in 0x31F24. */
    const u32 rec = SVC_KEYREC_TMP;
    config_keys_pack(rec);                                  /* 0x323F9 0x1AEE0 */
    svc_keyrec_devices(rec);                                /* 0x323FE..0x3242D */
    config_option_row(0u, rec, 0u);                         /* 0x32432..0x32438 0x31E28 */
    config_option_row(1u, rec, 0u);                         /* 0x3243D..0x32446 0x31E28 */
    if (DSW(DSD(DS_00101514) + 0x2D4u) == 0u) svc_dirs_draw(0u, rec);   /* 0x3244B..0x3245E 0x31C78 */
    if (DSW(DSD(DS_00101514) + 0x2D6u) == 0u) svc_dirs_draw(1u, rec);   /* 0x32463..0x32479 0x31C78 */
    const u16 d1 = DSW(DSD(DS_00101514) + 0x2D4u);          /* 0x3247E..0x32483 */
    if (d1 == 0u || d1 == 6u) svc_buttons_draw(0u, rec, 0xA, 0xB);      /* 0x3248A..0x324A9 0x31A78 */
    const u16 d2 = DSW(DSD(DS_00101514) + 0x2D6u);          /* 0x324AE..0x324B3 */
    if (d2 == 0u || d2 == 6u) svc_buttons_draw(1u, rec, 0x1E, 0xB);     /* 0x324BA..0x324DC 0x31A78 */
    config_screen_wait_zero();                              /* 0x324E1 0x2EA74 */
    const u32 table = diag != 0u ? SVC_CTRL_DIAG : SVC_CTRL_TABLE;   /* 0x324E6..0x324EF, 0x325D1..0x325DF */
    for (u32 e = table; DSD(e) != 0u || DSD(e + 8u) != 0u; e += 0xCu) {   /* 0x32520..0x32529 */
        /* The three diagnostic entries hold the pointers 0x80B6C/0x80B70/
         * 0x80B74 where a string id belongs; 0x1C500 decodes them as ids
         * (record §K11.5). */
        if (DSD(e + 8u) != 0u)                              /* 0x324F4..0x324F9 */
            text_cursor_set((s32)DSB(e + 4u) - 3, (s32)DSB(e + 5u) - 1,
                            game_string_get(DSD(e + 8u)), 0x4000u);   /* 0x324FB..0x32518 */
    }
    for (;;) {
        const u32 keys = config_input_poll(0u, 0u);         /* 0x32537..0x3253B 0x2EDE0 */
        const u32 k = config_key_latched();                 /* 0x32542 0x2EB80 (EBX kept) */
        if (k != 0u && k == 0x1Bu) break;                   /* 0x32547..0x3254E */
        if (diag != 0u) {                                   /* 0x32554..0x32556 */
            text_hex_set(0xE, 6, keys, 8, 0u, 0x3000u);     /* 0x32558..0x3256E 0x2F48C */
            /* PORT: 0x32573..0x32578 reads the byte at linear 0xFFE80003 (an
             * address outside the port's mem[] and any DOS memory; named gap,
             * record §K11.5); the port draws 0 in its place. */
            text_hex_set(0xE, 7, 0u, 8, 0u, 0x3000u);       /* 0x3257A..0x32596 0x2F48C */
        }
        const u32 now = config_input_poll(0u, 0u);          /* 0x3259B..0x3259F 0x2EDE0 */
        svc_stick_draw(0xA, 0xB, now & 0xF0000000u);        /* 0x325A4..0x325BA 0x314A0 */
        svc_stick_draw(0x1E, 0xB, now & 0xF000u);           /* 0x325BF..0x325D6 0x314A0 */
        for (u32 e = table; DSD(e) != 0u || DSD(e + 8u) != 0u; e += 0xCu)   /* 0x3260E..0x32617 */
            text_glyph_at((s32)DSB(e + 4u), (now & DSD(e)) != 0u ? 0x4F : 0x58,
                          (s32)DSB(e + 5u), 0x3000u);       /* 0x325E5..0x32609 0x2F174 */
        config_screen_wait_zero();                          /* 0x32619 0x2EA74 */
    }
    (void)config_input_poll(0xF300F000u, 1u);               /* 0x32623..0x3262D 0x2EDE0 */
    return 0u;                                              /* 0x32632 */
}

/* 0x19C60 — record §K11.6. EAX = slot, DX = the key word. `cmp eax,0xf; ja
 * 0x19CF0` (unsigned) returns with nothing stored; else jump table 0x19C20,
 * each case one `mov word [addr],dx; ret`. */
void svc_key_slot_set(u32 slot, u16 code)
{
    switch (slot) {                                         /* 0x19C60..0x19C69 */
    case 0u:  DSW(0x00100CAEu) = code; break;               /* [0x19C20] = 0x19C71 */
    case 1u:  DSW(0x00100CB4u) = code; break;               /* [0x19C24] = 0x19C79 */
    case 2u:  DSW(0x00100CB0u) = code; break;               /* [0x19C28] = 0x19C81 */
    case 3u:  DSW(0x00100CB2u) = code; break;               /* [0x19C2C] = 0x19C89 */
    case 4u:  DSW(0x00100CB6u) = code; break;               /* [0x19C30] = 0x19C91 */
    case 5u:  DSW(0x00100CB8u) = code; break;               /* [0x19C34] = 0x19C99 */
    case 6u:  DSW(0x00100CBAu) = code; break;               /* [0x19C38] = 0x19CA1 */
    case 7u:  DSW(0x00100CBCu) = code; break;               /* [0x19C3C] = 0x19CA9 */
    case 8u:  DSW(0x00100CC0u) = code; break;               /* [0x19C40] = 0x19CB1 */
    case 9u:  DSW(0x00100CC6u) = code; break;               /* [0x19C44] = 0x19CB9 */
    case 10u: DSW(0x00100CC2u) = code; break;               /* [0x19C48] = 0x19CC1 */
    case 11u: DSW(0x00100CC4u) = code; break;               /* [0x19C4C] = 0x19CC9 */
    case 12u: DSW(0x00100CC8u) = code; break;               /* [0x19C50] = 0x19CD1 */
    case 13u: DSW(0x00100CCAu) = code; break;               /* [0x19C54] = 0x19CD9 */
    case 14u: DSW(0x00100CCCu) = code; break;               /* [0x19C58] = 0x19CE1 */
    case 15u: DSW(0x00100CCEu) = code; break;               /* [0x19C5C] = 0x19CE9 */
    default: break;                                         /* 0x19C63 ja 0x19CF0 `ret` */
    }
}

/* 0x19D34 — record §K11.6. EAX = slot. `cmp eax,0xf; ja 0x19DD5` (unsigned)
 * returns 0 (`xor eax,eax`); else jump table 0x19CF4, each case `xor
 * eax,eax; mov ax,[addr]; ret` (zero-extended). */
u32 svc_key_slot_get(u32 slot)
{
    switch (slot) {                                         /* 0x19D34..0x19D3D */
    case 0u:  return DSW(0x00100CAEu);                      /* [0x19CF4] = 0x19D45 */
    case 1u:  return DSW(0x00100CB4u);                      /* [0x19CF8] = 0x19D4E */
    case 2u:  return DSW(0x00100CB0u);                      /* [0x19CFC] = 0x19D57 */
    case 3u:  return DSW(0x00100CB2u);                      /* [0x19D00] = 0x19D60 */
    case 4u:  return DSW(0x00100CB6u);                      /* [0x19D04] = 0x19D69 */
    case 5u:  return DSW(0x00100CB8u);                      /* [0x19D08] = 0x19D72 */
    case 6u:  return DSW(0x00100CBAu);                      /* [0x19D0C] = 0x19D7B */
    case 7u:  return DSW(0x00100CBCu);                      /* [0x19D10] = 0x19D84 */
    case 8u:  return DSW(0x00100CC0u);                      /* [0x19D14] = 0x19D8D */
    case 9u:  return DSW(0x00100CC6u);                      /* [0x19D18] = 0x19D96 */
    case 10u: return DSW(0x00100CC2u);                      /* [0x19D1C] = 0x19D9F */
    case 11u: return DSW(0x00100CC4u);                      /* [0x19D20] = 0x19DA8 */
    case 12u: return DSW(0x00100CC8u);                      /* [0x19D24] = 0x19DB1 */
    case 13u: return DSW(0x00100CCAu);                      /* [0x19D28] = 0x19DBA */
    case 14u: return DSW(0x00100CCCu);                      /* [0x19D2C] = 0x19DC3 */
    case 15u: return DSW(0x00100CCEu);                      /* [0x19D30] = 0x19DCC */
    default:  return 0u;                                    /* 0x19D37 ja 0x19DD5 `xor eax,eax` */
    }
}

/* 0x2EBBC — record §K11.6. */
u32 svc_raw_key_take(void)
{
    const u32 k = DSD(DS_00105F28);                         /* 0x2EBBF */
    DSD(DS_00105F28) = 0u;                                  /* 0x2EBBD xor edx,edx; 0x2EBC4 */
    return k;
}

/* 0x19DF0 — record §K11.6. OPTIONS MENU "CONFIGURE KEYBOARD". The record is
 * the global 0x100CAC (not a stack record) and 0x3157C's names go to
 * 0x100CD4. ESI is the slot, EDI its column, [esp] its row; ECX the key. */
u32 svc_configure_keyboard(u32 entry)
{
    (void)entry;
    config_keys_pack(SVC_KEYS_REC);                         /* 0x19DF8..0x19E02 0x1AEE0 (ECX = 0x2000, kept) */
    text_cursor_set(2, 4, game_string_get(0x17u), 0x2000u);        /* 0x19E07..0x19E1D "LEFT PLAYER" */
    text_cursor_set(0x16, 4, game_string_get(0x16u), 0x2000u);     /* 0x19E22..0x19E3D "RIGHT PLAYER" */
    for (u32 i = 0u; i < 8u; i++)                           /* 0x19E42..0x19F3D, unrolled */
        text_cursor_set(2, (s32)(7u + 2u * i), game_string_get(DSD(SVC_KEY_LABELS + 4u * i)), 0x1000u);
    for (u32 s = 0u; s < 8u; s++) {                         /* 0x19F42..0x1A0B2, unrolled */
        (void)config_key_name(svc_key_slot_get(s), 0u, SVC_KEYS_NAME);   /* 0x19D34, 0x3157C (EDX = 0) */
        text_cursor_set(0xC, (s32)(7u + 2u * s), mem + SVC_KEYS_NAME, 0xF000u);   /* 0x2F198 */
    }
    for (u32 i = 0u; i < 8u; i++)                           /* 0x1A0B7..0x1A1B2, unrolled */
        text_cursor_set(0x16, (s32)(7u + 2u * i), game_string_get(DSD(SVC_KEY_LABELS + 4u * i)), 0x1000u);
    for (u32 s = 8u; s < 16u; s++) {                        /* 0x1A1B7..0x1A32C, unrolled */
        (void)config_key_name(svc_key_slot_get(s), 0u, SVC_KEYS_NAME);   /* 0x19D34, 0x3157C */
        text_cursor_set(0x20, (s32)(7u + 2u * (s - 8u)), mem + SVC_KEYS_NAME, 0xF000u);   /* 0x2F198 */
    }
    (void)svc_raw_key_take();                               /* 0x1A331 0x2EBBC: a stale key is dropped */
    for (u32 slot = 0u; slot < 16u; slot++) {               /* 0x1A32A xor esi,esi; 0x1A542..0x1A546 */
        const s32 col = ((s32)slot > 7 ? 0x16 : 2) + 0xA;   /* 0x1A336..0x1A35A (`jg`) */
        const s32 row = (s32)(slot % 8u) * 2 + 7;           /* 0x1A347..0x1A369 `idiv`, [esp] */
        (void)config_key_name(svc_key_slot_get(slot), 0u, SVC_KEYS_NAME);   /* 0x1A367..0x1A373 */
        text_cursor_set(col, row, mem + SVC_KEYS_NAME, 0x3000u);    /* 0x1A378..0x1A382 (ECX = 0x3000) */
        u32 key;
        for (;;) {
            config_screen_wait(1);                          /* 0x1A387..0x1A38C 0x2EA78 */
            key = svc_raw_key_take();                       /* 0x1A391..0x1A396 0x2EBBC, ECX */
            if (key == 0u) continue;                        /* 0x1A398..0x1A39A */
            if ((key & 0xFFu) == 0x1Bu) return 0u;          /* 0x1A39C..0x1A3A4 -> 0x1A556: no 0x1AE28 */
            if ((key & 0xFFu) == 0x0Du)                     /* 0x1A3AA..0x1A3AD */
                key = svc_key_slot_get(slot);               /* 0x1A3AF..0x1A3B6: Enter keeps the slot's key */
            for (u32 j = 0u; (s32)j < (s32)slot; j++) {     /* 0x1A3B8..0x1A3BC, 0x1A3DA..0x1A3DD */
                if ((svc_key_slot_get(j) & 0xFF00u) == (key & 0xFF00u)) {   /* 0x1A3BE..0x1A3D4 */
                    key = 0u;                               /* 0x1A3D6 */
                    break;
                }
            }
            if (key == 0u) continue;                        /* 0x1A3DF..0x1A3E1 */
            if (config_key_name(key, 0u, SVC_KEYS_NAME) == 0u) continue;   /* 0x1A3E3..0x1A3F3 `test al,al` */
            break;
        }
        svc_key_slot_set(slot, (u16)key);                   /* 0x1A3F5..0x1A3FE 0x19C60 */
        text_cursor_set(col, row, mem + SVC_BLANK7, 0xF000u);        /* 0x1A3F9, 0x1A403..0x1A40D: releases 7 cells */
        text_cursor_set(col, row, mem + SVC_KEYS_NAME, 0xF000u);     /* 0x1A412..0x1A421 */
        if (slot == 5u) {                                   /* 0x1A426 */
            static const char spaten[6] = { 's', 'p', 'a', 't', 'e', 'n' };
            u32 i = 0u;
            while (i < 6u && (svc_key_slot_get(i) & 0xFFu) == (u8)spaten[i]) i++;   /* 0x1A42F..0x1A49F */
            if (i == 6u) DSB(DS_00105D60) = 1u;             /* 0x1A4A1: FREE PLAY */
        }
        if (slot == 6u) {                                   /* 0x1A4A8 */
            static const char morland[7] = { 'm', 'o', 'r', 'l', 'a', 'n', 'd' };
            u32 i = 0u;
            while (i < 7u && (svc_key_slot_get(i) & 0xFFu) == (u8)morland[i]) i++;  /* 0x1A4B1..0x1A539 */
            if (i == 7u) DSB(DS_00108113) = 1u;             /* 0x1A53B */
        }
    }
    config_keys_apply(SVC_KEYS_REC);                        /* 0x1A54C..0x1A551 0x1AE28 */
    return 0u;                                              /* 0x1A556 */
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
    fn_register(0x30864u, (void (*)(void))svc_adjust_volume);
    fn_register(0x31138u, (void (*)(void))svc_handicap);
    fn_register(0x31F24u, (void (*)(void))svc_modify_controls);
    fn_register(0x32358u, (void (*)(void))svc_test_controls);
    fn_register(0x19DF0u, (void (*)(void))svc_configure_keyboard);
}
