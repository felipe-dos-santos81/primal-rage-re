/* SMK2 container parse, validation, and tree bitstream decode. See smacker.h
 * and the plan's "Format reference" (verified byte-exact on TWI5.SMK and
 * TWG.SMK). Frame decode (Task 5) is not implemented here.
 *
 * Layout, all offsets from the start of `data`:
 *   0x00 magic "SMK2"                0x34 treesize
 *   0x04 width  0x08 height          0x38 4 x tree_size (mmap, mclr, full, type)
 *   0x0C frames 0x10 pts_inc         0x68 frame_size[frames]
 *   0x14 flags                       0x68+4*frames frame_flags[frames]
 *                                    0x68+5*frames tree bitstream (treesize)
 *                                    then sum(frame_size & ~3) payloads
 */
#include <stdio.h>

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
