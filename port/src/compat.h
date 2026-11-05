#ifndef PR_COMPAT_H
#define PR_COMPAT_H

/* PORT: host-portability shims, no original counterpart. The tree is built
 * with GCC/Clang on macOS and Linux and with MinGW-w64 on Windows (the release
 * build); only what MinGW lacks is covered here. */

#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
/* MinGW's mkdir takes no mode; the mode is ignored on Windows. */
#define pr_mkdir(path, mode) ((void)(mode), _mkdir(path))
/* PE/COFF objects cap an object's alignment at 0x2000; the 0x4000 of the other
 * hosts only matters to diffrun's page tracking, which is not built there. */
#define PR_MEM_ALIGN 0x1000
#else
#define pr_mkdir(path, mode) mkdir((path), (mode))
#define PR_MEM_ALIGN 0x4000
#endif

#endif
