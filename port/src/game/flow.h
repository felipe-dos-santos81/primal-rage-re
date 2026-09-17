/* Top-level game flow: the init chain (0x1BEC4), the master frame loop
 * (0x255CC), the per-frame update (0x24C5C) and the state machine (0x11D04).
 * This is the Task 14 seam that turns the platform modules into a running game:
 * the title-screen path is live; every callee owned by a later sub-project is
 * stubbed with a PORT marker in flow.c naming the sub-project. */
#ifndef PR_GAME_FLOW_H
#define PR_GAME_FLOW_H

/* The port of the original's game main, 0x1BEC4. Runs the init chain, then the
 * frame loop until it quits, then the teardown of 0x1BE30. Returns the process
 * exit code (always 0; the original's fatal path exits the process instead).
 * The SDL host must already be initialised by the caller (main.c). */
int game_main(void);

/* Tells game_main() which directory holds the INDEX-listed resources
 * (data/game/C). Must be called before game_main(). */
void game_set_game_dir(const char *dir);

/* 0x255CC: the master frame loop (input pump -> frame -> render -> present ->
 * pacing) until the quit flag DS_000A81A8 is set. Exposed for tests. */
void game_loop(void);

/* 0x24C5C: one per-frame update — frame counter, the two 0x94-byte player
 * records, the update process table (DS_000A8644 / DS_00104AE8), then the
 * state machine in the DAT_00104B00 == 3 mode only (the original's other modes
 * are deferred). Exposed for tests. */
void game_frame(void);

/* 0x11D04: switch(DS_000F0A64). Only the title state is ported; the other
 * cases carry PORT markers naming the sub-project that owns them. */
void game_state_step(void);

#endif /* PR_GAME_FLOW_H */
