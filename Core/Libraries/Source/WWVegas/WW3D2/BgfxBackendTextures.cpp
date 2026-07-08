/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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
#include <cstdlib>
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

#include "texture.h"
#include "surfaceclass.h"
#include "wwdebug.h"

#include "BgfxBackend.h"
#include "BgfxBackendState.h"
#include "DXTUtils.h"

namespace
{
static const bgfx::ViewId kBgfxRTTTextureCopyView = 3;

enum BgfxTextureUploadVariant
{
    kBgfxTextureUploadNormal = 0,
    kBgfxTextureUploadDxt5Expanded = 1,
    kBgfxTextureUploadTerrainAtlas = 2,
    kBgfxTextureUploadPackedAtlas = 3,
    kBgfxTextureUploadRenderTargetCopy = 4,
    kBgfxTextureUploadBaseMipOnly = 5,
};

static int ParseEnvLimit(const char *name, int fallback)
{
    const char *value = std::getenv(name);
    if (value == nullptr || *value == '\0')
    {
        return fallback;
    }
    const int parsed = std::atoi(value);
    return parsed > 0 ? parsed : fallback;
}

static bool ShouldLogEffectTexture(TextureClass * tex2d)
{
    if (std::getenv("GGC_EFFECT_TEXTURE_DIAG") == nullptr || tex2d == nullptr)
    {
        return false;
    }

    const char *name = tex2d->Get_Full_Path().str();
    return ContainsCaseInsensitive(name, "ex")
        || ContainsCaseInsensitive(name, "fire")
        || ContainsCaseInsensitive(name, "missile");
}

static void LogEffectTextureUpload(TextureClass *tex2d,
    bgfx::TextureFormat::Enum bgfxFmt,
    const TextureBaseClass::TextureMipSnapshot &mip,
    const bgfx::Memory *mem,
    unsigned expectedPitch,
    unsigned numRows)
{
    if (!ShouldLogEffectTexture(tex2d) || mem == nullptr)
    {
        return;
    }

    unsigned minAlpha = 255;
    unsigned maxAlpha = 0;
    unsigned nonZeroAlpha = 0;
    if (bgfxFmt == bgfx::TextureFormat::BGRA8)
    {
        const unsigned pixelCount = (expectedPitch / 4) * numRows;
        for (unsigned i = 0; i < pixelCount; ++i)
        {
            const unsigned alpha = mem->data[i * 4 + 3];
            minAlpha = alpha < minAlpha ? alpha : minAlpha;
            maxAlpha = alpha > maxAlpha ? alpha : maxAlpha;
            nonZeroAlpha += alpha != 0 ? 1 : 0;
        }
    }

    if (FILE *diag = std::fopen("ggc_effect_texture_diag.txt", "a"))
    {
        std::fprintf(diag,
            "texture name=%s srcFmt=%d bgfxFmt=%d size=%ux%u pitch=%u rows=%u alphaMin=%u alphaMax=%u alphaNonZero=%u compressed=%d\n",
            tex2d->Get_Full_Path().str(),
            static_cast<int>(mip.Format),
            static_cast<int>(bgfxFmt),
            mip.Width,
            mip.Height,
            expectedPitch,
            numRows,
            minAlpha,
            maxAlpha,
            nonZeroAlpha,
            (bgfxFmt == bgfx::TextureFormat::BC1
                || bgfxFmt == bgfx::TextureFormat::BC2
                || bgfxFmt == bgfx::TextureFormat::BC3) ? 1 : 0);
        std::fclose(diag);
    }
}

static void DumpShroudTextureForDiagnostics(const uint8_t *data,
                                            unsigned width,
                                            unsigned height,
                                            unsigned bpp,
                                            WW3DFormat format)
{
    const char *dir = std::getenv("GGC_BGFX_SHROUD_DUMP_DIR");
    if (dir == nullptr || *dir == '\0' || data == nullptr || width == 0 || height == 0)
    {
        return;
    }

    static int s_dumpCount = 0;
    const int limit = ParseEnvLimit("GGC_BGFX_SHROUD_DUMP_LIMIT", 8);
    if (s_dumpCount >= limit)
    {
        return;
    }

    char path[512];
    std::snprintf(path, sizeof(path), "%s/shroud_%03d.ppm", dir, s_dumpCount++);
    FILE *file = std::fopen(path, "wb");
    if (file == nullptr)
    {
        return;
    }

    std::fprintf(file, "P6\n%u %u\n255\n", width, height);
    for (unsigned y = 0; y < height; ++y)
    {
        for (unsigned x = 0; x < width; ++x)
        {
            const uint8_t *p = data + (y * width + x) * bpp;
            uint8_t rgb[3] = { 255, 255, 255 };
            if (format == WW3D_FORMAT_R5G6B5 && bpp == 2)
            {
                const uint16_t v = static_cast<uint16_t>(p[0] | (p[1] << 8));
                rgb[0] = static_cast<uint8_t>(((v >> 11) & 0x1f) * 255 / 31);
                rgb[1] = static_cast<uint8_t>(((v >> 5) & 0x3f) * 255 / 63);
                rgb[2] = static_cast<uint8_t>((v & 0x1f) * 255 / 31);
            }
            else if (format == WW3D_FORMAT_A4R4G4B4 && bpp == 2)
            {
                const uint16_t v = static_cast<uint16_t>(p[0] | (p[1] << 8));
                rgb[0] = static_cast<uint8_t>(((v >> 8) & 0x0f) * 17);
                rgb[1] = static_cast<uint8_t>(((v >> 4) & 0x0f) * 17);
                rgb[2] = static_cast<uint8_t>((v & 0x0f) * 17);
            }
            else if (bpp == 4)
            {
                rgb[0] = p[2];
                rgb[1] = p[1];
                rgb[2] = p[0];
            }
            else if (bpp == 1)
            {
                rgb[0] = p[0];
                rgb[1] = p[0];
                rgb[2] = p[0];
            }
            std::fwrite(rgb, 1, sizeof(rgb), file);
        }
    }
    std::fclose(file);
}
}

