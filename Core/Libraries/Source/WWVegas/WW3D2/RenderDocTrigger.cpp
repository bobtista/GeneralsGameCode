// TheSuperHackers @feature bobtista 01/06/2026  See RenderDocTrigger.h.

#include "RenderDocTrigger.h"
#include "GgcRuntimeFlags.h"

#include <cstdlib>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace
{

struct RenderDocApiTable
{
    void (*entries[22])();
};

using TriggerCaptureFn = void(__cdecl *)(void);
using GetAPIFn = int(__cdecl *)(int version, void **outApiPointers);

static const int kRENDERDOC_API_Version_1_0_0 = 10000;
static const int kTriggerCaptureIndex = 15;

static RenderDocApiTable * s_api = nullptr;
static bool s_apiResolved = false;
static int s_targetFrame = -1;
static int s_interval = 0;
static int s_frameIndex = 0;
static bool s_envResolved = false;

void Resolve_Env()
{
    if (s_envResolved) {
        return;
    }
    s_envResolved = true;
    s_targetFrame = GgcFlags::IntValue(GgcFlag_RenderDocCaptureAfter);
    s_interval = GgcFlags::IntValue(GgcFlag_RenderDocCaptureInterval);
}

void Resolve_Api()
{
    if (s_apiResolved) {
        return;
    }
    s_apiResolved = true;
    HMODULE mod = GetModuleHandleA("renderdoc.dll");
    if (mod == NULL) {
        return;
    }
    GetAPIFn getApi = reinterpret_cast<GetAPIFn>(
        GetProcAddress(mod, "RENDERDOC_GetAPI"));
    if (getApi == nullptr) {
        return;
    }
    void * outPtr = nullptr;
    if (getApi(kRENDERDOC_API_Version_1_0_0, &outPtr) != 1) {
        return;
    }
    s_api = reinterpret_cast<RenderDocApiTable *>(outPtr);
}

} // namespace

void RenderDoc_Maybe_Trigger_Capture()
{
    Resolve_Env();
    if (s_targetFrame <= 0) {
        return;
    }

    ++s_frameIndex;

    bool trigger = false;
    if (s_frameIndex == s_targetFrame) {
        trigger = true;
    }
    else if (s_interval > 0
        && s_frameIndex > s_targetFrame
        && ((s_frameIndex - s_targetFrame) % s_interval) == 0) {
        trigger = true;
    }
    if (!trigger) {
        return;
    }

    Resolve_Api();
    if (s_api == nullptr) {
        return;
    }
    TriggerCaptureFn fn = reinterpret_cast<TriggerCaptureFn>(
        s_api->entries[kTriggerCaptureIndex]);
    if (fn != nullptr) {
        fn();
    }
}

#else  // !_WIN32

void RenderDoc_Maybe_Trigger_Capture()
{
}

#endif
