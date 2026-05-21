/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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
*/

// TheSuperHackers @refactor bobtista 22/04/2026 Stage 5 —
// Standalone bgfx compatibility texture operations used while legacy surface
// and texture-copy callers are migrated to CPU-owned data.

#if defined(GGC_BGFX_STANDALONE)

#include <cstring>
#include <cstdint>

#include <d3d8.h>

#include "wwdebug.h"

// -----------------------------------------------------------------------------
// Texture / surface helpers
// -----------------------------------------------------------------------------

// Box-filter mip generator. Walks 2D textures level 1..N-1 and fills each
// by averaging the previous level's 2x2 quads. Implemented per-format so
// packed 16-bit formats (A1R5G5B5, R5G6B5, A4R4G4B4) get correct channel
// averaging instead of byte-wise garbage.
//
// TheSuperHackers @bugfix bobtista 22/04/2026 Terrain textures
// use D3DFMT_A1R5G5B5 (2 bpp). The previous 32bpp-only implementation
// both (a) read at a 32bpp stride that did not match what the stub now
// reports for 16bpp surfaces, and (b) averaged bytes as if they were
// separate channels, which silently shredded the R/G/B/A bitfields.
static inline uint16_t Filter_A1R5G5B5_4(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
	// A:1, R:5, G:5, B:5  (bits 15,14..10,9..5,4..0)
	const UINT aA = (a >> 15) & 0x1, aR = (a >> 10) & 0x1F, aG = (a >> 5) & 0x1F, aB = a & 0x1F;
	const UINT bA = (b >> 15) & 0x1, bR = (b >> 10) & 0x1F, bG = (b >> 5) & 0x1F, bB = b & 0x1F;
	const UINT cA = (c >> 15) & 0x1, cR = (c >> 10) & 0x1F, cG = (c >> 5) & 0x1F, cB = c & 0x1F;
	const UINT dA = (d >> 15) & 0x1, dR = (d >> 10) & 0x1F, dG = (d >> 5) & 0x1F, dB = d & 0x1F;
	const UINT mA = (aA + bA + cA + dA) >= 2 ? 1 : 0;
	const UINT mR = (aR + bR + cR + dR + 2) >> 2;
	const UINT mG = (aG + bG + cG + dG + 2) >> 2;
	const UINT mB = (aB + bB + cB + dB + 2) >> 2;
	return static_cast<uint16_t>((mA << 15) | (mR << 10) | (mG << 5) | mB);
}

static inline uint16_t Filter_R5G6B5_4(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
	const UINT aR = (a >> 11) & 0x1F, aG = (a >> 5) & 0x3F, aB = a & 0x1F;
	const UINT bR = (b >> 11) & 0x1F, bG = (b >> 5) & 0x3F, bB = b & 0x1F;
	const UINT cR = (c >> 11) & 0x1F, cG = (c >> 5) & 0x3F, cB = c & 0x1F;
	const UINT dR = (d >> 11) & 0x1F, dG = (d >> 5) & 0x3F, dB = d & 0x1F;
	const UINT mR = (aR + bR + cR + dR + 2) >> 2;
	const UINT mG = (aG + bG + cG + dG + 2) >> 2;
	const UINT mB = (aB + bB + cB + dB + 2) >> 2;
	return static_cast<uint16_t>((mR << 11) | (mG << 5) | mB);
}

static inline uint16_t Filter_A4R4G4B4_4(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
	const UINT aA = (a >> 12) & 0xF, aR = (a >> 8) & 0xF, aG = (a >> 4) & 0xF, aB = a & 0xF;
	const UINT bA = (b >> 12) & 0xF, bR = (b >> 8) & 0xF, bG = (b >> 4) & 0xF, bB = b & 0xF;
	const UINT cA = (c >> 12) & 0xF, cR = (c >> 8) & 0xF, cG = (c >> 4) & 0xF, cB = c & 0xF;
	const UINT dA = (d >> 12) & 0xF, dR = (d >> 8) & 0xF, dG = (d >> 4) & 0xF, dB = d & 0xF;
	const UINT mA = (aA + bA + cA + dA + 2) >> 2;
	const UINT mR = (aR + bR + cR + dR + 2) >> 2;
	const UINT mG = (aG + bG + cG + dG + 2) >> 2;
	const UINT mB = (aB + bB + cB + dB + 2) >> 2;
	return static_cast<uint16_t>((mA << 12) | (mR << 8) | (mG << 4) | mB);
}

