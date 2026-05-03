/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// TheSuperHackers @refactor bobtista 21/04/2026 Texture-cache half of the
// bgfx backend. Split out of BgfxBackend.cpp to keep each file under
// reasonable length. Shared state lives in BgfxBackendState.h.
//
// Contents: format translator, alpha-fixup helper, EnsureBgfxTexture
// (the core upload-or-reuse path), plus the three BgfxBackend class
// methods that touch the cache: Invalidate_Cached_Texture,
// Release_Cached_Texture, Capture_Shroud_Texture.

#include <cstring>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include <bgfx/bgfx.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "dx8wrapper.h"
#include "texture.h"
#include "surfaceclass.h"
#include "wwdebug.h"

#include "BgfxBackend.h"
#include "BgfxBackendState.h"
#include "DXTUtils.h"

namespace
{
static const bgfx::ViewId kBgfxRTTTextureCopyView = 3;
}

// External linkage so BgfxBackend.cpp can reference this from its
// Create_Texture implementation.
bgfx::TextureFormat::Enum TranslateWW3DFormat(WW3DFormat fmt)
{
    switch (fmt)
    {
        case WW3D_FORMAT_A8R8G8B8:
        case WW3D_FORMAT_X8R8G8B8:
            // D3D8 stores ARGB as 0xAARRGGBB which on little-endian
            // memory is BB GG RR AA - matches bgfx BGRA8 byte order.
            return bgfx::TextureFormat::BGRA8;
        case WW3D_FORMAT_R5G6B5:    return bgfx::TextureFormat::R5G6B5;
        case WW3D_FORMAT_A1R5G5B5:  return bgfx::TextureFormat::BGR5A1;
        case WW3D_FORMAT_A4R4G4B4:  return bgfx::TextureFormat::BGRA4;
        case WW3D_FORMAT_A8:        return bgfx::TextureFormat::A8;
        case WW3D_FORMAT_L8:        return bgfx::TextureFormat::R8;
        case WW3D_FORMAT_DXT1:      return bgfx::TextureFormat::BC1;
        case WW3D_FORMAT_DXT2:
        case WW3D_FORMAT_DXT3:      return bgfx::TextureFormat::BC2;
        case WW3D_FORMAT_DXT4:
        case WW3D_FORMAT_DXT5:      return bgfx::TextureFormat::BC3;
        default:                    return bgfx::TextureFormat::Unknown;
    }
}
// TheSuperHackers @refactor bobtista 20/04/2026 D3D8 ignores the X byte of X8R8G8B8 and samples alpha as 1.0. bgfx BGRA8 samples memory literally, so FFmpeg-produced procedural frames (BGR0, alpha byte = 0) would draw transparent under SRC_ALPHA blending. Force alpha=0xFF only when the texture has no file path, so TGA-loaded X8R8G8B8 textures (scorch marks, decals) keep their real alpha data.
static void ForceOpaqueIfProceduralX8R8G8B8(TextureClass * tex2d,
    bgfx::TextureFormat::Enum bgfxFmt, const bgfx::Memory * mem,
    unsigned expectedPitch, unsigned numRows)
{
    if (tex2d == nullptr || mem == nullptr)
    {
        return;
    }
    if (bgfxFmt != bgfx::TextureFormat::BGRA8)
    {
        return;
    }
    if (tex2d->Get_Texture_Format() != WW3D_FORMAT_X8R8G8B8)
    {
        return;
    }
    if (!tex2d->Get_Full_Path().Is_Empty())
    {
        return;
    }
    uint8_t * px = mem->data;
    const unsigned pixelCount = (expectedPitch / 4) * numRows;
    for (unsigned i = 0; i < pixelCount; ++i)
    {
        px[i * 4 + 3] = 0xff;
    }
}

static void ApplyTeamColorTextureKey(TextureClass * tex2d,
    bgfx::TextureFormat::Enum bgfxFmt, const bgfx::Memory * mem,
    unsigned expectedPitch, unsigned numRows)
{
    if (tex2d == nullptr || mem == nullptr)
    {
        return;
    }
    if (bgfxFmt != bgfx::TextureFormat::BGRA8)
    {
        return;
    }

    const char * texName = tex2d->Get_Full_Path().str();
    if (texName == nullptr || texName[0] != '#')
    {
        return;
    }

    uint8_t * pix = mem->data;
    const unsigned pixelCount = expectedPitch / 4 * numRows;
    for (unsigned i = 0; i < pixelCount; ++i)
    {
        // TheSuperHackers @fix bobtista 29/04/2026 Team-colored textures
        // use exact black RGB as a transparent matte in the D3D8 path.
        if (pix[i * 4 + 0] == 0 && pix[i * 4 + 1] == 0 && pix[i * 4 + 2] == 0)
        {
            pix[i * 4 + 3] = 0;
        }
    }
}

