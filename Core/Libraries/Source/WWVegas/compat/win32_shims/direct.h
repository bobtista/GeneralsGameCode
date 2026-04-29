#pragma once

// TheSuperHackers @build bobtista 29/04/2026 direct.h compatibility shim for
// non-Windows builds. Maps Win _chdir/_mkdir/_getcwd to POSIX equivalents via
// inline wrappers so the substitution doesn't fire inside system headers.

#include "windows.h"

#include <sys/stat.h>
#include <unistd.h>

inline int _chdir(const char *path) { return chdir(path); }
// TheSuperHackers @build bobtista 29/04/2026 _mkdir is provided by file_compat.h.
inline int _rmdir(const char *path) { return rmdir(path); }
inline char *_getcwd(char *buf, int size) { return getcwd(buf, size); }
