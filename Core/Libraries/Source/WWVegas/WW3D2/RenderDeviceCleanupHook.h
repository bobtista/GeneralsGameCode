/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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
