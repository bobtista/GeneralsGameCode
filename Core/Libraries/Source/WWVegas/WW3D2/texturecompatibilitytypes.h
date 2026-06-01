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

#if !defined(GGC_BGFX_STANDALONE)
#error texturecompatibilitytypes.h is only for the standalone bgfx compatibility build.
#endif

#include "WWLib/win.h"

struct NativeCompatibilitySurfaceDesc
{
	unsigned int Format = 0;
	unsigned int Type = 0;
	unsigned int Usage = 0;
	unsigned int Pool = 0;
	unsigned int Size = 0;
	unsigned int MultiSampleType = 0;
	unsigned int Width = 0;
	unsigned int Height = 0;
};

struct NativeCompatibilityVolumeDesc
{
	unsigned int Format = 0;
	unsigned int Type = 0;
	unsigned int Usage = 0;
	unsigned int Pool = 0;
	unsigned int Size = 0;
	unsigned int Width = 0;
	unsigned int Height = 0;
	unsigned int Depth = 0;
};

struct NativeCompatibilityLockedRect
{
	int Pitch = 0;
	void *pBits = nullptr;
};

struct NativeCompatibilityLockedBox
{
	int RowPitch = 0;
	int SlicePitch = 0;
	void *pBits = nullptr;
};

enum NativeCompatibilityCubeFace
{
	NATIVE_COMPATIBILITY_CUBE_FACE_POSITIVE_X = 0,
	NATIVE_COMPATIBILITY_CUBE_FACE_NEGATIVE_X = 1,
	NATIVE_COMPATIBILITY_CUBE_FACE_POSITIVE_Y = 2,
	NATIVE_COMPATIBILITY_CUBE_FACE_NEGATIVE_Y = 3,
	NATIVE_COMPATIBILITY_CUBE_FACE_POSITIVE_Z = 4,
	NATIVE_COMPATIBILITY_CUBE_FACE_NEGATIVE_Z = 5,
};

struct NativeCompatibilityBaseTexture
{
	ULONG AddRef() { return 0; }
	ULONG Release() { return 0; }
	unsigned int GetLevelCount() { return 0; }
};

struct NativeCompatibilitySurface
{
	ULONG AddRef() { return 0; }
	ULONG Release() { return 0; }
	HRESULT GetDesc(NativeCompatibilitySurfaceDesc *desc)
	{
		if (desc != nullptr) {
			*desc = NativeCompatibilitySurfaceDesc();
		}
		return E_FAIL;
	}
	HRESULT LockRect(NativeCompatibilityLockedRect *locked_rect, const RECT *rect, DWORD flags)
	{
		(void)rect;
		(void)flags;
		if (locked_rect != nullptr) {
			*locked_rect = NativeCompatibilityLockedRect();
		}
		return E_FAIL;
	}
	HRESULT UnlockRect() { return E_FAIL; }
};

struct NativeCompatibilityTexture2D : NativeCompatibilityBaseTexture
{
	HRESULT GetSurfaceLevel(UINT level, NativeCompatibilitySurface **surface)
	{
		(void)level;
		if (surface != nullptr) {
			*surface = nullptr;
		}
		return E_FAIL;
	}
	HRESULT GetLevelDesc(UINT level, NativeCompatibilitySurfaceDesc *desc)
	{
		(void)level;
		if (desc != nullptr) {
			*desc = NativeCompatibilitySurfaceDesc();
		}
		return E_FAIL;
	}
	HRESULT LockRect(UINT level, NativeCompatibilityLockedRect *locked_rect, const RECT *rect, DWORD flags)
	{
		(void)level;
		(void)rect;
		(void)flags;
		if (locked_rect != nullptr) {
			*locked_rect = NativeCompatibilityLockedRect();
		}
		return E_FAIL;
	}
	HRESULT UnlockRect(UINT level)
	{
		(void)level;
		return E_FAIL;
	}
};

struct NativeCompatibilityCubeTexture : NativeCompatibilityBaseTexture
{
	HRESULT GetLevelDesc(UINT level, NativeCompatibilitySurfaceDesc *desc)
	{
		(void)level;
		if (desc != nullptr) {
			*desc = NativeCompatibilitySurfaceDesc();
		}
		return E_FAIL;
	}
	HRESULT LockRect(
		NativeCompatibilityCubeFace face,
		UINT level,
		NativeCompatibilityLockedRect *locked_rect,
		const RECT *rect,
		DWORD flags)
	{
		(void)face;
		(void)level;
		(void)rect;
		(void)flags;
		if (locked_rect != nullptr) {
			*locked_rect = NativeCompatibilityLockedRect();
		}
		return E_FAIL;
	}
	HRESULT UnlockRect(NativeCompatibilityCubeFace face, UINT level)
	{
		(void)face;
		(void)level;
		return E_FAIL;
	}
};

struct NativeCompatibilityVolumeTexture : NativeCompatibilityBaseTexture
{
	HRESULT GetLevelDesc(UINT level, NativeCompatibilityVolumeDesc *desc)
	{
		(void)level;
		if (desc != nullptr) {
			*desc = NativeCompatibilityVolumeDesc();
		}
		return E_FAIL;
	}
	HRESULT LockBox(UINT level, NativeCompatibilityLockedBox *locked_box, const void *box, DWORD flags)
	{
		(void)level;
		(void)box;
		(void)flags;
		if (locked_box != nullptr) {
			*locked_box = NativeCompatibilityLockedBox();
		}
		return E_FAIL;
	}
	HRESULT UnlockBox(UINT level)
	{
		(void)level;
		return E_FAIL;
	}
};
