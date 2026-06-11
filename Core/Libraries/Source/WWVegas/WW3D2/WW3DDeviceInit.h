/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#pragma once

// TheSuperHackers @refactor bobtista 11/06/2026 Backend-neutral WW3D subsystem
// bring-up/teardown, extracted verbatim from DX8Wrapper's device-dependent
// init/shutdown so the bgfx backend can drive the device lifecycle without
// compiling dx8wrapper.cpp. These touch only WW3D engine subsystems; the
// backend's own g_renderBackend Initialize/Shutdown and the DX8Wrapper-internal
// caps + default-render-state setup stay with their owners.
namespace WW3DDeviceInit
{
	void Init_Subsystems();
	void Shutdown_Subsystems();
}
