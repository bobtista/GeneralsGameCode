#pragma once

// TheSuperHackers @build bobtista 29/04/2026 mbstring.h compat shim. The Win
// CRT exposes _mbsXxx multi-byte string helpers; on POSIX we don't use them
// so this is intentionally empty. Add stubs as needed if a TU references one.
