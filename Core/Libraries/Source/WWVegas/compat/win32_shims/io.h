#pragma once

// TheSuperHackers @build bobtista 29/04/2026 io.h compatibility shim for
// non-Windows builds. Maps Win _open/_close/etc. to POSIX equivalents via
// inline wrappers so the substitution doesn't accidentally fire inside
// system headers (e.g. <stdio.h> declares its own close()).

#include "windows.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdarg.h>

inline int _close(int fd) { return close(fd); }
inline ssize_t _read(int fd, void *buf, size_t count) { return read(fd, buf, count); }
inline ssize_t _write(int fd, const void *buf, size_t count) { return write(fd, buf, count); }
inline off_t _lseek(int fd, off_t off, int whence) { return lseek(fd, off, whence); }
inline off_t _tell(int fd) { return lseek(fd, 0, SEEK_CUR); }

inline int _open(const char *path, int flags, ...)
{
    int mode = 0;
    if (flags & O_CREAT)
    {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return open(path, flags, mode);
}

inline long _filelength(int fd)
{
    struct stat st;
    if (fstat(fd, &st) != 0)
    {
        return -1L;
    }
    return static_cast<long>(st.st_size);
}

#ifndef _O_RDONLY
#define _O_RDONLY O_RDONLY
#endif

#ifndef _O_WRONLY
#define _O_WRONLY O_WRONLY
#endif

#ifndef _O_RDWR
#define _O_RDWR O_RDWR
#endif

#ifndef _O_CREAT
#define _O_CREAT O_CREAT
#endif

#ifndef _O_TRUNC
#define _O_TRUNC O_TRUNC
#endif

#ifndef _O_APPEND
#define _O_APPEND O_APPEND
#endif

#ifndef _O_BINARY
#define _O_BINARY 0
#endif

#ifndef _O_TEXT
#define _O_TEXT 0
#endif

#ifndef _S_IREAD
#define _S_IREAD S_IRUSR
#endif

#ifndef _S_IWRITE
#define _S_IWRITE S_IWUSR
#endif
