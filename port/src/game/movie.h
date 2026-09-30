/* Boot-logo movie player (the port of 0x1C740): opens a Smacker movie by name
 * from the game directory, paces its frames on the host clock, writes the
 * palette into gfx_dac and presents each frame through gfx_present(). The
 * decoder (platform/smacker.h) is caller-owned; this module owns one static
 * SmkMovie so only one movie decodes at a time. No SDL, no mem[0xA0000]. */
#ifndef PR_MOVIE_H
#define PR_MOVIE_H

#include "types.h"

/* Plays `game_dir`/`name` (e.g. "twi5.smk"; the on-disk file is uppercase and
 * res_load_file's scan matches case-insensitively). Returns 1 when the movie
 * played to the end, was skipped at entry (a queued key, 0x1C752 kbhit, or the
 * quit flag DS_000A81A8, 0x1C75F: the entry blank only), was ended by a key
 * (left queued) or a 0x50161(0xFF00FF00) pad edge, or was cleanly skipped on a
 * missing/unreadable/unsupported file. Returns 0 on invalid arguments, and on a
 * frame that failed to decode, which ends the movie with the exit blank
 * (record named-gaps-f §F.1/§F.2). */
int movie_play(const char *game_dir, const char *name);

/* PORT: presented-frame count of the last movie_play() call — the player
 * presents every decoded frame, so a test asserts the count the player really
 * presented (like game_audio_ticks() for the audio service). 0 before any play
 * and after a skip. */
u32 movie_frames_presented(void);

/* PORT: a dump seam. The hook runs after each screen the player writes (the
 * 0x52106 entry and exit blanks and every presented frame); NULL clears it. */
void movie_set_screen_hook(void (*hook)(void));

#endif /* PR_MOVIE_H */
