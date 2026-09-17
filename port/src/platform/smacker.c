/* SMK2 container parse, validation, tree bitstream decode, and frame decode.
 * See smacker.h and the plan's "Format reference" (verified byte-exact on
 * TWI5.SMK and TWG.SMK against the DOSBox-X capture oracle).
 *
 * Layout, all offsets from the start of `data`:
 *   0x00 magic "SMK2"                0x34 treesize
 *   0x04 width  0x08 height          0x38 4 x tree_size (mmap, mclr, full, type)
 *   0x0C frames 0x10 pts_inc         0x68 frame_size[frames]
 *   0x14 flags                       0x68+4*frames frame_flags[frames]
 *                                    0x68+5*frames tree bitstream (treesize)
 *                                    then sum(frame_size & ~3) payloads
 *
 * A frame payload is an optional palette update (its first byte is the chunk
 * length in 4-byte units, including that byte) followed by the LSB-first video
 * bitstream.
 */
#include <stdio.h>
#include <string.h>

#include "smacker.h"

#define SMK_HEADER_SIZE 0x68
#define SMK_TREESIZE_OFF 0x34
#define SMK_TREE_SIZES_OFF 0x38
#define SMK_MAX_DIM 32768u
#define SMK_MAX_FRAMES 0xFFFFFFu

static u32 rd32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static int reject(const char *why)
{
    fprintf(stderr, "smk_open: %s\n", why);
    return 0;
}

/* ── Tree bitstream ─────────────────────────────────────────────────────────
 * See the plan's "Format reference", subsection "Trees". The region is read
 * LSB-first (bit 0 of a byte first). Every read is bounds-checked; a read past
 * the region latches `err`, after which every read returns 0 and the caller
 * rejects the movie.
 */
typedef struct {
    const u8 *p;
    u32 nbits, pos;
    int err;
} SmkBits;

static u32 smk_bit(SmkBits *b)
{
    u32 bit;
    if (b->err || b->pos >= b->nbits) {
        b->err = 1;
        return 0;
    }
    bit = (b->p[b->pos >> 3] >> (b->pos & 7)) & 1u;
    b->pos++;
    return bit;
}

static u32 smk_val(SmkBits *b, u32 n)
{
    u32 v = 0, i;
    for (i = 0; i < n; i++)
        v |= smk_bit(b) << i;
    return v;
}

#define SMK_SMALL_MAX_DEPTH 27
#define SMK_BIG_MAX_DEPTH 500

/* Small byte-tree: leaves recorded in read order with the code that reaches
 * them (bit-length and the path decisions, first read = bit 0). */
typedef struct {
    u32 n;
    u8  val[256];
    u8  len[256];
    u32 path[256];
} SmkByteTree;

static int smk_small_build(SmkBits *b, SmkByteTree *t, u32 depth, u32 path)
{
    if (depth > SMK_SMALL_MAX_DEPTH || t->n >= 256)
        return 0;
    if (smk_bit(b) == 0) {                       /* leaf: next 8 bits = value */
        t->val[t->n] = (u8)smk_val(b, 8);
        t->len[t->n] = (u8)depth;
        t->path[t->n] = path;
        t->n++;
        return 1;
    }
    /* node: left child (bit 0), then right child (bit 1) one depth deeper */
    if (!smk_small_build(b, t, depth + 1, path))
        return 0;
    return smk_small_build(b, t, depth + 1, path | (1u << depth));
}

static int smk_small_decode(SmkBits *b, const SmkByteTree *t)
{
    u32 bits = 0, len, i;
    for (len = 0; len <= SMK_SMALL_MAX_DEPTH; len++) {
        for (i = 0; i < t->n; i++)
            if (t->len[i] == len && t->path[i] == bits)
                return t->val[i];
        if (len == SMK_SMALL_MAX_DEPTH)
            break;
        if (smk_bit(b))
            bits |= 1u << len;
    }
    return -1;
}

/* Big-tree build: nodes are `0x80000000 | left_subtree_word_count` in
 * preorder; the return value is the word count of the subtree just built. */
typedef struct {
    SmkBits *bits;
    s32 *words;
    u32 cur;                                     /* next free word index */
    u32 limit;                                   /* (size+3)>>2 words max */
    s32 esc[3];
    s32 *last[3];
    const SmkByteTree *lo, *hi;
} SmkBig;

