#include "platform/gra.h"
#include "../mem.h"

int gra_open(u32 file_off, u32 file_len, GraChunk *out, int max, int *count)
{
    int n = 0;
    u32 off = 0;
    while (n < max) {
        if (off + 8 > file_len) return 0;
        u16 type = DSW(file_off + off);
        char m0 = (char)DSB(file_off + off + 2);
        char m1 = (char)DSB(file_off + off + 3);
        if (m0 != '4' || m1 != '3') return 0;
        u32 next = DSD(file_off + off + 4);
        if (next != 0 && (next > file_len || next <= off)) return 0;
        out[n].off = off;
        out[n].type = type;
        out[n].body_off = off + 8;
        out[n].body_len = (next ? next : file_len) - (off + 8);
        n++;
        if (next == 0) break;
        off = next;
    }
    *count = n;
    return 1;
}