static bool IsCompressedBgfxFormat(bgfx::TextureFormat::Enum bgfxFmt)
{
    return bgfxFmt == bgfx::TextureFormat::BC1
        || bgfxFmt == bgfx::TextureFormat::BC2
        || bgfxFmt == bgfx::TextureFormat::BC3;
}

static unsigned GetBytesPerPixel(bgfx::TextureFormat::Enum bgfxFmt)
{
    switch (bgfxFmt)
    {
        case bgfx::TextureFormat::BGRA4:
        case bgfx::TextureFormat::R5G6B5:
        case bgfx::TextureFormat::BGR5A1:
            return 2;
        case bgfx::TextureFormat::A8:
        case bgfx::TextureFormat::R8:
            return 1;
        default:
            return 4;
    }
}

static bool IsTerrainAtlasTexture(TextureClass * tex2d,
    const D3DSURFACE_DESC & desc,
    bgfx::TextureFormat::Enum bgfxFmt)
{
	// TheSuperHackers @bugfix bobtista 28/04/2026 The terrain texture is a
	// sparse tile atlas with black unused space between classes. D3D8's
	// terrain path relies on tile-aware source mips and authored borders,
	// while bgfx creates a full mip chain whenever mips are enabled. Upload a
	// complete atlas-safe chain only for textures explicitly tagged by the
	// terrain atlas builder.
	return tex2d != nullptr
		&& desc.Format == D3DFMT_A1R5G5B5
		&& bgfxFmt == bgfx::TextureFormat::BGR5A1
		&& tex2d->Has_Atlas_Regions();
}

static unsigned GetFullMipCount(unsigned width, unsigned height)
{
	unsigned count = 1;
	while (width > 1 || height > 1)
	{
		width = (width > 1) ? width >> 1 : 1;
		height = (height > 1) ? height >> 1 : 1;
		++count;
	}
	return count;
}

static bool IsTerrainAtlasRegionPixelValid(const std::vector<TextureClass::TextureAtlasRegion> & regions,
	unsigned x, unsigned y, unsigned level)
{
	const unsigned scale = 1u << level;
	for (unsigned i = 0; i < regions.size(); ++i)
	{
		const TextureClass::TextureAtlasRegion & region = regions[i];
		const unsigned x0 = region.X / scale;
		const unsigned y0 = region.Y / scale;
		const unsigned x1 = (region.X + region.Width + scale - 1) / scale;
		const unsigned y1 = (region.Y + region.Height + scale - 1) / scale;
		if (x >= x0 && x < x1 && y >= y0 && y < y1)
		{
			return true;
		}
	}

	return false;
}

static uint16_t FilterTerrainAtlasA1R5G5B5(const uint16_t samples[4], const bool validSamples[4], bool & valid)
{
    unsigned count = 0;
    unsigned red = 0;
    unsigned green = 0;
    unsigned blue = 0;
    for (unsigned i = 0; i < 4; ++i)
    {
        if (validSamples[i])
        {
            count++;
            red += (samples[i] >> 10) & 0x1f;
            green += (samples[i] >> 5) & 0x1f;
            blue += samples[i] & 0x1f;
        }
    }
    if (count == 0)
    {
		valid = false;
        return 0;
    }
	valid = true;
    red = (red + count / 2) / count;
    green = (green + count / 2) / count;
    blue = (blue + count / 2) / count;
    return static_cast<uint16_t>(0x8000 | (red << 10) | (green << 5) | blue);
}

static void BuildTerrainAtlasMip(const std::vector<uint16_t> & src,
    unsigned srcWidth, unsigned srcHeight, std::vector<uint16_t> & dst,
    std::vector<unsigned char> & validMask, unsigned dstWidth,
	unsigned dstHeight, const std::vector<TextureClass::TextureAtlasRegion> & regions,
	unsigned sourceLevel)
{
    for (unsigned y = 0; y < dstHeight; ++y)
    {
        const unsigned y0 = y * 2;
        const unsigned y1 = (y0 + 1 < srcHeight) ? y0 + 1 : y0;
        for (unsigned x = 0; x < dstWidth; ++x)
        {
            const unsigned x0 = x * 2;
            const unsigned x1 = (x0 + 1 < srcWidth) ? x0 + 1 : x0;
			const uint16_t samples[4] = {
				src[y0 * srcWidth + x0],
				src[y0 * srcWidth + x1],
				src[y1 * srcWidth + x0],
				src[y1 * srcWidth + x1]
			};
			const bool validSamples[4] = {
				IsTerrainAtlasRegionPixelValid(regions, x0, y0, sourceLevel),
				IsTerrainAtlasRegionPixelValid(regions, x1, y0, sourceLevel),
				IsTerrainAtlasRegionPixelValid(regions, x0, y1, sourceLevel),
				IsTerrainAtlasRegionPixelValid(regions, x1, y1, sourceLevel)
			};
			bool valid = false;
            dst[y * dstWidth + x] = FilterTerrainAtlasA1R5G5B5(samples, validSamples, valid);
			validMask[y * dstWidth + x] = valid ? 1 : 0;
        }
    }
}