HRESULT Standalone_Filter_Legacy_Texture_Mips(
	IDirect3DBaseTexture8 *base_texture,
	unsigned int src_level)
{
	if (base_texture == nullptr)
	{
		return E_POINTER;
	}
	if (base_texture->GetType() != D3DRTYPE_TEXTURE)
	{
		return D3D_OK;
	}
	LPDIRECT3DTEXTURE8 texture = static_cast<LPDIRECT3DTEXTURE8>(base_texture);
	const DWORD levels = texture->GetLevelCount();
	if (src_level == static_cast<unsigned int>(-1))
	{
		src_level = 0;
	}
	if (src_level >= levels)
	{
		return D3D_OK;
	}

	D3DSURFACE_DESC src_desc = {};
	if (FAILED(texture->GetLevelDesc(src_level, &src_desc)))
	{
		return E_FAIL;
	}
	UINT prev_w = src_desc.Width;
	UINT prev_h = src_desc.Height;
	const D3DFORMAT fmt = src_desc.Format;

	UINT bpp;
	switch (fmt)
	{
	case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8:
		bpp = 4; break;
	case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5:
	case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4:
		bpp = 2; break;
	case D3DFMT_A8: case D3DFMT_L8: case D3DFMT_P8: case D3DFMT_A4L4:
		bpp = 1; break;
	default:
		bpp = 4; break;
	}

	for (DWORD level = src_level + 1; level < levels; ++level)
	{
		UINT lw = prev_w >> 1; if (lw == 0) lw = 1;
		UINT lh = prev_h >> 1; if (lh == 0) lh = 1;
		D3DLOCKED_RECT src_lr = { 0 };
		D3DLOCKED_RECT dst_lr = { 0 };
		if (FAILED(texture->LockRect(level - 1, &src_lr, nullptr, 0)))
		{
			break;
		}
		if (FAILED(texture->LockRect(level, &dst_lr, nullptr, 0)))
		{
			texture->UnlockRect(level - 1);
			break;
		}
		// Clamp the second sample to the parent's extent so 1D edges
		// (2048x1, 1x1024) average with themselves instead of reading
		// past the allocated mip.
		const UINT parent_w = prev_w;
		const UINT parent_h = prev_h;
		const uint8_t * src = static_cast<const uint8_t *>(src_lr.pBits);
		uint8_t * dst = static_cast<uint8_t *>(dst_lr.pBits);
		for (UINT y = 0; y < lh; ++y)
		{
			const UINT y0 = 2 * y;
			const UINT y1 = (y0 + 1 < parent_h) ? (y0 + 1) : y0;
			for (UINT x = 0; x < lw; ++x)
			{
				const UINT x0 = 2 * x;
				const UINT x1 = (x0 + 1 < parent_w) ? (x0 + 1) : x0;
				if (bpp == 4)
				{
					const uint8_t * p00 = src + y0 * src_lr.Pitch + x0 * 4;
					const uint8_t * p10 = src + y0 * src_lr.Pitch + x1 * 4;
					const uint8_t * p01 = src + y1 * src_lr.Pitch + x0 * 4;
					const uint8_t * p11 = src + y1 * src_lr.Pitch + x1 * 4;
					uint8_t * d = dst + y * dst_lr.Pitch + x * 4;
					d[0] = static_cast<uint8_t>((p00[0] + p10[0] + p01[0] + p11[0] + 2) >> 2);
					d[1] = static_cast<uint8_t>((p00[1] + p10[1] + p01[1] + p11[1] + 2) >> 2);
					d[2] = static_cast<uint8_t>((p00[2] + p10[2] + p01[2] + p11[2] + 2) >> 2);
					d[3] = static_cast<uint8_t>((p00[3] + p10[3] + p01[3] + p11[3] + 2) >> 2);
				}
				else if (bpp == 2)
				{
					const uint16_t p00 = *reinterpret_cast<const uint16_t *>(src + y0 * src_lr.Pitch + x0 * 2);
					const uint16_t p10 = *reinterpret_cast<const uint16_t *>(src + y0 * src_lr.Pitch + x1 * 2);
					const uint16_t p01 = *reinterpret_cast<const uint16_t *>(src + y1 * src_lr.Pitch + x0 * 2);
					const uint16_t p11 = *reinterpret_cast<const uint16_t *>(src + y1 * src_lr.Pitch + x1 * 2);
					uint16_t * d = reinterpret_cast<uint16_t *>(dst + y * dst_lr.Pitch + x * 2);
					if (fmt == D3DFMT_R5G6B5)
					{
						*d = Filter_R5G6B5_4(p00, p10, p01, p11);
					}
					else if (fmt == D3DFMT_A4R4G4B4 || fmt == D3DFMT_X4R4G4B4)
					{
						*d = Filter_A4R4G4B4_4(p00, p10, p01, p11);
					}
					else
					{
						*d = Filter_A1R5G5B5_4(p00, p10, p01, p11);
					}
				}
				else
				{
					const UINT p00 = src[y0 * src_lr.Pitch + x0];
					const UINT p10 = src[y0 * src_lr.Pitch + x1];
					const UINT p01 = src[y1 * src_lr.Pitch + x0];
					const UINT p11 = src[y1 * src_lr.Pitch + x1];
					dst[y * dst_lr.Pitch + x] = static_cast<uint8_t>((p00 + p10 + p01 + p11 + 2) >> 2);
				}
			}
		}
		texture->UnlockRect(level);
		texture->UnlockRect(level - 1);
		prev_w = lw;
		prev_h = lh;
	}
	return D3D_OK;
}

