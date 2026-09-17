/* SMK2 container parse and validation. See smacker.h and the plan's "Format
 * reference" (verified byte-exact on TWI5.SMK and TWG.SMK). This task parses
 * the container only; the tree bitstream (Task 4) and frame decode (Task 5)
 * are not implemented here.
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

int smk_open(const u8 *data, u32 len, SmkMovie *out)
{
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

    out->data = data;
    out->len = len;
    out->width = width;
    out->height = height;
    out->frames = frames;
    /* pts_inc is a signed integer; its magnitude * 10us paces a frame. Two's
     * complement negation on the raw u32 stays defined for any input. */
    out->frame_delay_us = (((s32)pts_raw < 0) ? (0u - pts_raw) : pts_raw) * 10u;
    out->table_off = (u32)table_off;
    out->flags_off = (u32)flags_off;
    out->trees_off = (u32)trees_off;
    out->data_off = (u32)data_off;
    out->treesize = treesize;
    out->tree_size[0] = rd32(data + SMK_TREE_SIZES_OFF + 0);
    out->tree_size[1] = rd32(data + SMK_TREE_SIZES_OFF + 4);
    out->tree_size[2] = rd32(data + SMK_TREE_SIZES_OFF + 8);
    out->tree_size[3] = rd32(data + SMK_TREE_SIZES_OFF + 12);
    out->next_frame = 0;
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
