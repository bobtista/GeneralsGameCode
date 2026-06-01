// TheSuperHackers @feature bobtista 01/06/2026  See DrawCallLog.h.

#include "DrawCallLog.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

static int s_targetFrame = -1;
static int s_interval = 0;
static char s_basePath[512] = {};
static bool s_envResolved = false;
static int s_frameIndex = 0;
static FILE * s_currentFile = nullptr;
static int s_drawIdx = 0;

void Resolve_Env()
{
    if (s_envResolved) {
        return;
    }
    s_envResolved = true;
    const char * afterEnv = std::getenv("GGC_DRAWLOG_AFTER");
    if (afterEnv != nullptr) {
        s_targetFrame = std::atoi(afterEnv);
    }
    const char * intervalEnv = std::getenv("GGC_DRAWLOG_INTERVAL");
    if (intervalEnv != nullptr) {
        s_interval = std::atoi(intervalEnv);
    }
    const char * pathEnv = std::getenv("GGC_DRAWLOG_PATH");
    if (pathEnv != nullptr) {
        std::strncpy(s_basePath, pathEnv, sizeof(s_basePath) - 1);
    }

    // Confirmation file so the operator can verify env vars were picked up.
    // GGC_DRAWLOG_* must be set as persistent user env vars (setx /
    // [Environment]::SetEnvironmentVariable) — process-scoped env vars do
    // NOT propagate to the game from PowerShell or cmd because of how the
    // game's spawn chain resets the env block. See README for details.
    if (s_basePath[0] != '\0') {
        char marker[640];
        std::snprintf(marker, sizeof(marker), "%s.startup.txt", s_basePath);
        FILE * f = std::fopen(marker, "w");
        if (f != nullptr) {
            std::fprintf(f, "after=%d interval=%d path=%s\n",
                s_targetFrame, s_interval, s_basePath);
            std::fclose(f);
        }
    }
}

bool Should_Log_This_Frame()
{
    if (s_targetFrame <= 0 || s_basePath[0] == '\0') {
        return false;
    }
    if (s_frameIndex == s_targetFrame) {
        return true;
    }
    if (s_interval > 0
        && s_frameIndex > s_targetFrame
        && ((s_frameIndex - s_targetFrame) % s_interval) == 0) {
        return true;
    }
    return false;
}

void Open_Current_File_If_Needed()
{
    if (s_currentFile != nullptr) {
        return;
    }
    char path[640];
    std::snprintf(path, sizeof(path), "%s.%06d.csv", s_basePath, s_frameIndex);
    s_currentFile = std::fopen(path, "w");
    if (s_currentFile != nullptr) {
        std::fprintf(s_currentFile,
            "draw_idx,prim_type,poly_count,vert_count,vb_type,ib_type,"
            "shader_bits,sorted_draw_flags,tex0\n");
    }
    s_drawIdx = 0;
}

} // namespace

bool DrawCallLog_Is_Active()
{
    Resolve_Env();
    return Should_Log_This_Frame();
}

void DrawCallLog_Record(
    unsigned primitive_type,
    unsigned polygon_count,
    unsigned vertex_count,
    unsigned vb_type,
    unsigned ib_type,
    unsigned shader_bits,
    unsigned sorted_draw_flags,
    const char * texture0_name)
{
    Resolve_Env();
    if (!Should_Log_This_Frame()) {
        return;
    }
    Open_Current_File_If_Needed();
    if (s_currentFile == nullptr) {
        return;
    }
    const char * name = (texture0_name != nullptr) ? texture0_name : "";
    char safe[256];
    unsigned i = 0;
    for (; name[i] != '\0' && i + 1 < sizeof(safe); ++i) {
        const char c = name[i];
        safe[i] = (c == ',' || c == '"' || c == '\n' || c == '\r') ? '_' : c;
    }
    safe[i] = '\0';

    std::fprintf(s_currentFile,
        "%d,%u,%u,%u,%u,%u,0x%08x,0x%08x,%s\n",
        s_drawIdx++,
        primitive_type, polygon_count, vertex_count,
        vb_type, ib_type,
        shader_bits, sorted_draw_flags,
        safe);
}

void DrawCallLog_End_Frame()
{
    Resolve_Env();
    if (s_currentFile != nullptr) {
        std::fclose(s_currentFile);
        s_currentFile = nullptr;
    }
    ++s_frameIndex;

    if (s_basePath[0] != '\0' && (s_frameIndex % 60) == 0) {
        char marker[640];
        std::snprintf(marker, sizeof(marker), "%s.heartbeat.txt", s_basePath);
        FILE * f = std::fopen(marker, "a");
        if (f != nullptr) {
            std::fprintf(f, "frame=%d\n", s_frameIndex);
            std::fclose(f);
        }
    }
}
