#pragma once

// Called before and after render-device reset so device-dependent resources can
// be released and reacquired without exposing the full DX8 wrapper facade.
class RenderDeviceCleanupHook
{
public:
	virtual ~RenderDeviceCleanupHook() = default;
	virtual void ReleaseResources() = 0;
	virtual void ReAcquireResources() = 0;
};

using DX8_CleanupHook = RenderDeviceCleanupHook;