static void BleedTerrainAtlasMipGaps(std::vector<uint16_t> & pixels,
    std::vector<unsigned char> & validMask, unsigned width, unsigned height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    bool anyValid = false;
    for (unsigned i = 0; i < width * height; ++i)
    {
        if (validMask[i] != 0)
        {
            anyValid = true;
            break;
        }
    }
    if (!anyValid)
    {
        return;
    }

	uint16_t fallback = 0;
	for (unsigned i = 0; i < width * height; ++i)
	{
		if (validMask[i] != 0)
		{
			fallback = pixels[i];
			break;
		}
	}

    for (unsigned y = 0; y < height; ++y)
    {
        uint16_t last = 0;
        for (unsigned x = 0; x < width; ++x)
        {
            const unsigned index = y * width + x;
            if (validMask[index] != 0)
            {
                last = pixels[index];
            }
            else if (last != 0)
            {
                pixels[index] = last;
				validMask[index] = 1;
            }
        }

        last = 0;
        for (unsigned x = width; x > 0; --x)
        {
            const unsigned index = y * width + x - 1;
            if (validMask[index] != 0)
            {
                last = pixels[index];
            }
            else if (last != 0)
            {
                pixels[index] = last;
				validMask[index] = 1;
            }
        }
    }

    for (unsigned x = 0; x < width; ++x)
    {
        uint16_t last = 0;
        for (unsigned y = 0; y < height; ++y)
        {
            const unsigned index = y * width + x;
            if (validMask[index] != 0)
            {
                last = pixels[index];
            }
            else if (last != 0)
            {
                pixels[index] = last;
				validMask[index] = 1;
            }
        }

        last = 0;
        for (unsigned y = height; y > 0; --y)
        {
            const unsigned index = (y - 1) * width + x;
            if (validMask[index] != 0)
            {
                last = pixels[index];
            }
            else if (last != 0)
            {
                pixels[index] = last;
				validMask[index] = 1;
            }
        }
    }

	for (unsigned i = 0; i < width * height; ++i)
	{
		if (validMask[i] == 0)
		{
			pixels[i] = fallback;
		}
	}
}

static bool CopyTextureLevel(TextureClass * tex2d,
    bgfx::TextureFormat::Enum bgfxFmt,
    IDirect3DTexture8 * d3dTex,
    unsigned level,
    bgfx::Memory const ** outMem,
    uint16_t * outWidth,
    uint16_t * outHeight)
{
    if (tex2d == nullptr || d3dTex == nullptr || outMem == nullptr
        || outWidth == nullptr || outHeight == nullptr)
    {
        return false;
    }

    D3DSURFACE_DESC desc;
    if (FAILED(d3dTex->GetLevelDesc(level, &desc)))
    {
        return false;
    }

    D3DLOCKED_RECT locked = { 0 };
    HRESULT hr = d3dTex->LockRect(level, &locked, NULL, D3DLOCK_READONLY);
    if (FAILED(hr) || locked.pBits == NULL)
    {
        return false;
    }

    const bool isCompressed = IsCompressedBgfxFormat(bgfxFmt);
    unsigned expectedPitch = 0;
    unsigned numRows = 0;
    if (isCompressed)
    {
        const unsigned blockSize = (bgfxFmt == bgfx::TextureFormat::BC1) ? 8 : 16;
        expectedPitch = DXT_SurfacePitch(desc.Width, blockSize);
        numRows = DXT_SurfaceRows(desc.Height);
    }
    else
    {
        expectedPitch = desc.Width * GetBytesPerPixel(bgfxFmt);
        numRows = desc.Height;
    }

    const unsigned totalBytes = numRows * expectedPitch;
    const unsigned srcPitch = static_cast<unsigned>(locked.Pitch);
    const bgfx::Memory * mem = bgfx::alloc(totalBytes);
    if (srcPitch == expectedPitch)
    {
        std::memcpy(mem->data, locked.pBits, totalBytes);
    }
    else
    {
        const unsigned copyPitch = (srcPitch < expectedPitch) ? srcPitch : expectedPitch;
        const uint8_t * src = static_cast<const uint8_t *>(locked.pBits);
        uint8_t * dst = mem->data;
        for (unsigned row = 0; row < numRows; ++row)
        {
            std::memcpy(dst, src, copyPitch);
            if (copyPitch < expectedPitch)
            {
                std::memset(dst + copyPitch, 0, expectedPitch - copyPitch);
            }
            src += srcPitch;
            dst += expectedPitch;
        }
    }
    d3dTex->UnlockRect(level);

    if (!isCompressed)
    {
        ApplyTeamColorTextureKey(tex2d, bgfxFmt, mem, expectedPitch, numRows);
        ForceOpaqueIfProceduralX8R8G8B8(tex2d, bgfxFmt, mem, expectedPitch, numRows);
    }

    *outMem = mem;
    *outWidth = static_cast<uint16_t>(desc.Width);
    *outHeight = static_cast<uint16_t>(desc.Height);
    g_stats.textureCopies++;
    return true;
}

