/* RIFF/WAVE parsing for the one shipped sample. See samples.h for the format
 * facts and the lifetime contract.
 *
 * The RIFF size field is deliberately NOT trusted: the shipped blob declares
 * 19432 (8 + that = 19440) because it carries a trailing LIST/INFO and fact
 * chunk after the PCM, while the caller may pass only the fmt+data prefix. The
 * per-chunk bounds check below is what keeps every read inside `len`.
 */
#include <stddef.h>

#include "samples.h"

#define WAVE_FMT_PCM 1
#define WAVE_FMT_MIN 16

static u16 rd16(const u8 *p)
{
    return (u16)((u16)p[0] | ((u16)p[1] << 8));
}

static u32 rd32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void zero_out(SampleVoice *out)
{
    out->pcm = NULL;
    out->frames = 0;
    out->rate = 0;
    out->channels = 0;
}

int samples_load(const u8 *data, u32 len, SampleVoice *out)
{
    if (out == NULL)
        return 0;
    zero_out(out);

    if (data == NULL || len < 12)
        return 0;
    if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F')
        return 0;
    if (data[8] != 'W' || data[9] != 'A' || data[10] != 'V' || data[11] != 'E')
        return 0;

    const u8 *fmt = NULL;
    const u8 *pcm = NULL;
    u32 data_len = 0;
    u32 off = 12;

    while (off <= len && len - off >= 8) {
        const u8 *id = data + off;
        u32 sz = rd32(data + off + 4);
        if (sz > len - off - 8)
            return 0;   /* chunk body claims bytes past the buffer */
        if (id[0] == 'f' && id[1] == 'm' && id[2] == 't' && id[3] == ' ') {
            if (sz < WAVE_FMT_MIN)
                return 0;
            fmt = data + off + 8;
        } else if (id[0] == 'd' && id[1] == 'a' && id[2] == 't' && id[3] == 'a') {
            pcm = data + off + 8;
            data_len = sz;
            break;
        }
        off += 8 + sz + (sz & 1u);
    }

    if (fmt == NULL || pcm == NULL)
        return 0;
    if (rd16(fmt) != WAVE_FMT_PCM)          /* only uncompressed PCM */
        return 0;
    if (rd16(fmt + 2) != 1)                 /* mono only (mixer's job to widen) */
        return 0;
    if (rd16(fmt + 14) != 8)                /* 8-bit unsigned only */
        return 0;
    if (data_len == 0)
        return 0;

    u32 rate = rd32(fmt + 4);
    if (rate == 0)
        return 0;

    out->pcm = pcm;
    out->frames = data_len;                 /* 8-bit mono: bytes == frames */
    out->rate = rate;
    out->channels = 1;
    return 1;
}

u32 samples_to_s16(const u8 *pcm, u32 frames, s16 *out)
{
    if (pcm == NULL || out == NULL)
        return 0;
    for (u32 i = 0; i < frames; i++)
        out[i] = (s16)(((s32)pcm[i] - 128) << 8);
    return frames;
}
