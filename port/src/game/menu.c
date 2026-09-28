/* port/src/game/menu.c */
#include "game/menu.h"

#include <string.h>

#include "game/actors.h"
#include "game/config.h"
#include "game/flow.h"
#include "platform/input.h"

#include "../mem.h"
#include "../symbols.h"

/* Data addresses (record §49-X.1). The menu state DS_00107414..DS_0010744C is
 * 0x2FFC4's; the same variables are stack slots in 0x2FA40. */
#define MENU_ACTIVE     DS_00107414   /* byte: the menu has been initialised */
#define MENU_FLAGS      DS_00107418   /* dword: the flags of the initialising call */
#define MENU_ENTRIES    DS_0010741C   /* dword: entry 1's address (the item list) */
#define MENU_STR        DS_00107420   /* dword: the current string's address */
#define MENU_COUNT      DS_00107424   /* dword: items walked (hidden ones included) */
#define MENU_OLD        DS_00107428   /* dword: the selection last drawn; -2 = none */
#define MENU_CUR        DS_0010742C   /* dword: the selected item's index */
#define MENU_SEL        DS_00107430   /* dword: the selected item's entry address */
#define MENU_ENTRY      DS_00107434   /* dword: the entry being drawn / found */
#define MENU_LEN        DS_00107438   /* dword: strlen(first string) + 5 */
#define MENU_REDRAW     DS_0010743C   /* dword: redraw everything on the next step */
#define MENU_ROW0       DS_00107440   /* dword: the first row, 5 */
#define MENU_ROW        DS_00107444   /* dword: the row being drawn */
#define MENU_RESULT     DS_00107448   /* dword: a callback's result / the wrap fuse */
#define MENU_CB         DS_0010744C   /* dword: the menu-level callback address */

#define MENU_MSG_NULL   0x00080B54u   /* "Null Menu" */
#define MENU_STR_OS     0x00080B44u   /* "OS:   " */
#define MENU_STR_MAIN   0x00080B4Cu   /* "MAIN: " */
#define MENU_DEBUG_BUF  0x000BCD5Cu   /* the string 0x2F940 hands to 0x2F41C */
#define MENU_BACKDROP   0x0009AD84u   /* the descriptor 0x2FE84 spawns */
#define MENU_KEYS_MASK  0xC300C000u   /* the pad/key mask both drivers poll */

typedef u32 (*menu_cb_fn)(u32 entry);

/* PORT: a callback is a code address stored in the table. An address that was
 * never registered (the stock tables' 0x2CB74/0x2CB94/0x2CACC/0x2CAC0 are
 * unported) is skipped and reads as result 0. */
static u32 menu_call(u32 addr, u32 arg)
{
    menu_cb_fn fn = (menu_cb_fn)(void *)fn_resolve(addr);
    if (fn == NULL) return 0u;
    return fn(arg);
}

/* 0x2EA68 — record §49-X. */
void menu_fatal_error(u32 msg)
{
    (void)msg;   /* PORT: 0x2EA68 tail-jumps to 0x62003(1) with the message; the runtime's error exit is out of scope (spec §7). */
}

/* 0x2FE40 — record §49-X. */
u32 menu_entry_find(u32 base, u32 stride, s32 idx)
{
    u32 entry = base;                                       /* 0x2FE41 */
    if (idx < 0) return 0u;                                 /* 0x2FE43..0x2FE47 `jge` */
    s32 n = 0;                                              /* 0x2FE4B */
    if (idx > 0) {                                          /* 0x2FE4D..0x2FE4F `jle` */
        do {
            if (DSD(entry) == 0u) return 0u;                /* 0x2FE51..0x2FE56 */
            n++;                                            /* 0x2FE5A */
            entry += stride;                                /* 0x2FE5B */
        } while (n < idx);                                  /* 0x2FE5D..0x2FE5F `jl` */
    }
    u32 id = DSD(entry);                                    /* 0x2FE61 */
    if (id == 0u) return 0u;                                /* 0x2FE63..0x2FE67 */
    const u8 *s = game_string_get(id);                      /* 0x2FE6D 0x1C500 */
    if ((s8)*s == 0x3F) return 0u;                          /* 0x2FE72..0x2FE7A */
    return entry;                                           /* 0x2FE7E */
}

