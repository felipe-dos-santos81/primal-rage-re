/* Smacker (SMK2) video decoder: container parse and frame decode into a
 * caller-owned index buffer. Clean-room implementation of the public format
 * (see docs/superpowers/plans/2026-09-17-smacker-video.md "Format reference");
 * no dynamic allocation, fixed capacity, rejects what does not fit.
 *
 * The movie descriptor is caller-owned and exposed here (like SampleVoice), so
 * movie.c can hold one static SmkMovie. `data` is borrowed, not copied, and
 * must outlive the descriptor. `words[]` is the tree value arena; `tree[]` and
 * `last[]` point into it once the trees are decoded.
 *
 * PORT: the original played these through its licensed Smacker library
 * (0x1C740 / 0x10C30 / 0x11000 case 0); this port reimplements the decoder
 * rather than vendoring the LGPL/GPL alternatives.
 */
#ifndef PR_SMACKER_H
#define PR_SMACKER_H

#include "types.h"

#define SMK_TREE_WORDS 65536

typedef struct SmkMovie {
    const u8 *data; u32 len;
    u32 width, height, frames, frame_delay_us;
    u32 table_off, flags_off, trees_off, data_off, treesize;
    u32 tree_size[4];            /* header order: mmap, mclr, full, type */
    s32 *tree[4];                /* into `words` */
    s32 *last[4][3];             /* three recency slots per tree, into `words` */
    u8  pal[768];                /* current 256-entry RGB table */
    u32 next_frame;              /* frame cursor */
    s32 words[SMK_TREE_WORDS];   /* tree value arena */
} SmkMovie;

/* Parses and bounds-checks the container at `data` (length `len`) into `out`.
 * Returns 1 on a supported movie, 0 on anything else (a named reason goes to
 * stderr); `*out` is left untouched on 0. out == NULL also returns 0. */
int  smk_open(const u8 *data, u32 len, SmkMovie *out);  /* 0 = reject */
u32  smk_width(const SmkMovie *m);
u32  smk_height(const SmkMovie *m);
u32  smk_frames(const SmkMovie *m);
u32  smk_frame_delay_us(const SmkMovie *m);
int  smk_decode_frame(SmkMovie *m, u8 *frame);          /* 1 ok, 0 reject */
void smk_palette_to(const SmkMovie *m, u8 dac[256][3]);

#endif /* PR_SMACKER_H */