static bool UploadTerrainAtlasMips(TextureClass * tex2d,
	bgfx::TextureHandle h, IDirect3DTexture8 * d3dTex, unsigned mipCount)
{
	std::vector<uint16_t> prev;
	unsigned prevWidth = 0;
	unsigned prevHeight = 0;
	unsigned fullMipCount = mipCount;

    for (unsigned mip = 0; mip < mipCount; ++mip)
    {
		const bgfx::Memory * mem = nullptr;
		uint16_t mipWidth = 0;
		uint16_t mipHeight = 0;
		if (!CopyTextureLevel(tex2d, bgfx::TextureFormat::BGR5A1, d3dTex, mip,
							  &mem, &mipWidth, &mipHeight))
		{
			return false;
		}
		prev.resize(mipWidth * mipHeight);
		std::memcpy(&prev[0], mem->data, prev.size() * sizeof(uint16_t));
		prevWidth = mipWidth;
		prevHeight = mipHeight;
		bgfx::updateTexture2D(h, 0, static_cast<uint8_t>(mip), 0, 0,
			mipWidth, mipHeight, mem);
		g_stats.textureUploads++;
		if (mip == 0)
		{
			fullMipCount = GetFullMipCount(mipWidth, mipHeight);
		}
    }

	for (unsigned mip = mipCount; mip < fullMipCount; ++mip)
	{
		const unsigned nextWidth = (prevWidth > 1) ? prevWidth >> 1 : 1;
		const unsigned nextHeight = (prevHeight > 1) ? prevHeight >> 1 : 1;
		std::vector<uint16_t> next(nextWidth * nextHeight);
		std::vector<unsigned char> validMask(nextWidth * nextHeight);
		BuildTerrainAtlasMip(prev, prevWidth, prevHeight, next, validMask,
			nextWidth, nextHeight, tex2d->Get_Atlas_Regions(), mip - 1);
		BleedTerrainAtlasMipGaps(next, validMask, nextWidth, nextHeight);
		const bgfx::Memory * nextMem = bgfx::copy(&next[0], static_cast<uint32_t>(next.size() * sizeof(uint16_t)));
		bgfx::updateTexture2D(h, 0, static_cast<uint8_t>(mip), 0, 0,
			static_cast<uint16_t>(nextWidth), static_cast<uint16_t>(nextHeight),
			nextMem, static_cast<uint16_t>(nextWidth * sizeof(uint16_t)));
		g_stats.textureUploads++;
		prev.swap(next);
		prevWidth = nextWidth;
		prevHeight = nextHeight;
	}
    return true;
}