// Copy src surface rows into dst surface through a pair of LockRect
// calls. Honors dst_rect for offset/extent, derives bytes-per-pixel
// from the src format so non-32bpp surfaces copy correctly. Palette,
// filter, and color-key arguments are ignored. Format conversion is
// not supported — caller must match dst and src formats (the engine's
// SurfaceClass::Copy/Stretch_Copy callers always do).
HRESULT Standalone_Copy_Legacy_Surface(
	IDirect3DSurface8 *dst,
	const RECT *dst_rect,
	IDirect3DSurface8 *src,
	const RECT *src_rect)
{
	if (dst == nullptr || src == nullptr)
	{
		return E_POINTER;
	}
	D3DSURFACE_DESC sd = {}, dd = {};
	if (FAILED(src->GetDesc(&sd)))
	{
		return E_FAIL;
	}
	if (FAILED(dst->GetDesc(&dd)))
	{
		return E_FAIL;
	}

	UINT bpp;
	switch (sd.Format)
	{
	case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8:
	case D3DFMT_A2B10G10R10: case D3DFMT_G16R16:
	case D3DFMT_X8L8V8U8: case D3DFMT_Q8W8V8U8: case D3DFMT_V16U16:
		bpp = 4; break;
	case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5:
	case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4: case D3DFMT_A8L8:
	case D3DFMT_A8R3G3B2: case D3DFMT_V8U8: case D3DFMT_L6V5U5:
		bpp = 2; break;
	case D3DFMT_R3G3B2:
	case D3DFMT_A8: case D3DFMT_L8: case D3DFMT_P8: case D3DFMT_A4L4:
		bpp = 1; break;
	default:
		bpp = 4; break;
	}

	UINT src_x = 0, src_y = 0, src_w = sd.Width, src_h = sd.Height;
	if (src_rect != nullptr)
	{
		src_x = static_cast<UINT>(src_rect->left);
		src_y = static_cast<UINT>(src_rect->top);
		src_w = static_cast<UINT>(src_rect->right - src_rect->left);
		src_h = static_cast<UINT>(src_rect->bottom - src_rect->top);
	}
	UINT dst_x = 0, dst_y = 0, dst_w = dd.Width, dst_h = dd.Height;
	if (dst_rect != nullptr)
	{
		dst_x = static_cast<UINT>(dst_rect->left);
		dst_y = static_cast<UINT>(dst_rect->top);
		dst_w = static_cast<UINT>(dst_rect->right - dst_rect->left);
		dst_h = static_cast<UINT>(dst_rect->bottom - dst_rect->top);
	}

	D3DLOCKED_RECT sl = { 0 };
	D3DLOCKED_RECT dl = { 0 };
	if (FAILED(src->LockRect(&sl, nullptr, 0)))
	{
		return E_FAIL;
	}
	if (FAILED(dst->LockRect(&dl, nullptr, 0)))
	{
		src->UnlockRect();
		return E_FAIL;
	}
	const UINT h = src_h < dst_h ? src_h : dst_h;
	const UINT w = src_w < dst_w ? src_w : dst_w;
	const UINT rowBytes = w * bpp;
	for (UINT y = 0; y < h; ++y)
	{
		std::memcpy(
			static_cast<uint8_t *>(dl.pBits) + (dst_y + y) * dl.Pitch + dst_x * bpp,
			static_cast<const uint8_t *>(sl.pBits) + (src_y + y) * sl.Pitch + src_x * bpp,
			rowBytes);
	}
	dst->UnlockRect();
	src->UnlockRect();
	return D3D_OK;
}

#endif // GGC_BGFX_STANDALONE