static s32 smk_big_build(SmkBig *c, u32 depth)
{
    u32 t;
    s32 r, r2;
    int k;
    if (depth > SMK_BIG_MAX_DEPTH || c->cur >= c->limit)
        return -1;
    if (smk_bit(c->bits) == 0) {                 /* leaf: a 16-bit code */
        int lo = c->lo->n ? smk_small_decode(c->bits, c->lo) : 0;
        int hi = c->hi->n ? smk_small_decode(c->bits, c->hi) : 0;
        u32 val;
        if (lo < 0 || hi < 0)
            return -1;
        val = (u32)lo | ((u32)hi << 8);
        for (k = 0; k < 3; k++) {
            if (val == (u32)c->esc[k]) {         /* escape: mark a last slot */
                c->last[k] = &c->words[c->cur];
                val = 0;
                break;
            }
        }
        c->words[c->cur++] = (s32)val;
        return 1;
    }
    t = c->cur++;
    r = smk_big_build(c, depth + 1);
    if (r < 0)
        return -1;
    c->words[t] = (s32)(0x80000000u | (u32)r);
    r++;
    r2 = smk_big_build(c, depth + 1);
    if (r2 < 0)
        return -1;
    return r + r2;
}

/* Decode one of the four trees into the arena; `used` is the running arena
 * offset. Returns 0 to reject. */
static int smk_tree(SmkMovie *m, SmkBits *b, u32 idx, u32 *used)
{
    SmkByteTree lo, hi;
    SmkBig c;
    u32 words, base;
    s32 r;
    int k;

    if (b->err)
        return 0;

    if (smk_bit(b) == 0) {                       /* absent: single 0 code */
        if (*used >= SMK_TREE_WORDS)
            return 0;
        base = *used;
        m->words[base] = 0;
        m->tree[idx] = &m->words[base];
        for (k = 0; k < 3; k++)
            m->last[idx][k] = &m->words[base];
        *used = base + 1;
        return 1;
    }

    lo.n = hi.n = 0;
    if (smk_bit(b)) {                            /* low byte-tree + skip bit */
        if (!smk_small_build(b, &lo, 0, 0))
            return 0;
        smk_bit(b);
    }
    if (smk_bit(b)) {                            /* high byte-tree + skip bit */
        if (!smk_small_build(b, &hi, 0, 0))
            return 0;
        smk_bit(b);
    }
    if (b->err)
        return 0;

    /* Bound the value count before touching the arena (3 spare words for the
     * trailing `last` slots). */
    words = (m->tree_size[idx] + 3u) >> 2;
    if (m->tree_size[idx] > (0xFFFFFFFFu >> 4) ||
        words > SMK_TREE_WORDS - 3u || *used > SMK_TREE_WORDS - 3u - words)
        return 0;

    base = *used;
    c.bits = b;
    c.words = m->words;
    c.cur = base;
    c.limit = base + words;
    c.lo = &lo;
    c.hi = &hi;
    for (k = 0; k < 3; k++) {
        c.esc[k] = (s32)smk_val(b, 16);
        c.last[k] = NULL;
    }
    r = smk_big_build(&c, 0);
    if (r < 0)
        return 0;
    smk_bit(b);                                  /* skip bit */
    if (b->err)
        return 0;

    for (k = 0; k < 3; k++)                      /* unset last slots default */
        if (c.last[k] == NULL) {
            m->words[c.cur] = 0;
            c.last[k] = &m->words[c.cur];
            c.cur++;
        }
    m->tree[idx] = &m->words[base];
    for (k = 0; k < 3; k++)
        m->last[idx][k] = c.last[k];
    *used = c.cur;
    return 1;
}

