#ifndef PR_PRAGE_EXE_H
#define PR_PRAGE_EXE_H

/* PORT: the one supported PRAGE.EXE build, by size and CRC-32 (SDL_crc32, the
 * zlib polynomial). The port reads its data object from the player's PRAGE.EXE
 * at fixed addresses, so the release's folder check refuses any other build.
 * A version fingerprint, not a security check. Measured 2026-11-05 from the
 * owner's data/game/C/PRAGE.EXE (plan 2026-11-05-github-release Task 2 Step 5):
 *   python3 -c "import zlib,sys;b=open(sys.argv[1],'rb').read();print(len(b),hex(zlib.crc32(b)))" data/game/C/PRAGE.EXE
 *   -> 1276013 0x9cfbf01a */
#define PRAGE_EXE_SIZE  1276013u
#define PRAGE_EXE_CRC32 0x9CFBF01Au

#endif