// External linkage: called from BgfxBackend.cpp's Set_Texture path.
bgfx::TextureHandle EnsureBgfxTexture(TextureBaseClass * tex)
{
    if (tex == nullptr)
    {
        return BGFX_INVALID_HANDLE;
    }
    IDirect3DBaseTexture8 * curD3D = tex->Peek_D3D_Base_Texture();

    auto it = g_caches.texture.find(tex);
    if (it != g_caches.texture.end())
    {
        auto d3dIt = g_caches.d3dPtr.find(tex);
        bool d3dPtrMatch = (d3dIt != g_caches.d3dPtr.end() && d3dIt->second.ptr == curD3D);
        if (d3dPtrMatch)
        {
            return it->second;
        }
        // D3D8 texture pointer changed — the TextureClass* address was reused
        // for a different texture, OR the engine invalidated via
        // Invalidate_Cached_Texture after writing new pixels (e.g. video
        // frame, font atlas). Try to UPDATE the existing bgfx handle in
        // place (no handle churn) if dimensions and format match.
        // Only fall back to destroy+recreate if they differ.
        // TheSuperHackers @bugfix bobtista 20/04/2026 Capture the stored
        // dimensions BEFORE overwriting the cache entry — the in-place
        // update path below checks against them, and the iterator aliases
        // the same entry we are about to stomp.
        uint16_t cachedW = 0;
        uint16_t cachedH = 0;
        if (d3dIt != g_caches.d3dPtr.end())
        {
            cachedW = d3dIt->second.w;
            cachedH = d3dIt->second.h;
        }
        g_caches.d3dPtr[tex] = { curD3D, 0, 0 };
        if (bgfx::isValid(it->second))
        {
            TextureClass * tex2d = tex->As_TextureClass();
            if (tex2d != nullptr)
            {
                IDirect3DTexture8 * d3dTex = static_cast<IDirect3DTexture8 *>(curD3D);
                D3DSURFACE_DESC desc;
                if (SUCCEEDED(d3dTex->GetLevelDesc(0, &desc)))
                {
					const bgfx::TextureFormat::Enum bgfxFmt = TranslateWW3DFormat(tex2d->Get_Texture_Format());
					const bool terrainAtlasSafeMips = IsTerrainAtlasTexture(tex2d, desc, bgfxFmt);
                    // Check if dimensions match the existing bgfx handle
                    if (desc.Width == cachedW
                        && desc.Height == cachedH
                        && bgfxFmt != bgfx::TextureFormat::Unknown)
                    {
                        const unsigned mipCount = d3dTex->GetLevelCount();
                        bool updatedAllLevels = true;
                        if (terrainAtlasSafeMips)
                        {
                            updatedAllLevels = UploadTerrainAtlasMips(tex2d, it->second, d3dTex, mipCount);
                        }
                        else
                        {
                            for (unsigned mip = 0; mip < mipCount; ++mip)
                            {
                                const bgfx::Memory * mem = nullptr;
                                uint16_t mipWidth = 0;
                                uint16_t mipHeight = 0;
                                if (!CopyTextureLevel(tex2d, bgfxFmt, d3dTex, mip,
                                                      &mem, &mipWidth, &mipHeight))
                                {
                                    updatedAllLevels = false;
                                    break;
                                }
                                bgfx::updateTexture2D(it->second, 0,
                                    static_cast<uint8_t>(mip), 0, 0,
                                    mipWidth, mipHeight, mem);
                                g_stats.textureUploads++;
                            }
                        }
                        if (updatedAllLevels)
                        {
                            // Store dimensions so subsequent invalidations
                            // can still match the in-place update path.
                            g_caches.d3dPtr[tex] = { curD3D,
                                static_cast<uint16_t>(desc.Width),
                                static_cast<uint16_t>(desc.Height) };
                            return it->second; // Reused handle, no churn
                        }
                    }
                }
            }
            // Dimensions or format changed — must destroy and recreate
            g_caches.deferredDestroys.push_back(it->second);
        }
        g_caches.texture.erase(it);
    }
    else
    {
        g_caches.d3dPtr[tex] = { curD3D, 0, 0 };
    }

    // Only handle TextureClass (regular 2D) for now. Cube and volume
    // textures take a different path and would need their own helpers.
    TextureClass * tex2d = tex->As_TextureClass();
    if (tex2d == nullptr)
    {
        g_caches.texture[tex] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    if (tex->Get_Pool() == TextureBaseClass::POOL_DEFAULT)
    {
        auto fbIt = g_caches.framebuffer.find(tex);
        if (fbIt != g_caches.framebuffer.end())
        {
            static bool s_loggedRTTResolve = false;
            if (!s_loggedRTTResolve)
            {
                s_loggedRTTResolve = true;
                WWDEBUG_SAY(("[BgfxBackend] RTT RESOLVE: POOL_DEFAULT tex=%p "
                             "resolved to framebuffer color texture %dx%d",
                             tex, fbIt->second.width, fbIt->second.height));
            }
            return fbIt->second.colorTex;
        }

        if (curD3D == nullptr)
        {
            g_caches.renderTarget[tex] = true;
            return BGFX_INVALID_HANDLE;
        }
    }

    const bgfx::TextureFormat::Enum bgfxFmt = TranslateWW3DFormat(tex2d->Get_Texture_Format());
    if (bgfxFmt == bgfx::TextureFormat::Unknown)
    {
        static bool s_loggedUnknownFmt = false;
        if (!s_loggedUnknownFmt)
        {
            s_loggedUnknownFmt = true;
            WWDEBUG_SAY(("[BgfxBackend] UNKNOWN texture format: %s ww3dfmt=%d",
                         tex2d->Get_Full_Path().str(),
                         static_cast<int>(tex2d->Get_Texture_Format())));
        }
        g_caches.texture[tex] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    IDirect3DBaseTexture8 * baseTex = tex->Peek_D3D_Base_Texture();
    if (baseTex == nullptr)
    {
        static bool s_loggedNullBase = false;
        if (!s_loggedNullBase)
        {
            s_loggedNullBase = true;
            WWDEBUG_SAY(("[BgfxBackend] EnsureBgfxTexture: Peek_D3D_Base_Texture returned null for %s",
                         tex2d->Get_Full_Path().str()));
        }
        return BGFX_INVALID_HANDLE;
    }

    IDirect3DTexture8 * d3dTex = static_cast<IDirect3DTexture8 *>(baseTex);

    D3DSURFACE_DESC desc;
    if (FAILED(d3dTex->GetLevelDesc(0, &desc)))
    {
        static bool s_loggedGetLevelDesc = false;
        if (!s_loggedGetLevelDesc)
        {
            s_loggedGetLevelDesc = true;
            WWDEBUG_SAY(("[BgfxBackend] EnsureBgfxTexture: GetLevelDesc(0) failed for %s",
                         tex2d->Get_Full_Path().str()));
        }
        g_caches.texture[tex] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    // TheSuperHackers @fix bobtista 19/04/2026 Create textures WITHOUT
    // initial data so they are mutable. Passing data to createTexture2D
    // makes the texture immutable, silently rejecting later updates (font
    // atlas glyph additions, dynamic texture changes). Create empty, then
    // upload via updateTexture2D.
    // TheSuperHackers @bugfix bobtista 27/04/2026 Preserve the source
    // D3D texture mip chain. Several tiny additive effect textures, such
    // as the police-car red/blue lights, rely on their authored mipmaps
    // to average colored fringes instead of sampling only a white level-0
    // hotspot when minified.
	const unsigned mipCount = d3dTex->GetLevelCount();
	const bool terrainAtlasSafeMips = IsTerrainAtlasTexture(tex2d, desc, bgfxFmt);
    bgfx::TextureHandle h = bgfx::createTexture2D(
        static_cast<uint16_t>(desc.Width),
        static_cast<uint16_t>(desc.Height),
        mipCount > 1, 1,
        bgfxFmt,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC,
        nullptr);
    if (bgfx::isValid(h))
    {
        g_stats.textureCreates++;
        bool uploadedAllLevels = true;
        if (terrainAtlasSafeMips)
        {
            uploadedAllLevels = UploadTerrainAtlasMips(tex2d, h, d3dTex, mipCount);
        }
        else
        {
            for (unsigned mip = 0; mip < mipCount; ++mip)
            {
                const bgfx::Memory * mem = nullptr;
                uint16_t mipWidth = 0;
                uint16_t mipHeight = 0;
                if (!CopyTextureLevel(tex2d, bgfxFmt, d3dTex, mip,
                                      &mem, &mipWidth, &mipHeight))
                {
                    uploadedAllLevels = false;
                    break;
                }
                bgfx::updateTexture2D(h, 0, static_cast<uint8_t>(mip), 0, 0,
                    mipWidth, mipHeight, mem);
                g_stats.textureUploads++;
            }
        }
        if (!uploadedAllLevels)
        {
            g_caches.deferredDestroys.push_back(h);
            h = BGFX_INVALID_HANDLE;
        }
    }

    g_caches.texture[tex] = h;
    // Record dimensions so future reuse can update in place
    g_caches.d3dPtr[tex] = { curD3D, static_cast<uint16_t>(desc.Width), static_cast<uint16_t>(desc.Height) };
    return h;
}
void BgfxBackend::Invalidate_Cached_Texture(TextureBaseClass * texture)
{
    if (texture == nullptr)
    {
        return;
    }
    // Set the D3D pointer to nullptr (sentinel) so the next EnsureBgfxTexture
    // call detects a "change" and re-uploads pixel data. KEEP the dimensions
    // so the in-place update path can check if the bgfx handle is reusable.
    auto d3dIt = g_caches.d3dPtr.find(texture);
    if (d3dIt != g_caches.d3dPtr.end())
    {
        d3dIt->second.ptr = nullptr;
    }
}

void BgfxBackend::Copy_Render_Target_To_Texture(TextureClass * dst_texture,
                                                TextureClass * src_render_target)
{
    if (!g_device.initialized || dst_texture == nullptr || src_render_target == nullptr)
    {
        return;
    }

    auto srcIt = g_caches.framebuffer.find(src_render_target);
    if (srcIt == g_caches.framebuffer.end() || !bgfx::isValid(srcIt->second.colorTex))
    {
        return;
    }

    const uint16_t width = srcIt->second.width;
    const uint16_t height = srcIt->second.height;
    auto dstIt = g_caches.texture.find(dst_texture);
    bool createTexture = (dstIt == g_caches.texture.end() || !bgfx::isValid(dstIt->second));
    if (!createTexture)
    {
        auto dimIt = g_caches.d3dPtr.find(dst_texture);
        if (dimIt == g_caches.d3dPtr.end()
            || dimIt->second.w != width
            || dimIt->second.h != height)
        {
            g_caches.deferredDestroys.push_back(dstIt->second);
            g_caches.texture.erase(dstIt);
            createTexture = true;
        }
    }

    if (createTexture)
    {
        bgfx::TextureHandle h = bgfx::createTexture2D(
            width, height, false, 1,
            bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_BLIT_DST | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        g_caches.texture[dst_texture] = h;
        if (bgfx::isValid(h))
        {
            g_stats.textureCreates++;
            bgfx::setName(h, "projectedShadowCopy");
        }
    }

    bgfx::TextureHandle dstHandle = g_caches.texture[dst_texture];
    if (!bgfx::isValid(dstHandle))
    {
        return;
    }

    bgfx::blit(kBgfxRTTTextureCopyView, dstHandle, 0, 0, srcIt->second.colorTex);
    g_stats.textureCopies++;
    g_caches.d3dPtr[dst_texture] = {
        dst_texture->Peek_D3D_Base_Texture(),
        width,
        height
    };
    g_caches.renderTarget.erase(dst_texture);
}

void BgfxBackend::Release_Cached_Texture(TextureBaseClass * texture)
{
    if (texture == nullptr)
    {
        return;
    }
    // Called from TextureBaseClass::~TextureBaseClass before the D3D8
    // texture is released. Queue the bgfx handle for deferred destruction
    // (in-flight draws may still reference it this frame) and erase the
    // cache entries so a later allocation reusing this TextureBaseClass*
    // address cannot inherit the stale handle.
    auto it = g_caches.texture.find(texture);
    if (it != g_caches.texture.end())
    {
        if (bgfx::isValid(it->second))
        {
            g_caches.deferredDestroys.push_back(it->second);
        }
        g_caches.texture.erase(it);
    }
    g_caches.d3dPtr.erase(texture);
    g_caches.renderTarget.erase(texture);

    // Framebuffer-backed textures (render targets) own a framebuffer whose
    // color attachment IS the cached handle above. Destroying the
    // framebuffer also destroys its attached textures, so we do that
    // immediately and do NOT queue the colorTex for deferred destroy.
    auto fbIt = g_caches.framebuffer.find(texture);
    if (fbIt != g_caches.framebuffer.end())
    {
        if (bgfx::isValid(fbIt->second.fb))
        {
            bgfx::destroy(fbIt->second.fb);
        }
        g_caches.framebuffer.erase(fbIt);
    }
}
void BgfxBackend::Capture_Shroud_Texture(TextureClass * dst_texture,
                                          const void * pixel_data,
                                          unsigned dst_width,
                                          unsigned dst_height,
                                          unsigned src_width,
                                          unsigned src_height,
                                          unsigned dst_x,
                                          unsigned dst_y,
                                          unsigned pitch,
                                          WW3DFormat format)
{
    if (!g_device.initialized || dst_texture == nullptr || pixel_data == nullptr)
    {
        return;
    }

    const bgfx::TextureFormat::Enum bgfxFmt = TranslateWW3DFormat(format);
    if (bgfxFmt == bgfx::TextureFormat::Unknown)
    {
        return;
    }

    const unsigned bpp = (bgfxFmt == bgfx::TextureFormat::BGRA4 ||
                          bgfxFmt == bgfx::TextureFormat::R5G6B5) ? 2 : 4;

    // TheSuperHackers @bugfix bobtista 17/04/2026 Invalidate stale cache
    // entries when the shroud texture pointer changes (save/load destroys
    // and recreates m_pDstTexture). Without this, the old bgfx handle
    // stays in g_caches.texture keyed by the freed pointer, and if the
    // address is reused, updateTexture2D crashes with mismatched dimensions.
    // TheSuperHackers @performance bobtista 27/04/2026 Use the same
    // deferred bgfx texture destruction path as the general texture cache.
    // Shroud/radar textures can be replaced while prior frame submits are
    // still in flight, so immediate destroy is unsafe churn.
    static TextureClass * s_lastShroudDst = nullptr;
    static unsigned s_lastShroudW = 0;
    static unsigned s_lastShroudH = 0;
    if (dst_texture != s_lastShroudDst
        || dst_width != s_lastShroudW
        || dst_height != s_lastShroudH)
    {
        if (s_lastShroudDst != nullptr && s_lastShroudDst != dst_texture)
        {
            auto oldIt = g_caches.texture.find(s_lastShroudDst);
            if (oldIt != g_caches.texture.end())
            {
                if (bgfx::isValid(oldIt->second))
                {
                    g_caches.deferredDestroys.push_back(oldIt->second);
                }
                g_caches.texture.erase(oldIt);
            }
            g_caches.d3dPtr.erase(s_lastShroudDst);
            g_caches.renderTarget.erase(s_lastShroudDst);
        }
        // TheSuperHackers @bugfix bobtista 29/04/2026 Also invalidate the
        // new shroud destination pointer. TextureClass addresses can be reused
        // by replay/save loads after unrelated textures were cached, and the
        // old bgfx handle would then be mistaken for the shroud texture.
        auto currentIt = g_caches.texture.find(dst_texture);
        if (currentIt != g_caches.texture.end())
        {
            if (bgfx::isValid(currentIt->second))
            {
                g_caches.deferredDestroys.push_back(currentIt->second);
            }
            g_caches.texture.erase(currentIt);
        }
        g_caches.d3dPtr.erase(dst_texture);
        g_caches.renderTarget.erase(dst_texture);

        s_lastShroudDst = dst_texture;
        s_lastShroudW = dst_width;
        s_lastShroudH = dst_height;
    }

    auto it = g_caches.texture.find(dst_texture);
    if (it == g_caches.texture.end() || !bgfx::isValid(it->second))
    {
        bgfx::TextureHandle h = bgfx::createTexture2D(
            static_cast<uint16_t>(dst_width),
            static_cast<uint16_t>(dst_height),
            false, 1,
            bgfxFmt,
            BGFX_TEXTURE_NONE | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        g_caches.texture[dst_texture] = h;

        if (bgfx::isValid(h))
        {
            WWDEBUG_SAY(("[BgfxBackend] Shroud texture created: dst=%ux%u src=%ux%u "
                         "fmt=%d bgfxFmt=%d bpp=%u off=(%u,%u)",
                         dst_width, dst_height, src_width, src_height,
                         static_cast<int>(format), static_cast<int>(bgfxFmt),
                         bpp, dst_x, dst_y));
        }
        else
        {
            WWDEBUG_SAY(("[BgfxBackend] Shroud texture CREATE FAILED: dst=%ux%u fmt=%d",
                         dst_width, dst_height, static_cast<int>(format)));
        }
    }

    bgfx::TextureHandle h = g_caches.texture[dst_texture];
    if (!bgfx::isValid(h))
    {
        return;
    }

    // Build the shroud image into a persistent buffer and use makeRef
    // to avoid copying into bgfx's command buffer (which has a 64KB
    // default limit and overflows when combined with texture creation
    // bursts like the satellite reveal). The persistent buffer stays
    // valid until the next frame's update overwrites it.
    const unsigned fullSize = dst_width * dst_height * bpp;
    if (fullSize == 0 || src_width == 0 || src_height == 0
        || dst_width > 0xFFFF || dst_height > 0xFFFF)
    {
        return;
    }
    // TheSuperHackers @bugfix bobtista 27/04/2026 Keep shroud/radar updates
    // conservative. The engine's dirty rect can omit persistent explored
    // radar state after captures/buildings change ownership, so partial bgfx
    // uploads leave stale shroud on the radar. Rebuild the full texture from
    // the supplied source rect plus white fill every update, matching the
    // known-good path.
    const bgfx::Memory * mem = bgfx::alloc(fullSize);
    std::memset(mem->data, 0xFF, fullSize);
    const unsigned rowBytes = src_width * bpp;
    for (unsigned row = 0; row < src_height; ++row)
    {
        const unsigned dstOffset = ((dst_y + row) * dst_width + dst_x) * bpp;
        const unsigned srcOffset = row * pitch;
        if (dstOffset + rowBytes <= fullSize && srcOffset + rowBytes <= src_height * pitch)
        {
            std::memcpy(mem->data + dstOffset, static_cast<const uint8_t *>(pixel_data) + srcOffset, rowBytes);
        }
    }
    bgfx::updateTexture2D(h, 0, 0,
                          0, 0,
                          static_cast<uint16_t>(dst_width),
                          static_cast<uint16_t>(dst_height),
                          mem, static_cast<uint16_t>(dst_width * bpp));
}