int smk_open(const u8 *data, u32 len, SmkMovie *out)
{
    SmkBits bits;
    SmkMovie tmp;
    u32 used = 0, i;

    if (out == NULL)
        return 0;
    if (data == NULL || len < SMK_HEADER_SIZE)
        return reject("truncated header");
    if (data[0] != 'S' || data[1] != 'M' || data[2] != 'K' || data[3] != '2')
        return reject("bad magic (want SMK2)");

    u32 width = rd32(data + 0x04);
    u32 height = rd32(data + 0x08);
    u32 frames = rd32(data + 0x0C);
    u32 pts_raw = rd32(data + 0x10);
    u32 treesize = rd32(data + SMK_TREESIZE_OFF);

    if (width == 0 || width > SMK_MAX_DIM || height == 0 || height > SMK_MAX_DIM)
        return reject("width/height out of range");
    if (frames == 0 || frames > SMK_MAX_FRAMES)
        return reject("frame count out of range");

    /* Bounds first: every region must lie inside the buffer before use. */
    uint64_t table_off = SMK_HEADER_SIZE;
    uint64_t flags_off = table_off + 4ull * frames;
    uint64_t trees_off = flags_off + frames;
    uint64_t data_off = trees_off + treesize;

    if (trees_off > len)
        return reject("frame table/flags run past the buffer");
    if (data_off > len)
        return reject("tree bitstream runs past the buffer");

    uint64_t payload = 0;
    for (u32 i = 0; i < frames; i++) {
        payload += rd32(data + table_off + 4ull * i) & ~3u;
        if (data_off + payload > len)
            return reject("frame payload runs past the buffer");
    }
    if (data_off + payload != len)
        return reject("payload sizes do not account for the whole file");

    /* Everything after this point can fail, so stage into `tmp` and commit to
     * `*out` only once the trees decode: the header promises `*out` is
     * untouched on a 0 return. */
    tmp.data = data;
    tmp.len = len;
    tmp.width = width;
    tmp.height = height;
    tmp.frames = frames;
    /* pts_inc is a signed integer; its magnitude * 10us paces a frame. Two's
     * complement negation on the raw u32 stays defined for any input. */
    tmp.frame_delay_us = (((s32)pts_raw < 0) ? (0u - pts_raw) : pts_raw) * 10u;
    tmp.table_off = (u32)table_off;
    tmp.flags_off = (u32)flags_off;
    tmp.trees_off = (u32)trees_off;
    tmp.data_off = (u32)data_off;
    tmp.treesize = treesize;
    tmp.tree_size[0] = rd32(data + SMK_TREE_SIZES_OFF + 0);
    tmp.tree_size[1] = rd32(data + SMK_TREE_SIZES_OFF + 4);
    tmp.tree_size[2] = rd32(data + SMK_TREE_SIZES_OFF + 8);
    tmp.tree_size[3] = rd32(data + SMK_TREE_SIZES_OFF + 12);
    tmp.next_frame = 0;
    /* The palette starts black; a frame's update may skip entries, leaving
     * them at their previous (here zero) value. */
    memset(tmp.pal, 0, sizeof(tmp.pal));

    if (treesize > (0xFFFFFFFFu >> 3))
        return reject("tree bitstream too large");
    bits.p = data + tmp.trees_off;
    bits.nbits = treesize * 8;
    bits.pos = 0;
    bits.err = 0;
    for (i = 0; i < 4; i++)
        if (!smk_tree(&tmp, &bits, i, &used))
            return reject(bits.err ? "tree bitstream overrun"
                                   : "tree value count exceeds the arena");

    *out = tmp;
    /* The arena pointers above address `tmp.words`; rebase them into the copy. */
    for (i = 0; i < 4; i++) {
        out->tree[i] = out->words + (tmp.tree[i] - tmp.words);
        for (u32 k = 0; k < 3; k++)
            out->last[i][k] = out->words + (tmp.last[i][k] - tmp.words);
    }
    return 1;
}

/* ── Frame decode ───────────────────────────────────────────────────────────
 * See the plan's "Format reference", subsections "Frame payload" and "Video
 * bitstream". The frame buffer is written in place: a SKIP or a delta patch
 * relies on the caller's previous frame still being there.
 */

/* Block-run table indexed by the block type's bits 2..7 (the format's
 * "sizetable"). */
static const u16 smk_block_runs[64] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 128, 256, 512, 1024, 2048
};

/* Standard 6-bit -> 8-bit palette gun expansion (the format's palmap). */
static const u8 smk_pal[64] = {
    0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
    0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D, 0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
    0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E, 0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
    0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF, 0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF
};

