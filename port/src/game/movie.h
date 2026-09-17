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
 * played to the end or was cleanly skipped on a missing/unreadable/unsupported
 * file, and 0 only on a hard failure the caller should report (invalid
 * arguments, or a frame that failed to decode). ESC ends the movie cleanly. */
int movie_play(const char *game_dir, const char *name);

/* PORT: presented-frame count of the last movie_play() call — the trailing
 * ring/hold rule is the player's, so a test asserts the count the player really
 * presented (like game_audio_ticks() for the audio service). 0 before any play
 * and after a skip. */
u32 movie_frames_presented(void);

#endif /* PR_MOVIE_H */
