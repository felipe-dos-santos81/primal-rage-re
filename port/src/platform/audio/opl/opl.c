/* Private OPL FM synthesiser wrapper (see opl.h for origin/licence).
 *
 * PORT: the chip instance is one process-wide value because the wrapper's
 * interface (opl_reset/opl_write/opl_render) has no handle parameter and the
 * original owns exactly one FM chip. It is deliberately not `static`: the rule
 * that original state lives in mem[] means port-side audio bookkeeping with no
 * original counterpart stays link-visible rather than file-local. It therefore
 * cannot be registered as an original mem[] offset either, and is not one.
 */
#include "opl.h"
#include "opal/opal.h"

Opal g_opl;

static u32 g_writes;

void opl_reset(void)
{
    opalInit(&g_opl, OPAL_OPL3_SAMPLE_RATE);
    g_writes = 0;
}

void opl_write(u16 reg, u8 value)
{
    opalWriteReg(&g_opl, (uint16_t)reg, (uint8_t)value);
    g_writes++;
}

u32 opl_write_count(void)
{
    return g_writes;
}

void opl_render(s16 *out, u32 frames)
{
    for (u32 i = 0; i < frames; i++)
    {
        int16_t left;
        int16_t right;
        opalSample(&g_opl, &left, &right);
        out[i * 2] = left;
        out[i * 2 + 1] = right;
    }
}
