#include "mem.h"
#include <assert.h>

u8 mem[MEM_SIZE];

int mem_in_range(u32 addr, u32 len)
{
    if (len == 0) return 1;
    return addr < MEM_SIZE && len <= MEM_SIZE - addr;
}

void mem_fill(u32 addr, u8 value, u32 len)
{
    assert(mem_in_range(addr, len));
    for (u32 i = 0; i < len; i++) mem[addr + i] = value;
}
