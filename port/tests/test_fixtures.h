#ifndef PR_TEST_FIXTURES_H
#define PR_TEST_FIXTURES_H

#include "types.h"

/* Scratch addresses the shared fixtures seed. They are defined here with the
 * fixtures so the fixture bodies and the per-file checks that read the same
 * addresses share one value: FIGHT_* sit above the resource heap (test_effects
 * uses 0x3F00000); ANIM_* sit away from every resource the index allocator
 * hands out (the loaded image ends near 41 MB; MEM_SIZE is 64 MB). */
#define FIGHT_ACTORS 0x3F20000u
#define FIGHT_RECS   0x3F30000u
#define ANIM_SCRATCH 0x3E00000u
#define ANIM_DESC    (ANIM_SCRATCH + 0x800u)

/* The fixtures shared by more than one test file. Each body is a byte-for-byte
 * copy of its former per-file definition; only the name and the home changed. */
void tf_snap(u8 *dst, u32 off, u32 len);
void tf_put(const u8 *src, u32 off, u32 len);
u32  tf_demo_fixture(void);
u32  tf_hit_fixture(u32 ch);
void tf_effects_fixture_begin(void);
u32  tf_anim_alloc_record(void);
u32  tf_anim_spawn_stream(void);
void tf_frontend_seed_list(u32 handle, u8 *saved);
void tf_frontend_restore_list(const u8 *saved);

#endif /* PR_TEST_FIXTURES_H */
