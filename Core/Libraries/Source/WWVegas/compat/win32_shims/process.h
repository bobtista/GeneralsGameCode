#pragma once

// TheSuperHackers @build bobtista 29/04/2026 process.h compatibility shim
// for non-Windows builds. _beginthread is mapped to pthread_create so legacy
// Westwood code that spawns helper threads still links. The opaque pthread_t
// type is not portably castable to uintptr_t on macOS, so the return value
// here is best-effort: 0 on failure, non-zero on success.

#include "windows.h"

#include <pthread.h>

typedef void (*PTHREAD_START_ROUTINE_VOID)(void *);

inline uintptr_t _beginthread(PTHREAD_START_ROUTINE_VOID start, unsigned /*stack*/, void *arg)
{
    pthread_t tid;
    if (pthread_create(&tid, nullptr, reinterpret_cast<void *(*)(void *)>(start), arg) != 0)
    {
        return 0;
    }
    pthread_detach(tid);
    return 1;
}

inline void _endthread(void)
{
    pthread_exit(nullptr);
}

// _spawnl mode constants. Real implementation isn't provided on non-Win;
// callers will see -1 (failure) which the engine handles gracefully.
#ifndef _P_WAIT
#define _P_WAIT     0
#endif
#ifndef _P_NOWAIT
#define _P_NOWAIT   1
#endif
#ifndef _P_OVERLAY
#define _P_OVERLAY  2
#endif
#ifndef _P_NOWAITO
#define _P_NOWAITO  3
#endif
#ifndef _P_DETACH
#define _P_DETACH   4
#endif

inline int _spawnl(int /*mode*/, const char * /*cmd*/, ...) { return -1; }
