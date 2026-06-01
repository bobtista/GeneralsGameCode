// TheSuperHackers @feature bobtista 01/06/2026
// Backend-agnostic per-frame hook that drives RenderDoc's TriggerCapture()
// when GGC_RENDERDOC_CAPTURE_AFTER=<N> is set. Optional
// GGC_RENDERDOC_CAPTURE_INTERVAL=<K> re-triggers every K frames after the
// first capture. No-op if renderdoc.dll is not injected.

#pragma once

void RenderDoc_Maybe_Trigger_Capture();