// External linkage so BgfxBackend.cpp can reference this from its
// Create_Texture implementation.
bgfx::TextureFormat::Enum TranslateWW3DFormat(WW3DFormat fmt)
{
    switch (fmt)
    {
        case WW3D_FORMAT_A8R8G8B8:
        case WW3D_FORMAT_X8R8G8B8:
            // Legacy ARGB is stored as 0xAARRGGBB which on little-endian
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

bool BgfxBackend::Supports_Texture_Format(WW3DFormat format) const
{
    const bgfx::TextureFormat::Enum bgfxFormat = TranslateWW3DFormat(format);
    if (bgfxFormat == bgfx::TextureFormat::Unknown)
    {
        return false;
    }

    const bgfx::Caps * caps = bgfx::getCaps();
    return caps != nullptr
        && (caps->formats[bgfxFormat] & BGFX_CAPS_FORMAT_TEXTURE_2D) != 0;
}

bool BgfxBackend::Supports_Compressed_Textures() const
{
    return Supports_Texture_Format(WW3D_FORMAT_DXT1)
        || Supports_Texture_Format(WW3D_FORMAT_DXT2)
        || Supports_Texture_Format(WW3D_FORMAT_DXT3)
        || Supports_Texture_Format(WW3D_FORMAT_DXT4)
        || Supports_Texture_Format(WW3D_FORMAT_DXT5);
}

// TheSuperHackers @refactor bobtista 20/04/2026 The legacy renderer ignores the X byte of X8R8G8B8 and samples alpha as 1.0. bgfx BGRA8 samples memory literally, so FFmpeg-produced procedural frames (BGR0, alpha byte = 0) would draw transparent under SRC_ALPHA blending. Force alpha=0xFF only when the texture has no file path, so TGA-loaded X8R8G8B8 textures (scorch marks, decals) keep their real alpha data.
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
        // use exact black RGB as a transparent matte in the legacy path.
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

static bgfx::TextureFormat::Enum GetBgfxTextureUploadFormat(WW3DFormat fmt)
{
    if (fmt == WW3D_FORMAT_A4R4G4B4)
    {
        // D3D A4R4G4B4 is packed as 0xARGB. bgfx::BGRA4 is not a reliable
        // byte-for-byte substitute across renderers, so expand authored 4-bit
        // alpha textures to BGRA8 before upload. This preserves transparent
        // matte pixels used by projected decals such as spy satellite grids.
        return bgfx::TextureFormat::BGRA8;
    }
    return TranslateWW3DFormat(fmt);
}

static bgfx::TextureFormat::Enum GetBgfxTextureUploadFormat(TextureClass * tex2d)
{
    return GetBgfxTextureUploadFormat(tex2d != nullptr ? tex2d->Get_Texture_Format() : WW3D_FORMAT_UNKNOWN);
}

static void BuildDXT5AlphaTable(const uint8_t *block, uint8_t *alpha)
{
    alpha[0] = block[0];
    alpha[1] = block[1];
    if (alpha[0] > alpha[1])
    {
        for (unsigned i = 1; i < 7; ++i)
        {
            alpha[i + 1] = static_cast<uint8_t>(((7 - i) * alpha[0] + i * alpha[1] + 3) / 7);
        }
    }
    else
    {
        for (unsigned i = 1; i < 5; ++i)
        {
            alpha[i + 1] = static_cast<uint8_t>(((5 - i) * alpha[0] + i * alpha[1] + 2) / 5);
        }
        alpha[6] = 0;
        alpha[7] = 255;
    }
}

static bool DXT5UsesNonOpaqueAlpha(const TextureBaseClass::TextureMipSnapshot &mip)
{
    if (mip.Format != WW3D_FORMAT_DXT5 || mip.Data.empty() || mip.Width == 0 || mip.Height == 0)
    {
        return false;
    }

    const unsigned blockRows = (mip.Height + 3) / 4;
    const unsigned blockCols = (mip.Width + 3) / 4;
    const unsigned minPitch = blockCols * 16;
    if (mip.Pitch < minPitch || mip.Data.size() < (blockRows - 1) * mip.Pitch + minPitch)
    {
        return true;
    }

    for (unsigned by = 0; by < blockRows; ++by)
    {
        const uint8_t *row = &mip.Data[0] + by * mip.Pitch;
        for (unsigned bx = 0; bx < blockCols; ++bx)
        {
            const uint8_t *block = row + bx * 16;
            uint8_t alpha[8];
            BuildDXT5AlphaTable(block, alpha);

            uint64_t alphaBits = 0;
            for (unsigned i = 0; i < 6; ++i)
            {
                alphaBits |= static_cast<uint64_t>(block[2 + i]) << (8 * i);
            }

            for (unsigned pixel = 0; pixel < 16; ++pixel)
            {
                const unsigned alphaIndex = static_cast<unsigned>((alphaBits >> (3 * pixel)) & 0x7);
                if (alpha[alphaIndex] < 255)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool DXT1UsesTransparentIndex(const TextureBaseClass::TextureMipSnapshot &mip)
{
    if (mip.Format != WW3D_FORMAT_DXT1 || mip.Data.empty() || mip.Width == 0 || mip.Height == 0)
    {
        return false;
    }

    const unsigned blockRows = DXT_SurfaceRows(mip.Height);
    const unsigned rowPitch = DXT_SurfacePitch(mip.Width, 8);
    if (mip.Pitch < rowPitch)
    {
        return true;
    }
    const size_t requiredBytes = blockRows > 0
        ? (static_cast<size_t>(blockRows - 1) * mip.Pitch + rowPitch)
        : 0;
    if (mip.Data.size() < requiredBytes)
    {
        return true;
    }

    const unsigned blockCols = (mip.Width + 3) / 4;
    for (unsigned by = 0; by < blockRows; ++by)
    {
        const uint8_t *row = &mip.Data[0] + by * mip.Pitch;
        for (unsigned bx = 0; bx < blockCols; ++bx)
        {
            const uint8_t *block = row + bx * 8;
            const uint16_t c0 = static_cast<uint16_t>(block[0] | (block[1] << 8));
            const uint16_t c1 = static_cast<uint16_t>(block[2] | (block[3] << 8));
            if (c0 > c1)
            {
                continue;
            }

            const uint32_t bits = static_cast<uint32_t>(block[4])
                | (static_cast<uint32_t>(block[5]) << 8)
                | (static_cast<uint32_t>(block[6]) << 16)
                | (static_cast<uint32_t>(block[7]) << 24);
            for (unsigned pixel = 0; pixel < 16; ++pixel)
            {
                if (((bits >> (2 * pixel)) & 0x3) == 3)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool ShouldUseBaseMipForThinDxt1Strip(const std::vector<TextureBaseClass::TextureMipSnapshot> &mips)
{
    if (mips.size() <= 1 || mips[0].Format != WW3D_FORMAT_DXT1)
    {
        return false;
    }

    const unsigned width = mips[0].Width;
    const unsigned height = mips[0].Height;
    const unsigned major = (width > height) ? width : height;
    const unsigned minor = (width > height) ? height : width;
    static const unsigned kMinStripAspectRatio = 8;
    static const unsigned kMaxStripMinorExtent = 32;
    if (minor == 0 || major / minor < kMinStripAspectRatio || minor > kMaxStripMinorExtent)
    {
        return false;
    }

    // WW3D uses some high-aspect DXT1 mesh textures as compact strip atlases.
    // Their authored lower mips can collapse thin dark details across the strip,
    // while the base level stays stable. Keep this data-driven: only opaque DXT1
    // strips get the one-mip path, and true one-bit alpha DXT1 textures keep
    // their authored chain.
    return !DXT1UsesTransparentIndex(mips[0]);
}

static void ExpandA4R4G4B4ToBGRA8(const uint8_t * srcRow, unsigned srcPitch,
    unsigned width, unsigned height, const bgfx::Memory * mem)
{
    uint8_t * dst = mem->data;
    for (unsigned y = 0; y < height; ++y)
    {
        const uint16_t * src = reinterpret_cast<const uint16_t *>(srcRow);
        for (unsigned x = 0; x < width; ++x)
        {
            const uint16_t p = src[x];
            const uint8_t a = static_cast<uint8_t>(((p >> 12) & 0x0f) * 17);
            const uint8_t r = static_cast<uint8_t>(((p >> 8) & 0x0f) * 17);
            const uint8_t g = static_cast<uint8_t>(((p >> 4) & 0x0f) * 17);
            const uint8_t b = static_cast<uint8_t>((p & 0x0f) * 17);
            dst[0] = b;
            dst[1] = g;
            dst[2] = r;
            dst[3] = a;
            dst += 4;
        }
        srcRow += srcPitch;
    }
}

static void DecodeRgb565(uint16_t value, uint8_t *rgb)
{
    rgb[0] = static_cast<uint8_t>(((value >> 11) & 0x1F) * 255 / 31);
    rgb[1] = static_cast<uint8_t>(((value >> 5) & 0x3F) * 255 / 63);
    rgb[2] = static_cast<uint8_t>((value & 0x1F) * 255 / 31);
}

static void ExpandDXT5ToBGRA8(const uint8_t *src,
    unsigned srcPitch,
    unsigned width,
    unsigned height,
    const bgfx::Memory *mem)
{
    const unsigned blockRows = (height + 3) / 4;
    const unsigned blockCols = (width + 3) / 4;
    uint8_t *dst = mem->data;

    for (unsigned by = 0; by < blockRows; ++by)
    {
        const uint8_t *srcBlockRow = src + by * srcPitch;
        for (unsigned bx = 0; bx < blockCols; ++bx)
        {
            const uint8_t *block = srcBlockRow + bx * 16;

            uint8_t alpha[8];
            BuildDXT5AlphaTable(block, alpha);

            uint64_t alphaBits = 0;
            for (unsigned i = 0; i < 6; ++i)
            {
                alphaBits |= static_cast<uint64_t>(block[2 + i]) << (8 * i);
            }

            const uint16_t c0 = static_cast<uint16_t>(block[8] | (block[9] << 8));
            const uint16_t c1 = static_cast<uint16_t>(block[10] | (block[11] << 8));
            uint8_t color[4][3];
            DecodeRgb565(c0, color[0]);
            DecodeRgb565(c1, color[1]);
            for (unsigned c = 0; c < 3; ++c)
            {
                color[2][c] = static_cast<uint8_t>((2 * color[0][c] + color[1][c] + 1) / 3);
                color[3][c] = static_cast<uint8_t>((color[0][c] + 2 * color[1][c] + 1) / 3);
            }

            const uint32_t colorBits = static_cast<uint32_t>(block[12])
                | (static_cast<uint32_t>(block[13]) << 8)
                | (static_cast<uint32_t>(block[14]) << 16)
                | (static_cast<uint32_t>(block[15]) << 24);

            for (unsigned py = 0; py < 4; ++py)
            {
                const unsigned y = by * 4 + py;
                if (y >= height)
                {
                    continue;
                }
                for (unsigned px = 0; px < 4; ++px)
                {
                    const unsigned x = bx * 4 + px;
                    if (x >= width)
                    {
                        continue;
                    }
                    const unsigned pixel = py * 4 + px;
                    const unsigned alphaIndex = static_cast<unsigned>((alphaBits >> (3 * pixel)) & 0x7);
                    const unsigned colorIndex = static_cast<unsigned>((colorBits >> (2 * pixel)) & 0x3);
                    uint8_t *out = dst + (y * width + x) * 4;
                    out[0] = color[colorIndex][2];
                    out[1] = color[colorIndex][1];
                    out[2] = color[colorIndex][0];
                    out[3] = alpha[alphaIndex];
                }
            }
        }
    }
}

static bool IsTerrainAtlasTexture(TextureClass * tex2d,
    WW3DFormat sourceFmt,
    bgfx::TextureFormat::Enum bgfxFmt)
{
	// TheSuperHackers @bugfix bobtista 28/04/2026 The terrain texture is a
	// sparse tile atlas with black unused space between classes. The legacy
	// terrain path relies on tile-aware source mips and authored borders,
	// while bgfx creates a full mip chain whenever mips are enabled. Upload a
		// complete atlas-safe chain only for textures explicitly tagged by the
			// terrain atlas builder.
		return tex2d != nullptr
			&& sourceFmt == WW3D_FORMAT_A1R5G5B5
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
    const TextureBaseClass::TextureMipSnapshot & mip,
    unsigned level,
    bgfx::Memory const ** outMem,
    uint16_t * outWidth,
    uint16_t * outHeight)
{
    (void)level;
    if (tex2d == nullptr || outMem == nullptr
        || outWidth == nullptr || outHeight == nullptr)
    {
        return false;
    }

    if (mip.Data.empty() || mip.Width == 0 || mip.Height == 0)
    {
        return false;
    }

    const bool isCompressed = IsCompressedBgfxFormat(bgfxFmt);
    unsigned expectedPitch = 0;
    unsigned numRows = 0;
    if (isCompressed)
    {
        const unsigned blockSize = (bgfxFmt == bgfx::TextureFormat::BC1) ? 8 : 16;
        expectedPitch = DXT_SurfacePitch(mip.Width, blockSize);
        numRows = DXT_SurfaceRows(mip.Height);
    }
    else
    {
        expectedPitch = mip.Width * GetBytesPerPixel(bgfxFmt);
        numRows = mip.Height;
    }

    const unsigned totalBytes = numRows * expectedPitch;
    const unsigned srcPitch = mip.Pitch;
    const bool expandDXT5ToBGRA8 =
        mip.Format == WW3D_FORMAT_DXT5
        && bgfxFmt == bgfx::TextureFormat::BGRA8
        && !isCompressed;
    const unsigned requiredSourcePitch = expandDXT5ToBGRA8
        ? DXT_SurfacePitch(mip.Width, 16)
        : expectedPitch;
    if ((isCompressed || expandDXT5ToBGRA8) && srcPitch < requiredSourcePitch)
    {
        return false;
    }
    const unsigned requiredSourceRows =
        expandDXT5ToBGRA8 ? DXT_SurfaceRows(mip.Height) : numRows;
    const unsigned lastRowBytes = (isCompressed || expandDXT5ToBGRA8)
        ? requiredSourcePitch
        : ((srcPitch < expectedPitch) ? srcPitch : expectedPitch);
    const size_t requiredBytes = requiredSourceRows > 0
        ? (static_cast<size_t>(requiredSourceRows - 1) * srcPitch + lastRowBytes)
        : 0;
    if (mip.Data.size() < requiredBytes)
    {
        return false;
    }
    const bgfx::Memory * mem = bgfx::alloc(totalBytes);
    if (mip.Format == WW3D_FORMAT_A4R4G4B4
        && bgfxFmt == bgfx::TextureFormat::BGRA8
        && !isCompressed)
    {
        ExpandA4R4G4B4ToBGRA8(&mip.Data[0], srcPitch, mip.Width, mip.Height, mem);
    }
    else if (expandDXT5ToBGRA8)
    {
        ExpandDXT5ToBGRA8(&mip.Data[0], srcPitch, mip.Width, mip.Height, mem);
    }
    else if (srcPitch == expectedPitch)
    {
        std::memcpy(mem->data, &mip.Data[0], totalBytes);
    }
    else
    {
        const unsigned copyPitch = (srcPitch < expectedPitch) ? srcPitch : expectedPitch;
        const uint8_t * src = &mip.Data[0];
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

    if (!isCompressed)
    {
        ApplyTeamColorTextureKey(tex2d, bgfxFmt, mem, expectedPitch, numRows);
        ForceOpaqueIfProceduralX8R8G8B8(tex2d, bgfxFmt, mem, expectedPitch, numRows);
    }
    LogEffectTextureUpload(tex2d, bgfxFmt, mip, mem, expectedPitch, numRows);

    *outMem = mem;
    *outWidth = static_cast<uint16_t>(mip.Width);
    *outHeight = static_cast<uint16_t>(mip.Height);
    g_stats.textureCopies++;
    return true;
}

static bool UploadTerrainAtlasMips(TextureClass * tex2d,
	bgfx::TextureHandle h, const std::vector<TextureBaseClass::TextureMipSnapshot> & mips)
{
	std::vector<uint16_t> prev;
	unsigned prevWidth = 0;
	unsigned prevHeight = 0;
	unsigned fullMipCount = static_cast<unsigned>(mips.size());

    for (unsigned mip = 0; mip < mips.size(); ++mip)
    {
		const bgfx::Memory * mem = nullptr;
		uint16_t mipWidth = 0;
		uint16_t mipHeight = 0;
		if (!CopyTextureLevel(tex2d, bgfx::TextureFormat::BGR5A1, mips[mip], mip,
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

	for (unsigned mip = static_cast<unsigned>(mips.size()); mip < fullMipCount; ++mip)
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

// TheSuperHackers @bugfix bobtista 18/05/2026 The ubsnkatak_0 mesh atlas is a
// tightly packed 2x2 grid (scorpion | sand / metal | wood). Pre-authored DXT1
// mips average across quadrant boundaries, and hardware bilinear/aniso filtering
// at mip 0 samples across the boundary at v=0.5. This causes the wing (bottom-
// left, grey metal) to bleed red from the adjacent top-left (GLA scorpion).
// Fix: decompress BC1 to BGRA8, add a small gutter at quadrant boundaries, and
// generate per-quadrant-safe mipmaps (same principle as UploadTerrainAtlasMips).

static bool IsPackedMeshAtlasTexture(TextureClass *tex2d, WW3DFormat sourceFmt)
{
    if (tex2d == nullptr || sourceFmt != WW3D_FORMAT_DXT1)
    {
        return false;
    }
    const char *name = tex2d->Get_Full_Path().str();
    if (ContainsCaseInsensitive(name, "ubsnkatak_0"))
    {
        return true;
    }
    return false;
}

static void ExpandDXT1ToBGRA8(const uint8_t *src, unsigned srcPitch,
    unsigned width, unsigned height, uint8_t *dst)
{
    const unsigned blockRows = (height + 3) / 4;
    const unsigned blockCols = (width + 3) / 4;

    for (unsigned by = 0; by < blockRows; ++by)
    {
        const uint8_t *srcBlockRow = src + by * srcPitch;
        for (unsigned bx = 0; bx < blockCols; ++bx)
        {
            const uint8_t *block = srcBlockRow + bx * 8;

            const uint16_t c0 = static_cast<uint16_t>(block[0] | (block[1] << 8));
            const uint16_t c1 = static_cast<uint16_t>(block[2] | (block[3] << 8));
            uint8_t color[4][3];
            uint8_t alpha[4];
            DecodeRgb565(c0, color[0]);
            DecodeRgb565(c1, color[1]);

            if (c0 > c1)
            {
                for (unsigned c = 0; c < 3; ++c)
                {
                    color[2][c] = static_cast<uint8_t>((2 * color[0][c] + color[1][c] + 1) / 3);
                    color[3][c] = static_cast<uint8_t>((color[0][c] + 2 * color[1][c] + 1) / 3);
                }
                alpha[0] = alpha[1] = alpha[2] = alpha[3] = 255;
            }
            else
            {
                for (unsigned c = 0; c < 3; ++c)
                {
                    color[2][c] = static_cast<uint8_t>((color[0][c] + color[1][c]) / 2);
                    color[3][c] = 0;
                }
                alpha[0] = alpha[1] = alpha[2] = 255;
                alpha[3] = 0;
            }

            const uint32_t bits = static_cast<uint32_t>(block[4])
                | (static_cast<uint32_t>(block[5]) << 8)
                | (static_cast<uint32_t>(block[6]) << 16)
                | (static_cast<uint32_t>(block[7]) << 24);

            for (unsigned py = 0; py < 4; ++py)
            {
                const unsigned y = by * 4 + py;
                if (y >= height)
                {
                    continue;
                }
                for (unsigned px = 0; px < 4; ++px)
                {
                    const unsigned x = bx * 4 + px;
                    if (x >= width)
                    {
                        continue;
                    }
                    const unsigned idx = (bits >> (2 * (py * 4 + px))) & 0x3;
                    uint8_t *out = dst + (y * width + x) * 4;
                    out[0] = color[idx][2]; // B
                    out[1] = color[idx][1]; // G
                    out[2] = color[idx][0]; // R
                    out[3] = alpha[idx];    // A
                }
            }
        }
    }
}

static void AddPackedAtlasGutter(uint8_t *pixels, unsigned width, unsigned height,
    unsigned gutter)
{
    const unsigned halfW = width / 2;
    const unsigned halfH = height / 2;
    const unsigned bpp = 4;
    const unsigned stride = width * bpp;

    // Protect bottom quadrants from top-quadrant bleed at y = halfH:
    // copy row halfH+g into row halfH-1-g.
    for (unsigned g = 0; g < gutter && g < halfH; ++g)
    {
        std::memcpy(pixels + (halfH - 1 - g) * stride,
                     pixels + (halfH + g) * stride, stride);
    }

    // Protect top quadrants from bottom-quadrant bleed:
    // copy row halfH-1-g into row halfH+g.
    // (Must happen after the above so we read the already-overwritten copies
    //  which now hold bottom-quadrant colors — that is intentional: both sides
    //  of the seam now match.)

    // Protect right quadrants from left-quadrant bleed at x = halfW:
    // copy column halfW-1-g into column halfW+g.
    for (unsigned y = 0; y < height; ++y)
    {
        for (unsigned g = 0; g < gutter && g < halfW; ++g)
        {
            std::memcpy(pixels + (y * width + halfW + g) * bpp,
                         pixels + (y * width + halfW - 1 - g) * bpp, bpp);
        }
    }

    // Protect left quadrants from right-quadrant bleed:
    // copy column halfW+g into column halfW-1-g.
    for (unsigned y = 0; y < height; ++y)
    {
        for (unsigned g = 0; g < gutter && g < halfW; ++g)
        {
            std::memcpy(pixels + (y * width + halfW - 1 - g) * bpp,
                         pixels + (y * width + halfW + g) * bpp, bpp);
        }
    }
}

static void BuildPackedAtlasMip(const uint8_t *src, unsigned srcW, unsigned srcH,
    uint8_t *dst, unsigned dstW, unsigned dstH,
    unsigned boundaryX, unsigned boundaryY)
{
    for (unsigned y = 0; y < dstH; ++y)
    {
        const unsigned sy0 = y * 2;
        const unsigned sy1 = (sy0 + 1 < srcH) ? sy0 + 1 : sy0;
        for (unsigned x = 0; x < dstW; ++x)
        {
            const unsigned sx0 = x * 2;
            const unsigned sx1 = (sx0 + 1 < srcW) ? sx0 + 1 : sx0;

            const bool leftOfBoundary = (sx0 < boundaryX);
            const bool aboveBoundary  = (sy0 < boundaryY);

            const unsigned coords[4][2] = {
                { sx0, sy0 }, { sx1, sy0 }, { sx0, sy1 }, { sx1, sy1 }
            };

            for (unsigned c = 0; c < 4; ++c)
            {
                unsigned sum = 0;
                unsigned count = 0;
                for (unsigned s = 0; s < 4; ++s)
                {
                    const bool sameX = (leftOfBoundary == (coords[s][0] < boundaryX))
                        || (boundaryX == 0);
                    const bool sameY = (aboveBoundary == (coords[s][1] < boundaryY))
                        || (boundaryY == 0);
                    if (sameX && sameY)
                    {
                        sum += src[(coords[s][1] * srcW + coords[s][0]) * 4 + c];
                        count++;
                    }
                }
                if (count == 0)
                {
                    count = 1;
                }
                dst[(y * dstW + x) * 4 + c] = static_cast<uint8_t>(
                    (sum + count / 2) / count);
            }
        }
    }
}

static bool UploadPackedAtlasMips(TextureClass *tex2d, bgfx::TextureHandle h,
    const std::vector<TextureBaseClass::TextureMipSnapshot> &mips)
{
    if (mips.empty() || mips[0].Data.empty())
    {
        return false;
    }

    const TextureBaseClass::TextureMipSnapshot &baseMip = mips[0];
    const unsigned width = baseMip.Width;
    const unsigned height = baseMip.Height;
    const unsigned bpp = 4;
    const unsigned pixelBytes = width * height * bpp;

    const unsigned blockRows = (height + 3) / 4;
    const unsigned blockCols = (width + 3) / 4;
    const unsigned srcPitch = blockCols * 8;
    if (baseMip.Pitch < srcPitch)
    {
        return false;
    }
    const size_t requiredBytes = blockRows > 0
        ? (static_cast<size_t>(blockRows - 1) * baseMip.Pitch + srcPitch)
        : 0;
    if (baseMip.Data.size() < requiredBytes)
    {
        return false;
    }

    std::vector<uint8_t> prev(pixelBytes);
    ExpandDXT1ToBGRA8(&baseMip.Data[0], baseMip.Pitch, width, height, &prev[0]);

    static const unsigned kGutter = 16;
    AddPackedAtlasGutter(&prev[0], width, height, kGutter);

    const bgfx::Memory *mem = bgfx::copy(&prev[0], static_cast<uint32_t>(pixelBytes));
    bgfx::updateTexture2D(h, 0, 0, 0, 0,
        static_cast<uint16_t>(width), static_cast<uint16_t>(height), mem);
    g_stats.textureUploads++;

    const unsigned fullMipCount = GetFullMipCount(width, height);
    unsigned prevW = width;
    unsigned prevH = height;
    unsigned boundaryX = width / 2;
    unsigned boundaryY = height / 2;

    for (unsigned mip = 1; mip < fullMipCount; ++mip)
    {
        const unsigned nextW = (prevW > 1) ? prevW >> 1 : 1;
        const unsigned nextH = (prevH > 1) ? prevH >> 1 : 1;
        std::vector<uint8_t> next(nextW * nextH * bpp);
        BuildPackedAtlasMip(&prev[0], prevW, prevH, &next[0], nextW, nextH,
            boundaryX, boundaryY);
        const bgfx::Memory *nextMem = bgfx::copy(&next[0],
            static_cast<uint32_t>(next.size()));
        bgfx::updateTexture2D(h, 0, static_cast<uint8_t>(mip), 0, 0,
            static_cast<uint16_t>(nextW), static_cast<uint16_t>(nextH), nextMem);
        g_stats.textureUploads++;
        prev.swap(next);
        prevW = nextW;
        prevH = nextH;
        boundaryX = (boundaryX > 1) ? boundaryX >> 1 : 0;
        boundaryY = (boundaryY > 1) ? boundaryY >> 1 : 0;
    }

    return true;
}

static TextureCacheInfo MakeTextureCacheInfo(unsigned revision,
    TextureClass *tex2d,
    const TextureBaseClass::TextureMipSnapshot &baseMip,
    const std::vector<TextureBaseClass::TextureMipSnapshot> &mips,
    bgfx::TextureFormat::Enum bgfxFmt,
    bool baseMipOnly)
{
    const bool terrainAtlasSafeMips = tex2d != nullptr
        && IsTerrainAtlasTexture(tex2d, baseMip.Format, bgfxFmt);
    const bool packedMeshAtlas = tex2d != nullptr
        && IsPackedMeshAtlasTexture(tex2d, baseMip.Format);
    const bool dxt5Expanded = DXT5UsesNonOpaqueAlpha(baseMip)
        && bgfxFmt == bgfx::TextureFormat::BGRA8;
    const bgfx::TextureFormat::Enum createFmt = packedMeshAtlas && !baseMipOnly
        ? bgfx::TextureFormat::BGRA8
        : bgfxFmt;
    const unsigned createMipCount = baseMipOnly
        ? 1
        : (packedMeshAtlas
            ? GetFullMipCount(baseMip.Width, baseMip.Height)
            : static_cast<unsigned>(mips.size()));
    const unsigned uploadVariant = baseMipOnly
        ? kBgfxTextureUploadBaseMipOnly
        : (packedMeshAtlas
            ? kBgfxTextureUploadPackedAtlas
            : (terrainAtlasSafeMips
                ? kBgfxTextureUploadTerrainAtlas
                : (dxt5Expanded
                    ? kBgfxTextureUploadDxt5Expanded
                    : kBgfxTextureUploadNormal)));

    TextureCacheInfo info = {
        revision,
        static_cast<uint16_t>(baseMip.Width),
        static_cast<uint16_t>(baseMip.Height),
        static_cast<int>(baseMip.Format),
        static_cast<int>(createFmt),
        static_cast<uint16_t>(createMipCount),
        static_cast<uint16_t>(uploadVariant)
    };
    return info;
}

static bool TextureCacheInfoMatches(const TextureCacheInfo &lhs,
    const TextureCacheInfo &rhs)
{
    return lhs.revision == rhs.revision
        && lhs.w == rhs.w
        && lhs.h == rhs.h
        && lhs.sourceFormat == rhs.sourceFormat
        && lhs.createFormat == rhs.createFormat
        && lhs.mipCount == rhs.mipCount
        && lhs.uploadVariant == rhs.uploadVariant;
}

struct TextureUploadPlan
{
    bgfx::TextureFormat::Enum uploadFormat;
    bgfx::TextureFormat::Enum createFormat;
    unsigned createMipCount;
    unsigned uploadMipCount;
    bool terrainAtlasSafeMips;
    bool packedMeshAtlas;
    TextureCacheInfo cacheInfo;
};

static bool BuildTextureUploadPlan(unsigned revision,
    TextureClass *tex2d,
    const std::vector<TextureBaseClass::TextureMipSnapshot> &mips,
    bool baseMipOnly,
    TextureUploadPlan *outPlan)
{
    if (tex2d == nullptr || mips.empty() || outPlan == nullptr)
    {
        return false;
    }

    const TextureBaseClass::TextureMipSnapshot &baseMip = mips[0];
    const bool effectiveBaseMipOnly = baseMipOnly || ShouldUseBaseMipForThinDxt1Strip(mips);
    const bgfx::TextureFormat::Enum uploadFormat =
        DXT5UsesNonOpaqueAlpha(baseMip)
            ? bgfx::TextureFormat::BGRA8
            : GetBgfxTextureUploadFormat(baseMip.Format);
    if (uploadFormat == bgfx::TextureFormat::Unknown)
    {
        return false;
    }

    TextureUploadPlan plan;
    plan.uploadFormat = uploadFormat;
    plan.terrainAtlasSafeMips = !effectiveBaseMipOnly && IsTerrainAtlasTexture(tex2d, baseMip.Format, uploadFormat);
    plan.packedMeshAtlas = !effectiveBaseMipOnly && IsPackedMeshAtlasTexture(tex2d, baseMip.Format);
    plan.createFormat = plan.packedMeshAtlas
        ? bgfx::TextureFormat::BGRA8
        : uploadFormat;
    // Disabled mip filtering is represented by a separate one-mip bgfx texture
    // because bgfx sampler flags cannot disable mip sampling for a mipped texture.
    plan.createMipCount = effectiveBaseMipOnly
        ? 1
        : (plan.packedMeshAtlas
            ? GetFullMipCount(baseMip.Width, baseMip.Height)
            : static_cast<unsigned>(mips.size()));
    plan.uploadMipCount = plan.createMipCount;
    plan.cacheInfo = MakeTextureCacheInfo(revision, tex2d, baseMip, mips, uploadFormat, effectiveBaseMipOnly);
    *outPlan = plan;
    return true;
}

static bool UploadBgfxTextureMips(TextureClass *tex2d,
    bgfx::TextureHandle handle,
    const std::vector<TextureBaseClass::TextureMipSnapshot> &mips,
    const TextureUploadPlan &plan)
{
    if (plan.terrainAtlasSafeMips)
    {
        return UploadTerrainAtlasMips(tex2d, handle, mips);
    }
    if (plan.packedMeshAtlas)
    {
        return UploadPackedAtlasMips(tex2d, handle, mips);
    }

    const unsigned uploadMipCount = (plan.uploadMipCount < mips.size())
        ? plan.uploadMipCount
        : static_cast<unsigned>(mips.size());
    for (unsigned mip = 0; mip < uploadMipCount; ++mip)
    {
        const bgfx::Memory *mem = nullptr;
        uint16_t mipWidth = 0;
        uint16_t mipHeight = 0;
        if (!CopyTextureLevel(tex2d, plan.uploadFormat, mips[mip], mip,
                              &mem, &mipWidth, &mipHeight))
        {
            return false;
        }
        bgfx::updateTexture2D(handle, 0, static_cast<uint8_t>(mip), 0, 0,
            mipWidth, mipHeight, mem);
        g_stats.textureUploads++;
    }
    return true;
}

// TheSuperHackers @bugfix bobtista 02/07/2026 Mid-game textures reusing freed GPU memory
// (e.g. "_n" night textures) rendered stale on Metal via create-empty + updateTexture2D;
// create immutably with data up front. Atlas-rebuild variants keep the mutable path.
static bool TryCreateImmutableBaseMip(TextureClass *tex2d,
    const std::vector<TextureBaseClass::TextureMipSnapshot> &mips,
    const TextureUploadPlan &plan,
    uint64_t texFlags,
    bgfx::TextureHandle *outHandle)
{
    if (plan.cacheInfo.uploadVariant != kBgfxTextureUploadNormal
        && plan.cacheInfo.uploadVariant != kBgfxTextureUploadBaseMipOnly
        && plan.cacheInfo.uploadVariant != kBgfxTextureUploadDxt5Expanded)
    {
        return false;
    }

    const unsigned baseW = mips[0].Width;
    const unsigned baseH = mips[0].Height;
    unsigned levels = (plan.uploadMipCount < mips.size())
        ? plan.uploadMipCount
        : static_cast<unsigned>(mips.size());
    if (levels == 0)
    {
        return false;
    }
    // createTexture2D with mem and _hasMips=true expects the complete mip chain.
    // Only claim mips when we actually hold every level; otherwise upload the base
    // level alone (still immutable, so the Metal upload lands).
    const bool completeChain = (levels == GetFullMipCount(baseW, baseH)) && levels > 1;
    const unsigned useLevels = completeChain ? levels : 1;
    if (useLevels > 16)
    {
        return false;
    }

    const bgfx::Memory *levelMem[16] = { nullptr };
    uint16_t levelW[16] = { 0 };
    uint16_t levelH[16] = { 0 };
    uint32_t totalBytes = 0;
    for (unsigned mip = 0; mip < useLevels; ++mip)
    {
        if (!CopyTextureLevel(tex2d, plan.uploadFormat, mips[mip], mip,
                              &levelMem[mip], &levelW[mip], &levelH[mip])
            || levelMem[mip] == nullptr)
        {
            return false;
        }
        totalBytes += levelMem[mip]->size;
    }

    const bgfx::Memory *packed = bgfx::alloc(totalBytes);
    uint32_t offset = 0;
    for (unsigned mip = 0; mip < useLevels; ++mip)
    {
        std::memcpy(packed->data + offset, levelMem[mip]->data, levelMem[mip]->size);
        offset += levelMem[mip]->size;
    }

    *outHandle = bgfx::createTexture2D(static_cast<uint16_t>(baseW),
        static_cast<uint16_t>(baseH), completeChain, 1,
        plan.createFormat, texFlags, packed);
    return bgfx::isValid(*outHandle);
}

static bgfx::TextureHandle CreateBgfxTextureFromSnapshots(TextureClass *tex2d,
    const std::vector<TextureBaseClass::TextureMipSnapshot> &mips,
    const TextureUploadPlan &plan)
{
    const TextureBaseClass::TextureMipSnapshot &baseMip = mips[0];
    const uint64_t texFlags = g_device.srgbEnabled ? BGFX_TEXTURE_SRGB : BGFX_TEXTURE_NONE;

    bgfx::TextureHandle immutableHandle = BGFX_INVALID_HANDLE;
    if (TryCreateImmutableBaseMip(tex2d, mips, plan, texFlags, &immutableHandle))
    {
        g_stats.textureCreates++;
        return immutableHandle;
    }

    bgfx::TextureHandle handle = bgfx::createTexture2D(
        static_cast<uint16_t>(baseMip.Width),
        static_cast<uint16_t>(baseMip.Height),
        plan.createMipCount > 1, 1,
        plan.createFormat,
        texFlags,
        nullptr);
    if (!bgfx::isValid(handle))
    {
        return BGFX_INVALID_HANDLE;
    }

    g_stats.textureCreates++;
    if (!UploadBgfxTextureMips(tex2d, handle, mips, plan))
    {
        g_caches.deferredDestroys.push_back(handle);
        return BGFX_INVALID_HANDLE;
    }
    return handle;
}

// External linkage: called from BgfxBackend.cpp's Set_Texture path.
bgfx::TextureHandle EnsureBgfxTexture(TextureBaseClass * tex, bool baseMipOnly)
{
    if (tex == nullptr)
    {
        return BGFX_INVALID_HANDLE;
    }

    TextureClass * tex2d = tex->As_TextureClass();
    if (tex2d != nullptr &&
        tex->Get_Pool() != TextureBaseClass::POOL_DEFAULT &&
        tex->Get_CPU_Texture_Mips().empty() &&
        tex->Has_Compatibility_Texture())
    {
        WWASSERT_PRINT(
            false,
            "EnsureBgfxTexture: BGFX texture ownership missing CPU mips; no compatibility texture recapture is allowed");
    }

    const unsigned textureRevision = tex->Get_CPU_Texture_Revision();
    const std::vector<TextureBaseClass::TextureMipSnapshot> & mips = tex->Get_CPU_Texture_Mips();
    std::unordered_map<const TextureBaseClass *, bgfx::TextureHandle> &textureCache =
        baseMipOnly ? g_caches.textureBaseMip : g_caches.texture;
    std::unordered_map<const TextureBaseClass *, TextureCacheInfo> &textureInfo =
        baseMipOnly ? g_caches.textureBaseMipInfo : g_caches.textureInfo;

    auto it = textureCache.find(tex);
    if (it != textureCache.end())
    {
        auto infoIt = textureInfo.find(tex);
        bool cacheKeyMatch = infoIt != textureInfo.end()
            && infoIt->second.revision == textureRevision;
        // TheSuperHackers @performance bobtista 04/06/2026 On a revision match with a
        // fully-populated entry, the upload plan is a pure function of the
        // revision-tracked snapshot, so return the cached handle without re-deriving
        // it. The re-derivation re-ran an O(pixels) DXT5 alpha scan on every one of
        // the ~6000 texture binds per frame. The {revision,0,0} in-progress sentinel
        // (w==0) falls through to a real rebuild; content changes bump the revision
        // and atlas-region changes pair with Invalidate_Cached_Texture which clears it.
        if (cacheKeyMatch && infoIt->second.w != 0 && infoIt->second.h != 0)
        {
            return it->second;
        }
        if (cacheKeyMatch && !mips.empty())
        {
            TextureUploadPlan plan;
            cacheKeyMatch = BuildTextureUploadPlan(textureRevision, tex2d, mips, baseMipOnly, &plan)
                && TextureCacheInfoMatches(infoIt->second, plan.cacheInfo);
        }
        if (cacheKeyMatch)
        {
            return it->second;
        }

        uint16_t cachedW = 0;
        uint16_t cachedH = 0;
        if (infoIt != textureInfo.end())
        {
            cachedW = infoIt->second.w;
            cachedH = infoIt->second.h;
        }
        textureInfo[tex] = { textureRevision, 0, 0 };

        if (!mips.empty() && bgfx::isValid(it->second))
        {
            if (tex2d != nullptr)
            {
                const TextureBaseClass::TextureMipSnapshot & baseMip = mips[0];
                TextureUploadPlan plan;
                // TheSuperHackers @bugfix bobtista 02/07/2026 The in-place
                // updateTexture2D re-upload does not land on Metal for the same
                // textures the immutable-create path fixes (mid-game "_n" night
                // textures reusing freed day-texture memory), leaving the stale
                // contents. When the content-changed re-upload targets an
                // immutable-eligible texture, destroy and recreate it via the
                // immutable path instead of updating in place.
                const bool immutableEligible =
                    BuildTextureUploadPlan(textureRevision, tex2d, mips, baseMipOnly, &plan)
                    && (plan.cacheInfo.uploadVariant == kBgfxTextureUploadNormal
                        || plan.cacheInfo.uploadVariant == kBgfxTextureUploadBaseMipOnly
                        || plan.cacheInfo.uploadVariant == kBgfxTextureUploadDxt5Expanded);
                if (!immutableEligible
                    && baseMip.Width == cachedW
                    && baseMip.Height == cachedH)
                {
                    if (UploadBgfxTextureMips(tex2d, it->second, mips, plan))
                    {
                        textureInfo[tex] = plan.cacheInfo;
                        return it->second;
                    }
                }
            }
            // Immutable-eligible, dimensions, or format changed — destroy and recreate
            g_caches.deferredDestroys.push_back(it->second);
        }
        textureCache.erase(it);
    }
    else
    {
        textureInfo[tex] = { textureRevision, 0, 0 };
    }

    // Only handle regular 2D TextureClass resources here. Cube and volume
    // textures are dormant in the GeneralsMD bgfx runtime, and would need
    // separate snapshot/cache/upload plumbing if a real caller appears.
    if (tex2d == nullptr || tex->Get_Asset_Type() != TextureBaseClass::TEX_REGULAR)
    {
        textureCache[tex] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    if (tex->Is_Render_Target())
    {
        g_caches.renderTarget[tex] = true;
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

        if (tex->Is_Render_Target() || mips.empty())
        {
            g_caches.renderTarget[tex] = true;
            return BGFX_INVALID_HANDLE;
        }
    }

    if (mips.empty())
    {
        static bool s_loggedNullBase = false;
        if (!s_loggedNullBase)
        {
            s_loggedNullBase = true;
            WWDEBUG_SAY(("[BgfxBackend] EnsureBgfxTexture: no CPU texture snapshot for %s",
                         tex2d->Get_Full_Path().str()));
        }
        return BGFX_INVALID_HANDLE;
    }

    const TextureBaseClass::TextureMipSnapshot & baseMip = mips[0];
    TextureUploadPlan plan;
    if (!BuildTextureUploadPlan(textureRevision, tex2d, mips, baseMipOnly, &plan))
    {
        static bool s_loggedUnknownFmt = false;
        if (!s_loggedUnknownFmt)
        {
            s_loggedUnknownFmt = true;
            WWDEBUG_SAY(("[BgfxBackend] UNKNOWN texture format: %s ww3dfmt=%d",
                         tex2d->Get_Full_Path().str(),
                         static_cast<int>(tex2d->Get_Texture_Format())));
        }
        textureCache[tex] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    // TheSuperHackers @fix bobtista 19/04/2026 Create mutable (createTexture2D with data is
    // immutable and silently rejects later updates) and preserve the authored mip chain,
    // which tiny additive effect textures rely on when minified.
    bgfx::TextureHandle h = CreateBgfxTextureFromSnapshots(tex2d, mips, plan);

    textureCache[tex] = h;
    // Record dimensions so future reuse can update in place
    textureInfo[tex] = plan.cacheInfo;
    return h;
}
void BgfxBackend::Invalidate_Cached_Texture(TextureBaseClass * texture)
{
    // TheSuperHackers @bugfix bobtista 08/07/2026 Static render objects release
    // texture refs from destructors that run after std::exit has begun; the
    // g_caches maps this function touches may already be destroyed by then.
    // Same exit-order hazard as Destroy_Resource, guarded the same way.
    if (BgfxExitTeardownActive())
    {
        return;
    }

    if (texture == nullptr)
    {
        return;
    }
    if (!texture->Has_CPU_Texture_Mips())
    {
        WWASSERT_PRINT(
            false,
            "Invalidate_Cached_Texture: BGFX texture ownership missing CPU mips; no compatibility texture recapture is allowed");
    }
    // Set the cached revision to 0 (sentinel) so the next EnsureBgfxTexture
    // detects a change and re-uploads pixel data. Keep dimensions so the
    // in-place update path can check if the bgfx handle is reusable.
    auto infoIt = g_caches.textureInfo.find(texture);
    if (infoIt != g_caches.textureInfo.end())
    {
        infoIt->second.revision = 0;
    }
    auto baseMipInfoIt = g_caches.textureBaseMipInfo.find(texture);
    if (baseMipInfoIt != g_caches.textureBaseMipInfo.end())
    {
        baseMipInfoIt->second.revision = 0;
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
        auto dimIt = g_caches.textureInfo.find(dst_texture);
        if (dimIt == g_caches.textureInfo.end()
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
    g_caches.textureInfo[dst_texture] = {
        dst_texture->Get_CPU_Texture_Revision(),
        width,
        height
    };
    g_caches.renderTarget.erase(dst_texture);
}

void BgfxBackend::Release_Cached_Texture(TextureBaseClass * texture)
{
    // TheSuperHackers @bugfix bobtista 08/07/2026 Static render objects release
    // texture refs from destructors that run after std::exit has begun; the
    // g_caches maps this function touches may already be destroyed by then.
    // Same exit-order hazard as Destroy_Resource, guarded the same way.
    if (BgfxExitTeardownActive())
    {
        return;
    }

    if (texture == nullptr)
    {
        return;
    }
    // Called from TextureBaseClass::~TextureBaseClass before the legacy
    // texture is released. Queue the bgfx handle for deferred destruction
    // (in-flight draws may still reference it this frame) and erase the
    // cache entries so a later allocation reusing this TextureBaseClass*
    // address cannot inherit the stale handle.
    auto it = g_caches.texture.find(texture);
    bgfx::TextureHandle oldHandle = BGFX_INVALID_HANDLE;
    if (it != g_caches.texture.end())
    {
        oldHandle = it->second;
        if (bgfx::isValid(it->second))
        {
            g_caches.deferredDestroys.push_back(it->second);
        }
        g_caches.texture.erase(it);
    }
    g_caches.textureInfo.erase(texture);

    auto baseMipIt = g_caches.textureBaseMip.find(texture);
    bgfx::TextureHandle oldBaseMipHandle = BGFX_INVALID_HANDLE;
    if (baseMipIt != g_caches.textureBaseMip.end())
    {
        oldBaseMipHandle = baseMipIt->second;
        if (bgfx::isValid(baseMipIt->second))
        {
            g_caches.deferredDestroys.push_back(baseMipIt->second);
        }
        g_caches.textureBaseMip.erase(baseMipIt);
    }
    g_caches.textureBaseMipInfo.erase(texture);

    g_caches.renderTarget.erase(texture);
    for (unsigned i = 0; i < 4; ++i)
    {
        if (g_draw.sourceTextures[i] == texture
            || (bgfx::isValid(oldHandle) && g_draw.tex[i].idx == oldHandle.idx)
            || (bgfx::isValid(oldBaseMipHandle) && g_draw.tex[i].idx == oldBaseMipHandle.idx))
        {
            g_draw.tex[i] = BGFX_INVALID_HANDLE;
            g_draw.sourceTextures[i] = nullptr;
            g_draw.textureIsMissing[i] = false;
        }
    }

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
                                          unsigned src_x,
                                          unsigned src_y,
                                          unsigned dst_x,
                                          unsigned dst_y,
                                          unsigned pitch,
                                          WW3DFormat format,
                                          unsigned border_pixel)
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

    const unsigned bpp = Get_Bytes_Per_Pixel(format);
    if (bpp == 0)
    {
        return;
    }

    // TheSuperHackers @bugfix bobtista 17/04/2026 Invalidate stale cache entries when the
    // shroud texture pointer changes (save/load recreates m_pDstTexture); destroy the old
    // handle via the deferred path since prior frame submits may still be in flight.
    static TextureClass * s_lastShroudDst = nullptr;
    static unsigned s_lastShroudW = 0;
    static unsigned s_lastShroudH = 0;
    bool forceFullUpload = false;
    if (dst_texture != s_lastShroudDst
        || dst_width != s_lastShroudW
        || dst_height != s_lastShroudH)
    {
        forceFullUpload = true;
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
            g_caches.textureInfo.erase(s_lastShroudDst);
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
        g_caches.textureInfo.erase(dst_texture);
        g_caches.renderTarget.erase(dst_texture);

        s_lastShroudDst = dst_texture;
        s_lastShroudW = dst_width;
        s_lastShroudH = dst_height;
    }

    // TheSuperHackers @bugfix bobtista 03/07/2026 Refill and re-upload the
    // whole mirror when the border shroud color changes (script-driven via
    // setBorderShroudLevel), since the border ring lives only in the
    // destination image and is untouched by the per-cell dirty tracking.
    static unsigned s_lastBorderPixel = 0xFFFFu;
    if (border_pixel != s_lastBorderPixel)
    {
        forceFullUpload = true;
        s_lastBorderPixel = border_pixel;
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
        forceFullUpload = true;
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
    static std::vector<uint8_t> s_prevShroudData;
    static std::vector<uint8_t> s_fullShroudImage;
    const unsigned srcBytes = src_height * pitch;
    const unsigned rowBytes = src_width * bpp;
    const uint8_t * srcBase = static_cast<const uint8_t *>(pixel_data) + src_y * pitch;

    if (!forceFullUpload
        && s_prevShroudData.size() == srcBytes
        && std::memcmp(s_prevShroudData.data(), srcBase, srcBytes) == 0)
    {
        return;
    }

    if (s_fullShroudImage.size() != fullSize || forceFullUpload)
    {
        // TheSuperHackers @bugfix bobtista 03/07/2026 Fill the destination
        // mirror with the border shroud pixel instead of hardcoded white.
        // Terrain beyond the map boundary clamp-samples the border ring of
        // this texture; white left the map edge fully lit instead of fading
        // to the border shroud color (black by default) like DX8's
        // fillBorderShroudData does.
        s_fullShroudImage.resize(fullSize);
        if (bpp == 2)
        {
            uint16_t * fillPtr = reinterpret_cast<uint16_t *>(s_fullShroudImage.data());
            const uint16_t fillPixel = static_cast<uint16_t>(border_pixel);
            for (unsigned i = 0; i < fullSize / 2u; ++i)
            {
                fillPtr[i] = fillPixel;
            }
        }
        else
        {
            std::memset(s_fullShroudImage.data(), 0xFF, fullSize);
        }
    }

    unsigned dirtyRowMin = src_height;
    unsigned dirtyRowMax = 0;
    const bool hasPrev = s_prevShroudData.size() == srcBytes;
    for (unsigned row = 0; row < src_height; ++row)
    {
        const unsigned srcRowOff = row * pitch + src_x * bpp;
        const unsigned cacheRowOff = row * pitch;
        bool rowDirty = forceFullUpload
            || !hasPrev
            || std::memcmp(s_prevShroudData.data() + cacheRowOff,
                           srcBase + cacheRowOff, rowBytes) != 0;
        const unsigned dstOffset = ((dst_y + row) * dst_width + dst_x) * bpp;
        if (rowDirty && dstOffset + rowBytes <= fullSize)
        {
            std::memcpy(s_fullShroudImage.data() + dstOffset,
                        static_cast<const uint8_t *>(pixel_data) + src_y * pitch + srcRowOff,
                        rowBytes);
            if (row < dirtyRowMin) { dirtyRowMin = row; }
            if (row >= dirtyRowMax) { dirtyRowMax = row + 1; }
        }
    }

    s_prevShroudData.resize(srcBytes);
    std::memcpy(s_prevShroudData.data(), srcBase, srcBytes);

    SurfaceClass::SurfaceImageData shroudImage;
    shroudImage.Width = dst_width;
    shroudImage.Height = dst_height;
    shroudImage.Pitch = dst_width * bpp;
    shroudImage.Format = format;
    shroudImage.Data.assign(s_fullShroudImage.begin(), s_fullShroudImage.end());
    dst_texture->Update_Surface_Level_From_Surface(0, shroudImage);

    if (forceFullUpload)
    {
        dirtyRowMin = 0;
        dirtyRowMax = dst_height;
    }
    else if (dirtyRowMin >= dirtyRowMax)
    {
        dirtyRowMin = 0;
        dirtyRowMax = src_height;
    }
    const unsigned uploadY = forceFullUpload ? 0 : dst_y + dirtyRowMin;
    const unsigned uploadH = forceFullUpload ? dst_height : dirtyRowMax - dirtyRowMin;
    const unsigned uploadBytes = uploadH * dst_width * bpp;
    const bgfx::Memory * mem = bgfx::alloc(uploadBytes);
    for (unsigned row = 0; row < uploadH; ++row)
    {
        const unsigned imgOff = ((uploadY + row) * dst_width) * bpp;
        std::memcpy(mem->data + row * dst_width * bpp,
                    s_fullShroudImage.data() + imgOff,
                    dst_width * bpp);
    }
    bgfx::updateTexture2D(h, 0, 0,
                          0, static_cast<uint16_t>(uploadY),
                          static_cast<uint16_t>(dst_width),
                          static_cast<uint16_t>(uploadH),
                          mem, static_cast<uint16_t>(dst_width * bpp));
    g_caches.textureInfo[dst_texture] = {
        dst_texture->Get_CPU_Texture_Revision(),
        static_cast<uint16_t>(dst_width),
        static_cast<uint16_t>(dst_height),
        static_cast<int>(format),
        static_cast<int>(bgfxFmt),
        1,
        kBgfxTextureUploadNormal
    };
    DumpShroudTextureForDiagnostics(mem->data, dst_width, dst_height, bpp, format);
}