/* 0x2F940 — record §49-X. */
void menu_debug_lines(u32 row)
{
    text_cursor_set(4, (s32)row, mem + MENU_STR_OS, 0x5000u);         /* 0x2F946..0x2F957 0x2F198 */
    text_cursor_next_line(mem + MENU_DEBUG_BUF, 0x5000u);             /* 0x2F95C..0x2F970 0x2F41C */
    text_cursor_set(4, (s32)row + 1, mem + MENU_STR_MAIN, 0x5000u);   /* 0x2F975..0x2F97D 0x2F198 */
    text_cursor_next_line(mem + DSD(DSD(DS_0010740C) + 0xCu), 0x5000u);   /* 0x2F982..0x2F98F 0x2F41C */
}

/* 0x2FE84 — record §49-X. */
void menu_title_draw(u32 entry, u32 mode_a, u32 mode_b, u32 flags)
{
    u8 buf[0x30];
    frontend_input_reset();                                 /* 0x2FE96 0x4F1E4 */
    actors_reset();                                         /* 0x2FEA4 0x2BAF4 (eax = 1) */
    frontend_origin_zero();                                 /* 0x2FEAD 0x4F1D0 */
    frontend_spawn_row((const u32 *)(mem + MENU_BACKDROP), 0u, 0u);   /* 0x2FEB7 0x38B18 (edx = ebx = 0) */
    if ((flags & 1u) != 0u)                                 /* 0x2FEBC */
        menu_debug_lines(0x1Bu);                            /* 0x2FEC8 0x2F940 */
    const u8 *p = game_string_get(DSD(entry));              /* 0x2FECD..0x2FED0 0x1C500 */
    if ((s8)*p == 0x3F) p++;                                /* 0x2FED5..0x2FEDF */
    if ((s8)*p == 0x0B) p += 2;                             /* 0x2FEE0..0x2FEE8 */
    if ((s8)*p == 0x0A) p++;                                /* 0x2FEEB..0x2FEF3 */
    s32 n = 0;                                              /* 0x2FEF4 */
    do {                                                    /* 0x2FEF6..0x2FF04 */
        u8 c = *p;
        buf[n] = c;
        if (c == 0u) break;
        n++;
        p++;
    } while (n < 0x2A);
    if (DSD(entry + 4u) != 0u) {                            /* 0x2FF06 */
        buf[n] = 0x20u;                                     /* 0x2FF0C */
        const u8 *q = game_string_get(DSD(entry + 4u));     /* 0x2FF10..0x2FF14 0x1C500 */
        n++;                                                /* 0x2FF13 */
        if (n < 0x2A) {                                     /* 0x2FF19..0x2FF1C */
            do {                                            /* 0x2FF1E..0x2FF2C */
                u8 c = *q;
                buf[n] = c;
                if (c == 0u) break;
                n++;
                q++;
            } while (n < 0x2A);
        }
    }
    u32 wide = mode_a | 2u;                                 /* 0x2FF30..0x2FF34 */
    buf[n] = 0u;                                            /* 0x2FF37 */
    if (text_width(buf, wide) > 0x28) {                     /* 0x2FF3C 0x2F0F0, 0x2FF41 `jle` */
        for (n = 0; buf[n] != 0u; n++)                      /* 0x2FF46..0x2FF68 */
            if ((s8)buf[n] == 0x5F) buf[n] = 0x20u;         /* 0x2FF56..0x2FF5B */
    } else {
        mode_a = wide;                                      /* 0x2FF6A */
    }
    text_cursor_set(-1, 0, buf, mode_a);                    /* 0x2FF6C..0x2FF77 0x2F198 */
    if ((flags & 4u) == 0u) {                               /* 0x2FF7C */
        text_cursor_set(-1, 0x1B, game_string_get(0x209u), mode_b);   /* 0x2FF83..0x2FF9B 0x1C500, 0x2F198 */
        text_cursor_set(-1, 0x1C, game_string_get(0x20Au), mode_b);   /* 0x2FFA0..0x2FFB8 0x1C500, 0x2F198 */
    }
}

/* One drawn item of 0x2FA40/0x2FFC4: the highlighting cases share the calls.
 * `cell_mode` is the 0x2F280 release mode (0 = none), `draw_mode` the 0x2F198
 * mode. Draws the entry's first string `s` at column 4 and the second string
 * 5 columns past its end. */