/* Palette update: the chunk's first byte is its length in 4-byte units (the
 * byte included). `p`/`n` are the whole payload; on success `*chunk` is the
 * chunk length and the update stays inside `p[0..chunk)`. Rejects an update
 * that would read outside the chunk or copy outside the 256-entry palette. */
static int smk_palette_update(SmkMovie *m, const u8 *p, u32 n, u32 *chunk)
{
    u8 old[768];
    u32 len, i = 1, sz = 0;

    if (n == 0)
        return 0;
    /* PORT: the plan's original Format reference omitted the palette chunk's
     * leading length byte; this follows the corrected reference (b2e20f4): the
     * first byte is the chunk length in 4-byte units, the byte included. */
    len = (u32)p[0] * 4u;
    if (len == 0 || len > n)
        return 0;
    memcpy(old, m->pal, sizeof(old));
    while (sz < 256) {
        u8 t;
        if (i >= len)
            return 0;
        t = p[i++];
        if (t & 0x80) {                          /* skip: keep the old entries */
            sz += (u32)(t & 0x7f) + 1;
        } else if (t & 0x40) {                    /* copy from an earlier entry */
            u32 off, cnt;
            if (i >= len)
                return 0;
            off = p[i++];
            cnt = (u32)(t & 0x3f) + 1;
            if (off + cnt > 0x100)
                return 0;
            while (cnt-- != 0 && sz < 256) {
                m->pal[sz * 3 + 0] = old[off * 3 + 0];
                m->pal[sz * 3 + 1] = old[off * 3 + 1];
                m->pal[sz * 3 + 2] = old[off * 3 + 2];
                sz++;
                off++;
            }
        } else {                                  /* new 6-bit entry */
            if (i + 2 > len)
                return 0;
            m->pal[sz * 3 + 0] = smk_pal[t & 0x3f];
            m->pal[sz * 3 + 1] = smk_pal[p[i] & 0x3f]; i++;
            m->pal[sz * 3 + 2] = smk_pal[p[i] & 0x3f]; i++;
            sz++;
        }
    }
    *chunk = len;
    return 1;
}

/* Decode one code from a big tree. Node words are `0x80000000 | left_count`;
 * the left child is `p + 1`, the right child `p + 1 + (word & 0x7FFFFFFF)`, a
 * 0 bit descends left and a 1 bit right, and a word below 0x80000000 is a leaf
 * value. The three `last` slots share storage with the tree's escape leaves:
 * a decoded value that differs from the current MRU shifts the three slots. */
static s32 smk_tree_code(SmkBits *b, const s32 *tree, s32 *mru[3])
{
    const s32 *p = tree;
    s32 v;

    while ((u32)*p & 0x80000000u) {
        u32 node = (u32)*p;
        if (smk_bit(b))
            p += 1 + (s32)(node & 0x7fffffffu);
        else
            p += 1;
    }
    v = *p;
    if (v != *mru[0]) {
        *mru[2] = *mru[1];
        *mru[1] = *mru[0];
        *mru[0] = v;
    }
    return v;
}

/* Decode the video bitstream (`p`/`n` bytes, LSB-first) into `frame`, which
 * already holds the previous frame. 4x4 blocks in raster order; every write is
 * inside width*height and every read inside the bitstream. */
