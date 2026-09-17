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

/* Test seam (opl.h): bounded record of the writes since the last reset. */
static u16 g_trace_reg[OPL_TRACE_MAX];
static u8 g_trace_val[OPL_TRACE_MAX];
static int g_trace_overflow;

void opl_reset(void)
{
    opalInit(&g_opl, OPAL_OPL3_SAMPLE_RATE);
    g_writes = 0;
    g_trace_overflow = 0;
}

void opl_write(u16 reg, u8 value)
{
    opalWriteReg(&g_opl, (uint16_t)reg, (uint8_t)value);
    if (g_writes < OPL_TRACE_MAX) {
        g_trace_reg[g_writes] = reg;
        g_trace_val[g_writes] = value;
    } else {
        g_trace_overflow = 1;
    }
    g_writes++;
}

u32 opl_write_count(void)
{
    return g_writes;
}

u16 opl_trace_reg(u32 i)
{
    return i < OPL_TRACE_MAX ? g_trace_reg[i] : 0;
}

u8 opl_trace_val(u32 i)
{
    return i < OPL_TRACE_MAX ? g_trace_val[i] : 0;
}

int opl_trace_overflow(void)
{
    return g_trace_overflow;
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