static void menu_item_draw(u32 entry, const u8 *s, s32 row, u32 cell_mode, u32 draw_mode,
                           int track)
{
    if (cell_mode != 0u) text_cells_release(4, row, s, cell_mode);    /* 0x2F280 */
    text_cursor_set(4, row, s, draw_mode);                            /* 0x2F198 */
    if (DSD(entry + 4u) != 0u) {                                      /* the second string */
        s32 col = (s32)strlen((const char *)s) + 5;
        const u8 *t = game_string_get(DSD(entry + 4u));               /* 0x1C500 */
        if (track) {
            DSD(MENU_LEN) = (u32)col;                                 /* 0x3021E */
            DSD(MENU_STR) = (u32)(t - mem);                           /* 0x3023A */
        }
        if (cell_mode != 0u) text_cells_release(col, row, t, cell_mode);
        text_cursor_set(col, row, t, draw_mode);
    }
}

/* 0x2FA40 — record §49-X. The stack slots [esp+4]..[esp+0x2c] are the locals
 * title, row0, hdr, flags, cb, old, sel, entry, row, count, base. */
u32 menu_run(u32 table, u32 stride, u32 flags)
{
    u32 title = table;                                      /* 0x2FA5D */
    s32 row0 = 5;                                           /* 0x2FA58 */
    u32 hdr = 1u;                                           /* 0x2FA76 */
    s32 old = -2;                                           /* 0x2FA4E, 0x2FA72 */
    s32 cur = 0;                                            /* 0x2FA6B */
    u32 sel = 0u;
    s32 count;
    u32 entry, base;
    s32 row;
    u32 cb;
    /* 0x2FA61 0x2EA74 is `xor eax,eax; mov eax,eax`, a no-op. PORT: 0x2FA6D
     * 0x2C3FC(0x100) voice, not wired (record §45-A). */
    const u8 *s = game_string_get(DSD(table + stride));     /* 0x2FA7A..0x2FA81 0x1C500 */
    if (s != NULL) {                                        /* 0x2FA88 (0x1C500 never returns 0) */
        if ((s8)*s == 0x3F) s++;                            /* 0x2FA8C..0x2FA94 */
        if ((s8)*s == 0x0B) {                               /* 0x2FA96..0x2FA9C */
            row0 += (s8)s[1] - 0x30;                        /* 0x2FA9E..0x2FAA9 */
            s += 2;                                         /* 0x2FAAB */
        }
        if (*s == 0u) title = 0u;                           /* 0x2FAB2..0x2FAB9 */
    }
    cb = DSD(table + 8u);                                   /* 0x2FABD */
    base = table + stride;                                  /* 0x2FAC0..0x2FAC6 */

L_redraw:                                                   /* 0x2FACA */
    entry = base;                                           /* 0x2FAD4 */
    sel = 0u;                                               /* 0x2FAD8 */
    count = 0;                                              /* 0x2FAE0 */
    row = row0;                                             /* 0x2FAE4 */
    if (hdr != 0u) {                                        /* 0x2FAE8 */
        /* 0x2FAEC 0x2EA64 is a `ret`. */
        menu_title_draw(title, 0x5000u, 0x1000u, flags);    /* 0x2FAF1..0x2FB03 0x2FE84 */
        if (cb != 0u) (void)menu_call(cb, 0u);              /* 0x2FB08..0x2FB11 */
        hdr = 0u;                                           /* 0x2FB15 */
    }
    while (cur != old) {                                    /* 0x2FB1B..0x2FB1F */
        u32 id = DSD(entry);                                /* 0x2FB29 */
        if (id == 0u) {                                     /* 0x2FB2B */
            old = cur;                                      /* 0x2FB2F */
            break;
        }
        const u8 *str = game_string_get(id);                /* 0x2FB3A 0x1C500 */
        s32 first = (s8)*str;                               /* 0x2FB41 */
        if (first == 0x3F) {                                /* 0x2FB45 */
            entry += stride;                                /* 0x2FB4A..0x2FB55 */
            count++;
            continue;                                       /* 0x2FB5D */
        }
        if (first == 0x0A) {                                /* 0x2FB5F */
            row++;                                          /* 0x2FB68 */
            str++;                                          /* 0x2FB69 */
        }
        row += (s32)DSD(entry + 0xCu);                      /* 0x2FB77..0x2FB84 */
        if (count == old && cur != count)                   /* 0x2FB88..0x2FB94 */
            menu_item_draw(entry, str, row, 0x2000u, 0xF000u, 0);    /* 0x2FB96..0x2FC0D unhighlight */
        else if (cur == count) {                            /* 0x2FC12 */
            sel = entry;                                    /* 0x2FC2B */
            menu_item_draw(entry, str, row, 0xF000u, 0x2000u, 0);    /* 0x2FC1C..0x2FC9B highlight */
        } else {
            menu_item_draw(entry, str, row, 0u, 0xF000u, 0);         /* 0x2FC9D..0x2FCE6 plain */
        }
        row++;                                              /* 0x2FCF7 */
        count++;                                            /* 0x2FCF8 */
        entry += stride;                                    /* 0x2FCF9 */
    }

L_poll:                                                     /* 0x2FD0C */
    {
        s32 last = count - 1;                               /* 0x2FD0C..0x2FD10 */
        if (cur != old) goto L_redraw;                      /* 0x2FD11..0x2FD15 */
        u32 keys = config_input_poll(MENU_KEYS_MASK, 1u);     /* 0x2FD1B..0x2FD29 0x2EDE0 */
        if (cb != 0u) {                                     /* 0x2FD30 */
            u32 r = menu_call(cb, sel);                     /* 0x2FD34..0x2FD38 */
            if (r != 0u) return r;                          /* 0x2FD3C..0x2FD3E */
        }
        if ((keys & 0x2000000u) != 0u) {                    /* 0x2FD44 */
            if ((flags & 4u) == 0u)                         /* 0x2FD4C */
                return (u32)((cur == count ? 1 : 0) - 1);   /* 0x2FD53..0x2FD60 */
            keys |= 0x40004000u;                            /* 0x2FD65 */
        } else if ((keys & 0x1000000u) != 0u) {             /* 0x2FD6D */
            /* 0x2FD75 0x2EA64 is a `ret`. */
            menu_title_draw(sel, 0x5000u, 0x1000u, 0u);     /* 0x2FD7A..0x2FD8A 0x2FE84 */
            old = -2;                                       /* 0x2FD93..0x2FD9A */
            (void)menu_call(DSD(sel + 8u), sel);            /* 0x2FD98..0x2FD9E */
            hdr = 1u;                                       /* 0x2FDA1..0x2FDA8 */
            keys = 0u;                                      /* 0x2FDA6 */
        }
        {
            s32 fuse = 1;                                   /* 0x2FDAC */
            u32 found = 0u;                                 /* 0x2FDB1 */
            if ((keys & 0x80008000u) != 0u) {               /* 0x2FDB3 */
                while (found == 0u) {                       /* 0x2FDBB */
                    cur--;                                  /* 0x2FDBF */
                    if (cur < 0) {                          /* 0x2FDC0..0x2FDC2 `jge` */
                        fuse--;                             /* 0x2FDC4 */
                        cur = last;                         /* 0x2FDC5 */
                        if (fuse < 0) {                     /* 0x2FDC7..0x2FDC9 `jge` */
                            menu_fatal_error(MENU_MSG_NULL);    /* 0x2FDCB..0x2FDD0 0x2EA68 */
                            return 0u;                      /* PORT: 0x62003 does not return; the port stops instead of spinning. */
                        }
                    }
                    found = menu_entry_find(base, stride, cur);   /* 0x2FDD5..0x2FDDD 0x2FE40 */
                }
            } else if ((keys & 0x40004000u) != 0u) {        /* 0x2FDE4 */
                while (found == 0u) {                       /* 0x2FDEC */
                    cur++;                                  /* 0x2FDF4 */
                    if (cur == count) {                     /* 0x2FDF5..0x2FDF7 */
                        fuse--;                             /* 0x2FDF9 */
                        cur ^= count;                       /* 0x2FDFA */
                        if (fuse < 0) {                     /* 0x2FDFC..0x2FDFE `jge` */
                            menu_fatal_error(MENU_MSG_NULL);    /* 0x2FE00..0x2FE05 0x2EA68 */
                            return 0u;                      /* PORT: 0x62003 does not return; the port stops instead of spinning. */
                        }
                    }
                    found = menu_entry_find(base, stride, cur);   /* 0x2FE0A..0x2FE12 0x2FE40 */
                }
            }
        }
        if ((flags & 1u) != 0u)                             /* 0x2FE19 */
            config_code_row(0x11, 2);                     /* 0x2FE20..0x2FE2A 0x305FC */
        /* 0x2FE2F 0x2EA74 is a no-op. */
        goto L_poll;                                        /* 0x2FE34 jmp 0x2FD11 */
    }
}