static int smk_video(SmkMovie *m, u8 *frame, const u8 *p, u32 n)
{
    SmkBits b;
    u32 bw = m->width >> 2, bh = m->height >> 2;
    u32 blocks, blk = 0, k;

    if (bw == 0 || bh == 0 || n > (0xFFFFFFFFu >> 3))
        return 0;
    blocks = bw * bh;

    /* The escape/MRU slots are per frame: reset before decoding. */
    for (k = 0; k < 4; k++) {
        *m->last[k][0] = 0;
        *m->last[k][1] = 0;
        *m->last[k][2] = 0;
    }
    b.p = p;
    b.nbits = n * 8u;
    b.pos = 0;
    b.err = 0;

    /* PORT: the pixel order follows the corrected Format reference (b2e20f4),
     * not the plan's old wording: MONO uses all 16 bits of `map` (there is no
     * 8-bit truncation) and FULL's first code paints columns 2-3, the second
     * columns 0-1. */
    while (blk < blocks) {
        s32 type = smk_tree_code(&b, m->tree[3], m->last[3]);
        u32 run, mode;
        if (b.err)
            return 0;
        run = smk_block_runs[((u32)type >> 2) & 0x3fu];
        mode = (u32)type & 3u;

        if (mode == 0) {                          /* MONO */
            while (run-- != 0 && blk < blocks) {
                s32 clr = smk_tree_code(&b, m->tree[1], m->last[1]);
                s32 map = smk_tree_code(&b, m->tree[0], m->last[0]);
                u32 hi = ((u32)clr >> 8) & 0xffu;
                u32 lo = (u32)clr & 0xffu;
                u8 *o = frame + (blk / bw) * 4u * m->width + (blk % bw) * 4u;
                u32 row, col;
                if (b.err)
                    return 0;
                for (row = 0; row < 4; row++) {
                    for (col = 0; col < 4; col++)
                        o[col] = ((((u32)map >> (4 * row + col)) & 1u) ? (u8)hi : (u8)lo);
                    o += m->width;
                }
                blk++;
            }
        } else if (mode == 1) {                   /* FULL */
            while (run-- != 0 && blk < blocks) {
                u8 *o = frame + (blk / bw) * 4u * m->width + (blk % bw) * 4u;
                u32 row;
                for (row = 0; row < 4; row++) {
                    s32 c1 = smk_tree_code(&b, m->tree[2], m->last[2]);
                    s32 c2 = smk_tree_code(&b, m->tree[2], m->last[2]);
                    if (b.err)
                        return 0;
                    /* First code paints pixels 3 and 4, second pixels 1 and 2. */
                    o[2] = (u8)((u32)c1 & 0xffu);
                    o[3] = (u8)(((u32)c1 >> 8) & 0xffu);
                    o[0] = (u8)((u32)c2 & 0xffu);
                    o[1] = (u8)(((u32)c2 >> 8) & 0xffu);
                    o += m->width;
                }
                blk++;
            }
        } else if (mode == 2) {                   /* SKIP: keep the previous */
            blk += run;
            if (blk > blocks)
                blk = blocks;
        } else {                                  /* FILL */
            u8 col = (u8)(((u32)type >> 8) & 0xffu);
            while (run-- != 0 && blk < blocks) {
                u8 *o = frame + (blk / bw) * 4u * m->width + (blk % bw) * 4u;
                u32 row;
                for (row = 0; row < 4; row++) {
                    o[0] = o[1] = o[2] = o[3] = col;
                    o += m->width;
                }
                blk++;
            }
        }
    }
    return 1;
}

int smk_decode_frame(SmkMovie *m, u8 *frame)
{
    const u8 *payload;
    uint64_t off;
    u32 idx, size, pal_len = 0;

    if (m == NULL || frame == NULL || m->data == NULL)
        return 0;
    idx = m->next_frame;
    if (idx >= m->frames)
        return 0;                                 /* past the last frame */

    size = rd32(m->data + m->table_off + 4u * idx) & ~3u;
    off = m->data_off;
    for (u32 i = 0; i < idx; i++)
        off += rd32(m->data + m->table_off + 4u * i) & ~3u;
    if (off + size > m->len)
        return 0;
    payload = m->data + off;

    if ((m->data[m->flags_off + idx] & 1u) != 0) {
        if (!smk_palette_update(m, payload, size, &pal_len))
            return 0;
    }
    if (!smk_video(m, frame, payload + pal_len, size - pal_len))
        return 0;

    m->next_frame = idx + 1;
    return 1;
}

void smk_palette_to(const SmkMovie *m, u8 dac[256][3])
{
    u32 c;
    if (m == NULL || dac == NULL)
        return;
    for (c = 0; c < 256; c++) {
        dac[c][0] = m->pal[c * 3 + 0];
        dac[c][1] = m->pal[c * 3 + 1];
        dac[c][2] = m->pal[c * 3 + 2];
    }
}

u32 smk_width(const SmkMovie *m)
{
    return m ? m->width : 0;
}

u32 smk_height(const SmkMovie *m)
{
    return m ? m->height : 0;
}

u32 smk_frames(const SmkMovie *m)
{
    return m ? m->frames : 0;
}

u32 smk_frame_delay_us(const SmkMovie *m)
{
    return m ? m->frame_delay_us : 0;
}
