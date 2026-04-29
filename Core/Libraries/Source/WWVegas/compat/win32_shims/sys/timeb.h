#pragma once

// TheSuperHackers @build bobtista 29/04/2026 sys/timeb.h compatibility shim
// for non-Windows builds. Provides _ftime / struct _timeb backed by
// gettimeofday so legacy timing code keeps compiling.

#include <sys/time.h>
#include <time.h>

#ifndef _TIMEB_DEFINED
#define _TIMEB_DEFINED
struct _timeb
{
    long   time;
    short  millitm;
    short  timezone;
    short  dstflag;
};
#endif

#ifndef _ftime
inline void _ftime(struct _timeb *tb)
{
    if (tb == nullptr)
    {
        return;
    }
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    tb->time = static_cast<long>(tv.tv_sec);
    tb->millitm = static_cast<short>(tv.tv_usec / 1000);
    tb->timezone = 0;
    tb->dstflag = 0;
}
#endif