/* 0x2FFC4 — record §49-X. */
u32 menu_step(u32 table, u32 stride, u32 flags)
{
    u32 title = table;                                      /* 0x2FFC7 */
    if (DSB(MENU_ACTIVE) == 0u) {                           /* 0x2FFCD..0x2FFD4 */
        DSD(DS_00105F2C) = DSD(DS_00101500);                /* 0x2FFDA 0x500BB, 0x2FFDF */
        DSD(MENU_ENTRIES) = table + stride;                 /* 0x2FFE4..0x2FFEC */
        /* 0x2FFF1 0x2EA74 is a no-op. PORT: 0x2FFF6 0x2C3FC(0x100) voice, not
         * wired (record §45-A). */
        DSD(MENU_CUR) = 0u;                                 /* 0x30007 */
        DSD(MENU_REDRAW) = 1u;                              /* 0x3000D */
        DSD(MENU_OLD) = (u32)-2;                            /* 0x30017 */
        DSD(MENU_ROW0) = 5u;                                /* 0x30024 */
        const u8 *s = game_string_get(DSD(DSD(MENU_ENTRIES)));   /* 0x30012..0x3002A 0x1C500 */
        DSD(MENU_STR) = (u32)(s - mem);                     /* 0x3002F */
        if (s != NULL) {                                    /* 0x30034 (0x1C500 never returns 0) */
            if ((s8)*s == 0x3F) DSD(MENU_STR) += 1u;        /* 0x30038..0x30041 */
            u32 p = DSD(MENU_STR);                          /* 0x30046 */
            if ((s8)DSB(p) == 0x0B) {                       /* 0x3004B..0x30051 */
                DSD(MENU_ROW0) += (u32)((s32)(s8)DSB(p + 1u) - 0x30);   /* 0x30053..0x30063 */
                DSD(MENU_STR) = p + 2u;                     /* 0x30060..0x30065 */
            }
            if (DSB(DSD(MENU_STR)) == 0u) title = 0u;       /* 0x30070..0x3007A */
        }
        DSD(MENU_CB) = DSD(table + 8u);                     /* 0x30086..0x3008F */
        DSD(MENU_FLAGS) = flags;                            /* 0x30089 */
        DSD(MENU_ENTRIES) = table + stride;                 /* 0x30094..0x30098 */
        DSD(MENU_ENTRY) = table + stride;                   /* 0x3009E */
        DSD(MENU_SEL) = 0u;                                 /* 0x300A4 */
        DSD(MENU_COUNT) = 0u;                               /* 0x300AA */
        /* 0x300B0 0x2EA64 is a `ret`. */
        menu_title_draw(title, 0x5000u, 0x1000u, DSD(MENU_FLAGS));    /* 0x300B5..0x300BD 0x2FE84 */
        DSB(MENU_ACTIVE) = 1u;                              /* 0x300C2 */
    }
    if (DSD(MENU_REDRAW) != 0u) {                           /* 0x300C9..0x300D0 */
        if (DSD(MENU_CB) != 0u) (void)menu_call(DSD(MENU_CB), 0u);   /* 0x300D6..0x300E1 */
        DSD(MENU_ROW0) = 5u;                                /* 0x300F9 */
        DSD(MENU_ROW) = 5u;                                 /* 0x300FF */
        DSD(MENU_ENTRY) = DSD(MENU_ENTRIES);                /* 0x300EC, 0x30105 */
        DSD(MENU_COUNT) = 0u;                               /* 0x3010F */
        while (DSD(MENU_OLD) != DSD(MENU_CUR)) {            /* 0x3010A..0x30117, 0x303AF..0x303BA */
            u32 id = DSD(DSD(MENU_ENTRY));                  /* 0x3011D..0x30122 */
            if (id == 0u) {                                 /* 0x30124 */
                DSD(MENU_OLD) = DSD(MENU_CUR);              /* 0x30128..0x3012D */
                break;                                      /* 0x30132 */
            }
            const u8 *str = game_string_get(id);            /* 0x30139 0x1C500 */
            DSD(MENU_STR) = (u32)(str - mem);               /* 0x3013E */
            s32 first = (s8)*str;                           /* 0x30143 */
            if (first == 0x3F) {                            /* 0x30147 */
                DSD(MENU_ENTRY) += stride;                  /* 0x3014C..0x30154 */
                DSD(MENU_COUNT) += 1u;                      /* 0x3015A..0x30166 */
                DSD(MENU_STR) = (u32)(str + 1 - mem);       /* 0x30161 */
                continue;                                   /* 0x3016C */
            }
            if (first == 0x0A) {                            /* 0x30171 */
                DSD(MENU_ROW) += 1u;                        /* 0x30176..0x30182 */
                DSD(MENU_STR) = (u32)(str + 1 - mem);       /* 0x3017D */
            }
            DSD(MENU_ROW) += DSD(DSD(MENU_ENTRY) + 0xCu);   /* 0x30188..0x301A3 */
            u32 entry = DSD(MENU_ENTRY);
            const u8 *cur_str = mem + DSD(MENU_STR);
            s32 row = (s32)DSD(MENU_ROW);
            u32 idx = DSD(MENU_COUNT);
            if (idx == DSD(MENU_OLD) && idx != DSD(MENU_CUR)) {   /* 0x301A9..0x301BC */
                menu_item_draw(entry, cur_str, row, 0x2000u, 0xF000u, 1);   /* 0x301C2..0x30386 */
            } else if (idx == DSD(MENU_CUR)) {              /* 0x30263..0x3026E */
                DSD(MENU_SEL) = entry;                      /* 0x3028A */
                menu_item_draw(entry, cur_str, row, 0xF000u, 0x2000u, 1);   /* 0x30274..0x30386 */
            } else {
                menu_item_draw(entry, cur_str, row, 0u, 0xF000u, 1);        /* 0x30320..0x30386 */
            }
            DSD(MENU_ENTRY) = entry + stride;               /* 0x3038B..0x30398 */
            DSD(MENU_ROW) = (u32)row + 1u;                  /* 0x30392..0x303A4 */
            DSD(MENU_COUNT) = idx + 1u;                     /* 0x3039D..0x303AA */
        }
        DSD(MENU_REDRAW) = 0u;                              /* 0x303C0..0x303C2 */
    }
    u32 keys = config_input_poll_clear(MENU_KEYS_MASK, 1u);   /* 0x303C8..0x303D2 0x2EEC8 */
    u32 key = config_key_latched();                        /* 0x303D9 0x2EB80 */
    if (key != 0u && key == 0x1Bu)                          /* 0x303DE..0x303E5 */
        return (DSD(MENU_FLAGS) & 4u) != 0u ? (u32)-10 : (u32)-5;   /* 0x303E7..0x303F9, 0x30466..0x3046E */
    if (DSD(MENU_CB) != 0u) {                               /* 0x303FD */
        u32 r = menu_call(DSD(MENU_CB), DSD(MENU_SEL));     /* 0x30406..0x3040B */
        DSD(MENU_RESULT) = r;                               /* 0x30411 */
        if (r != 0u) return r;                              /* 0x30416..0x30418 */
    }
    if ((keys & 0x2000000u) != 0u) {                        /* 0x3041E */
        DSD(MENU_REDRAW) = 1u;                              /* 0x30426 */
        if ((DSD(MENU_FLAGS) & 4u) != 0u) {                 /* 0x30430 */
            DSB(MENU_ACTIVE) = 0u;                          /* 0x3043B */
            return (u32)-1;                                 /* 0x30440 */
        }
        u32 cur = DSD(MENU_CUR);                            /* 0x30449 */
        u32 count = DSD(MENU_COUNT);                        /* 0x30450 */
        DSB(MENU_ACTIVE) = 0u;                              /* 0x30456 */
        return cur == count ? 0u : (u32)-5;                 /* 0x3045C..0x30466 */
    }
    if ((keys & 0x1000000u) != 0u) {                        /* 0x3046F */
        DSB(MENU_ACTIVE) = 0u;                              /* 0x30480 */
        /* 0x30486 0x2EA64 is a `ret`. */
        menu_title_draw(DSD(MENU_SEL), 0x5000u, 0x1000u, 0u);   /* 0x3048B..0x30495 0x2FE84 */
        u32 sel = DSD(MENU_SEL);                            /* 0x3049A */
        u32 r = menu_call(DSD(sel + 8u), sel);              /* 0x304A0..0x304A2 */
        DSD(MENU_RESULT) = r;                               /* 0x304A5 */
        if (r == (u32)-10) return r;                        /* 0x304AA..0x304AD */
        if (r != (u32)-5) return 0u;                        /* 0x304B3..0x305E7 */
        DSD(MENU_REDRAW) = 1u;                              /* 0x304C6 */
        DSD(MENU_OLD) = (u32)-2;                            /* 0x304CE */
        DSB(MENU_ACTIVE) = 0u;                              /* 0x304D4 */
        return r;                                           /* 0x304DD */
    }
    DSD(MENU_ENTRY) = 0u;                                   /* 0x304E5 */
    DSD(MENU_RESULT) = 1u;                                  /* 0x304EB */
    if ((keys & 0x80008000u) != 0u) {                       /* 0x304F1 */
        for (;;) {
            if (DSD(MENU_ENTRY) != 0u) break;               /* 0x30503..0x3050B */
            s32 cur = (s32)DSD(MENU_CUR) - 1;               /* 0x30511..0x30516 */
            DSD(MENU_CUR) = (u32)cur;                       /* 0x30518 */
            if (cur < 0) {                                  /* 0x3051D `jle` (0 > cur) */
                DSD(MENU_CUR) = DSD(MENU_COUNT) - 1u;       /* 0x30521..0x3052F */
                DSD(MENU_RESULT) -= 1u;                     /* 0x30526..0x30534 */
                if ((s32)DSD(MENU_RESULT) < 0) {            /* 0x3053A `jle` (0 > result) */
                    menu_fatal_error(MENU_MSG_NULL);        /* 0x3053E..0x30540 0x2EA68 */
                    return 0u;                              /* PORT: 0x62003 does not return; the port stops instead of spinning. */
                }
            }
            DSD(MENU_ENTRY) = menu_entry_find(DSD(MENU_ENTRIES), stride,
                                              (s32)DSD(MENU_CUR));    /* 0x30545..0x30557 0x2FE40 */
        }
        DSD(MENU_REDRAW) = 1u;                              /* 0x305C5 */
    } else if ((keys & 0x40004000u) != 0u) {                /* 0x3055E */
        for (;;) {
            if (DSD(MENU_ENTRY) != 0u) break;               /* 0x3056D..0x30575 */
            u32 cur = DSD(MENU_CUR) + 1u;                   /* 0x30577..0x3057D */
            DSD(MENU_CUR) = cur;                            /* 0x30584 */
            if (cur == DSD(MENU_COUNT)) {                   /* 0x3058A */
                DSD(MENU_CUR) = DSD(MENU_ENTRY);            /* 0x3058E (0) */
                DSD(MENU_RESULT) -= 1u;                     /* 0x30594..0x3059A */
                if ((s32)DSD(MENU_RESULT) < 0) {            /* 0x305A1 `jle` (0 > result) */
                    menu_fatal_error(MENU_MSG_NULL);        /* 0x305A5..0x305A7 0x2EA68 */
                    return 0u;                              /* PORT: 0x62003 does not return; the port stops instead of spinning. */
                }
            }
            DSD(MENU_ENTRY) = menu_entry_find(DSD(MENU_ENTRIES), stride,
                                              (s32)DSD(MENU_CUR));    /* 0x305AC..0x305BE 0x2FE40 */
        }
        DSD(MENU_REDRAW) = 1u;                              /* 0x305C5 */
    }
    if ((DSD(MENU_FLAGS) & 1u) != 0u)                       /* 0x305CF */
        config_code_row(0x11, 2);                         /* 0x305D8..0x305E2 0x305FC */
    return 0u;                                              /* 0x305E7 */
}
