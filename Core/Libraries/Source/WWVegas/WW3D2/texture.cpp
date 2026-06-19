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
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/texture.cpp                            $*
 *                                                                                             *
 *                  $Org Author:: Steve_t                                                     $*
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 08/05/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 85                                                          $*
 *                                                                                             *
 * 06/27/02 KM Texture class abstraction																			*
 * 08/05/02 KM Texture class redesign (revisited)
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   FileListTextureClass::Load_Frame_Surface -- Load source texture                           *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "texture.h"

#if !defined(GGC_RENDER_BACKEND_BGFX)
#include "dx8wrapper.h"
#endif
#include "TARGA.h"
#include <nstrdup.h>
#include "w3d_file.h"
#include "assetmgr.h"
#include "dx8formatconv.h"
#include "dx8texturelegacytypes.h"
#include "texturecompatibilityinterop.h"
#include "textureloader.h"
#include "bitmaphandler.h"
#include "missingtexture.h"
#include "ffactory.h"
#include "TextureResourceManager.h"
#if !defined(GGC_RENDER_BACKEND_BGFX)
#include "dx8texman.h"
#endif
#include "meshmatdesc.h"
#include "texturethumbnail.h"
#include "wwprofile.h"
#include "RenderBackend.h"
#include "IRenderBackend.h"
#include "DXTUtils.h"
#include <algorithm>
#include <cstring>
#include <utility>

#if defined(GGC_RENDER_BACKEND_BGFX)
#define DX8_ErrorCode(hr) ((void)(hr))
#define DX8_RECORD_TEXTURE(texture) ((void)(texture))
#endif

const unsigned DEFAULT_INACTIVATION_TIME=20000;

namespace
{
	constexpr unsigned kLegacyLockReadOnly = 0x00000010L;

	bool Is_Block_Compressed_Texture_Format(WW3DFormat format)
	{
		return format == WW3D_FORMAT_DXT1 ||
			format == WW3D_FORMAT_DXT2 ||
			format == WW3D_FORMAT_DXT3 ||
			format == WW3D_FORMAT_DXT4 ||
			format == WW3D_FORMAT_DXT5;
	}

	bool Should_Use_CPU_Only_Texture_Level_Surfaces()
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		return true;
#else
		return false;
#endif
	}

	bool Should_Use_CPU_Only_Surface_Textures()
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		return true;
#else
		return false;
#endif
	}

	bool Should_Block_Unmigrated_Bgfx_Texture_Type(TextureBaseClass::TexAssetType asset_type)
	{
		return Should_Use_CPU_Only_Surface_Textures() && asset_type != TextureBaseClass::TEX_REGULAR;
	}

	bool Is_Strategy_Center_Slab_Texture(const char *name)
	{
		if (name == nullptr)
		{
			return false;
		}

		const char *base = std::strrchr(name, '\\');
		const char *slash = std::strrchr(name, '/');
		if (slash != nullptr && (base == nullptr || slash > base))
		{
			base = slash;
		}
		base = (base != nullptr) ? base + 1 : name;

		char stem[64] = {};
		std::size_t i = 0;
		for (; base[i] != '\0' && base[i] != '.' && i + 1 < sizeof(stem); ++i)
		{
			stem[i] = base[i];
		}

		return stricmp(stem, "atstratslab") == 0
			|| stricmp(stem, "atstratslab_d") == 0
			|| stricmp(stem, "atstratslab_ds") == 0
			|| stricmp(stem, "atstratslab_e") == 0
			|| stricmp(stem, "atstratslab_es") == 0
			|| stricmp(stem, "atstratslab_s") == 0;
	}

	void Apply_Texture_Compatibility_Filter_Overrides(TextureClass *texture, const char *name)
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		if (texture == nullptr)
		{
			return;
		}

		if (Is_Strategy_Center_Slab_Texture(name))
		{
			// The Strategy Center's foundation side-wall UVs touch the exact 0/1
			// atlas borders. Repeating causes bgfx anisotropic taps to pull pixels
			// from the opposite atlas edge at full zoom; the original asset expects
			// edge sampling instead.
			texture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
			texture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		}
#else
		(void)texture;
		(void)name;
#endif
	}

	MipCountType Legacy_Texture_Mip_Count_For_Construct(void *legacy_texture)
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		(void)legacy_texture;
		return MIP_LEVELS_1;
#else
		return static_cast<MipCountType>(static_cast<LegacyBaseTexture *>(legacy_texture)->GetLevelCount());
#endif
	}

	unsigned Requested_Mip_Count(unsigned width, unsigned height, MipCountType mip_level_count)
	{
		if (mip_level_count == MIP_LEVELS_ALL) {
			unsigned levels = 1;
			unsigned size = std::max(width, height);
			while (size > 1) {
				size >>= 1;
				++levels;
			}
			return levels;
		}

		switch (mip_level_count) {
		case MIP_LEVELS_1: return 1;
		case MIP_LEVELS_2: return 2;
		case MIP_LEVELS_3: return 3;
		case MIP_LEVELS_4: return 4;
		case MIP_LEVELS_5: return 5;
		case MIP_LEVELS_6: return 6;
		case MIP_LEVELS_7: return 7;
		case MIP_LEVELS_8: return 8;
		case MIP_LEVELS_10: return 10;
		case MIP_LEVELS_11: return 11;
		case MIP_LEVELS_12: return 12;
		default: return 1;
		}
	}

	bool Build_CPU_Texture_Mips_From_Surface(
		const SurfaceClass::SurfaceImageData &surface_image,
		MipCountType mip_level_count,
		std::vector<TextureBaseClass::TextureMipSnapshot> &mips)
	{
		mips.clear();
		if (surface_image.Format == WW3D_FORMAT_UNKNOWN ||
			Is_Block_Compressed_Texture_Format(surface_image.Format) ||
			surface_image.Width == 0 ||
			surface_image.Height == 0 ||
			surface_image.Data.empty()) {
			return false;
		}

		const unsigned bytes_per_pixel = ::Get_Bytes_Per_Pixel(surface_image.Format);
		const unsigned base_row_size = surface_image.Width * bytes_per_pixel;
		if (bytes_per_pixel == 0 || surface_image.Pitch < base_row_size) {
			return false;
		}

		const unsigned requested_levels = Requested_Mip_Count(surface_image.Width, surface_image.Height, mip_level_count);
		mips.reserve(requested_levels);

		TextureBaseClass::TextureMipSnapshot base_mip;
		base_mip.Width = surface_image.Width;
		base_mip.Height = surface_image.Height;
		base_mip.Pitch = base_row_size;
		base_mip.Format = surface_image.Format;
		base_mip.Data.resize(static_cast<size_t>(base_row_size) * surface_image.Height);
		for (unsigned y = 0; y < surface_image.Height; ++y) {
			memcpy(
				base_mip.Data.data() + y * base_mip.Pitch,
				surface_image.Data.data() + y * surface_image.Pitch,
				base_row_size);
		}
		mips.push_back(std::move(base_mip));

		while (mips.size() < requested_levels) {
			const TextureBaseClass::TextureMipSnapshot &previous = mips.back();
			if (previous.Width == 1 && previous.Height == 1) {
				break;
			}

			TextureBaseClass::TextureMipSnapshot mip;
			mip.Width = std::max(1u, previous.Width / 2);
			mip.Height = std::max(1u, previous.Height / 2);
			mip.Pitch = mip.Width * bytes_per_pixel;
			mip.Format = previous.Format;
			mip.Data.resize(static_cast<size_t>(mip.Pitch) * mip.Height);

			for (unsigned y = 0; y < mip.Height; ++y) {
				for (unsigned x = 0; x < mip.Width; ++x) {
					const unsigned src_x = x * 2;
					const unsigned src_y = y * 2;
					const auto read_pixel = [&](unsigned px, unsigned py) {
						px = std::min(px, previous.Width - 1);
						py = std::min(py, previous.Height - 1);
						unsigned color = 0;
						BitmapHandlerClass::Read_B8G8R8A8(
							color,
							previous.Data.data() + py * previous.Pitch + px * bytes_per_pixel,
							previous.Format,
							nullptr,
							0);
						return color;
					};

					const unsigned combined = BitmapHandlerClass::Combine_A8R8G8B8(
						read_pixel(src_x, src_y),
						read_pixel(src_x + 1, src_y),
						read_pixel(src_x, src_y + 1),
						read_pixel(src_x + 1, src_y + 1));
					BitmapHandlerClass::Write_B8G8R8A8(
						mip.Data.data() + y * mip.Pitch + x * bytes_per_pixel,
						mip.Format,
						combined);
				}
			}

			mips.push_back(std::move(mip));
		}

		return !mips.empty();
	}

	bool Build_Blank_CPU_Texture_Mips(
		unsigned width,
		unsigned height,
		WW3DFormat format,
		MipCountType mip_level_count,
		std::vector<TextureBaseClass::TextureMipSnapshot> &mips)
	{
		mips.clear();
		if (width == 0 ||
			height == 0 ||
			format == WW3D_FORMAT_UNKNOWN ||
			Is_Block_Compressed_Texture_Format(format)) {
			return false;
		}

		const unsigned bytes_per_pixel = ::Get_Bytes_Per_Pixel(format);
		if (bytes_per_pixel == 0) {
			return false;
		}

		const unsigned requested_levels = Requested_Mip_Count(width, height, mip_level_count);
		mips.reserve(requested_levels);

		for (unsigned level = 0; level < requested_levels; ++level)
		{
			TextureBaseClass::TextureMipSnapshot mip;
			mip.Width = width;
			mip.Height = height;
			mip.Pitch = width * bytes_per_pixel;
			mip.Format = format;
			mip.Data.resize(static_cast<size_t>(mip.Pitch) * mip.Height);
			std::memset(mip.Data.data(), 0, mip.Data.size());
			mips.push_back(std::move(mip));

			if (width == 1 && height == 1) {
				break;
			}
			width = std::max(1u, width >> 1);
			height = std::max(1u, height >> 1);
		}

		return !mips.empty();
	}

	int Legacy_Texture_Pool(TextureBaseClass::PoolType pool)
	{
		switch (pool)
		{
		case TextureBaseClass::POOL_DEFAULT: return LEGACY_TEXTURE_POOL_DEFAULT;
		case TextureBaseClass::POOL_MANAGED: return LEGACY_TEXTURE_POOL_MANAGED;
		case TextureBaseClass::POOL_SYSTEMMEM: return LEGACY_TEXTURE_POOL_SYSTEMMEM;
		default:
			WWASSERT(0);
			return LEGACY_TEXTURE_POOL_MANAGED;
		}
	}

	LegacyBaseTexture *Legacy_Texture(void *texture)
	{
		return static_cast<LegacyBaseTexture *>(texture);
	}
}

struct TextureCompatibilityState
{
	void *NativeTexture = nullptr;
};

#define NativeCompatibilityTexture CompatibilityState->NativeTexture

/*
** Definitions of static members:
*/

static unsigned unused_texture_id;

// This throttles submissions to the background texture loading queue.
static unsigned TexturesAppliedPerFrame;
const unsigned MAX_TEXTURES_APPLIED_PER_FRAME=2;


/*!
 * KM General base constructor for texture classes
 */
TextureBaseClass::TextureBaseClass
(
	unsigned int width,
	unsigned int height,
	enum MipCountType mip_level_count,
	enum PoolType pool,
	bool rendertarget,
	bool reducible
)
	:	MipLevelCount(mip_level_count),
		CompatibilityState(new TextureCompatibilityState),
		CPUTextureRevision(0),
		m_backendHandle(kInvalidRenderResource),
	Initialized(false),
   Name(""),
	FullPath(""),
	texture_id(unused_texture_id++),
	IsLightmap(false),
	IsRenderTarget(rendertarget),
	IsProcedural(false),
	IsReducible(reducible),
	IsMissingTexture(false),
	IsCompressionAllowed(false),
	InactivationTime(0),
	ExtendedInactivationTime(0),
	LastInactivationSyncTime(0),
	LastAccessed(0),
	Width(width),
	Height(height),
	PreserveCPUTextureSnapshotOnNextLegacySet(false),
	Pool(pool),
	Dirty(false),
	TextureLoadTask(nullptr),
	ThumbnailLoadTask(nullptr),
	HSVShift(0.0f,0.0f,0.0f)
{
}


//**********************************************************************************************
//! Base texture class destructor
/*! KJM
*/
TextureBaseClass::~TextureBaseClass()
{
	TextureLoader::Delete_Texture_Load_Tasks(this);

	// TheSuperHackers @fix bobtista 20/04/2026 Notify the render backend
	// before the legacy texture goes away. bgfx caches a handle keyed on
	// this TextureBaseClass* address; if the memory is reallocated for
	// a different texture later, the old handle would be served for the
	// new object (ABA). Releasing the cache entry here closes that window.
	if (g_renderBackend != nullptr)
	{
		g_renderBackend->Release_Cached_Texture(this);
		//: also release the backend-neutral resource.
		if (m_backendHandle != kInvalidRenderResource) {
			g_renderBackend->Destroy_Resource(m_backendHandle);
			m_backendHandle = kInvalidRenderResource;
		}
	}

	if (NativeCompatibilityTexture)
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		WWASSERT_PRINT(
			false,
			"TextureBaseClass::~TextureBaseClass: standalone bgfx cannot release fake-D3D textures");
#else
		Legacy_Texture(NativeCompatibilityTexture)->Release();
#endif
		NativeCompatibilityTexture = nullptr;
	}
	Clear_CPU_Texture_Snapshot();

	TextureResourceManagerClass::Remove(this);
	delete CompatibilityState;
	CompatibilityState = nullptr;
}




//**********************************************************************************************
//! Invalidate old unused textures
/*!
*/
void TextureBaseClass::Invalidate_Old_Unused_Textures(unsigned invalidation_time_override)
{
	// (gth) If thumbnails are not enabled, then we don't run this code.
	if (WW3D::Get_Thumbnail_Enabled() == false) {
		return;
	}

	// Zero the texture apply count in this function because this is called every frame...(this wasn't in E&B main branch KJM)
	TexturesAppliedPerFrame=0;

	unsigned synctime=WW3D::Get_Sync_Time();
	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager

	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		TextureClass* tex=ite.Peek_Value();

		// Consider invalidating if texture has been initialized and defines inactivation time
		if (tex->Initialized && tex->InactivationTime)
		{
			unsigned age=synctime-tex->LastAccessed;

			if (invalidation_time_override)
			{
				if (age>invalidation_time_override)
				{
					tex->Invalidate();
					tex->LastInactivationSyncTime=synctime;
				}
			}
			else
			{
				// Not used in the last n milliseconds?
				if (age>(tex->InactivationTime+tex->ExtendedInactivationTime))
				{
					tex->Invalidate();
					tex->LastInactivationSyncTime=synctime;
				}
			}
		}
	}
}





//**********************************************************************************************
//! Invalidate this texture
/*!
*/
void TextureBaseClass::Invalidate()
{
	// TheSuperHackers @bugfix bobtista 07/06/2026 On the standalone bgfx backend the device is
	// never lost on a window/resolution change - bgfx::reset only rebuilds the swapchain and
	// preserves all user textures. The CPU mip snapshot is the only authoritative copy of the
	// pixel data (there is no D3D MANAGED system-memory backing to reload from). Tearing the
	// texture down here - Release_Cached_Texture + Destroy_Resource + Clear_CPU_Texture_Snapshot -
	// discards that only copy, and nothing reloads it, so every file texture (palm trees, rocks,
	// units) rebuilds white after a resolution change. There is no device-loss to recover from on
	// bgfx, so skip the invalidation entirely. The DX8 reference backend (Has_Shader_Pipeline()
	// false) is unaffected and keeps the original release-on-reset behavior.
	if (g_renderBackend != nullptr && g_renderBackend->Has_Shader_Pipeline()) {
		return;
	}

	if (TextureLoadTask) {
		return;
	}
	if (ThumbnailLoadTask) {
		return;
	}

	// Don't invalidate procedural textures
	if (IsProcedural) {
		return;
	}

	if (g_renderBackend != nullptr)
	{
		g_renderBackend->Release_Cached_Texture(this);
		if (m_backendHandle != kInvalidRenderResource) {
			g_renderBackend->Destroy_Resource(m_backendHandle);
			m_backendHandle = kInvalidRenderResource;
		}
	}

	if (NativeCompatibilityTexture)
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		WWASSERT_PRINT(
			false,
			"TextureBaseClass::Invalidate: standalone bgfx cannot release fake-D3D textures");
#else
		Legacy_Texture(NativeCompatibilityTexture)->Release();
#endif
		NativeCompatibilityTexture = nullptr;
	}
	Clear_CPU_Texture_Snapshot();

	Initialized=false;

	LastAccessed=WW3D::Get_Sync_Time();
/*	was battlefield version// If the texture has already been initialised we should exit now
	if (Initialized) return;

	WWPROFILE(("TextureClass::Init()"));

	// If the texture has recently been inactivated, increase the inactivation time (this texture obviously
	// should not have been inactivated yet).

	if (InactivationTime && LastInactivationSyncTime) {
		if ((WW3D::Get_Sync_Time()-LastInactivationSyncTime)<InactivationTime) {
			ExtendedInactivationTime=3*InactivationTime;
		}
		LastInactivationSyncTime=0;
	}

	if (ThumbnailLoadTask)
	{
		return;
	}

	// Don't invalidate procedural textures
	if (IsProcedural)
	{
		return;
	}

	if (NativeCompatibilityTexture)
	{
		NativeCompatibilityTexture->Release();
		NativeCompatibilityTexture = nullptr;
	}

	Initialized=false;

	LastAccessed=WW3D::Get_Sync_Time();*/
}

void TextureBaseClass::Clear_CPU_Texture_Snapshot()
{
	PreserveCPUTextureSnapshotOnNextLegacySet = false;
	if (!CPUTextureMips.empty()) {
		CPUTextureMips.clear();
	}
	++CPUTextureRevision;
}

void *TextureBaseClass::Get_Native_Compatibility_Texture() const
{
	return CompatibilityState != nullptr ? CompatibilityState->NativeTexture : nullptr;
}

void TextureBaseClass::Set_Native_Compatibility_Texture(void *native_texture)
{
	WWASSERT(CompatibilityState != nullptr);
	CompatibilityState->NativeTexture = native_texture;
}

void TextureBaseClass::Set_CPU_Texture_Snapshot(std::vector<TextureMipSnapshot> &&mips)
{
	CPUTextureMips = std::move(mips);
	PreserveCPUTextureSnapshotOnNextLegacySet = true;
	++CPUTextureRevision;
}

void TextureBaseClass::Update_CPU_Texture_Mip_Snapshot(unsigned int level, TextureMipSnapshot &&mip)
{
	if (CPUTextureMips.size() <= level) {
		CPUTextureMips.resize(level + 1);
	}
	CPUTextureMips[level] = std::move(mip);
	PreserveCPUTextureSnapshotOnNextLegacySet = true;
	++CPUTextureRevision;
}

void TextureBaseClass::Refresh_CPU_Texture_Snapshot()
{
	Capture_CPU_Texture_Snapshot(NativeCompatibilityTexture);
}

bool TextureBaseClass::Has_Compatibility_Texture() const
{
	return NativeCompatibilityTexture != nullptr;
}

void TextureBaseClass::Mark_CPU_Texture_Mips_Changed()
{
	PreserveCPUTextureSnapshotOnNextLegacySet = true;
	++CPUTextureRevision;
}

void TextureBaseClass::Share_Texture_Storage_With(const TextureBaseClass *source)
{
	Share_Legacy_Texture_With(*this, source);
}

void TextureBaseClass::Capture_CPU_Texture_Snapshot(void *native_texture)
{
	PreserveCPUTextureSnapshotOnNextLegacySet = false;
	CPUTextureMips.clear();
	++CPUTextureRevision;

#if defined(GGC_RENDER_BACKEND_BGFX)
	if (native_texture != nullptr) {
		WWASSERT_PRINT(
			false,
			"TextureBaseClass::Capture_CPU_Texture_Snapshot: standalone bgfx cannot read fake-D3D texture snapshots");
	}
	return;
#else
	TextureClass * tex2d = As_TextureClass();
	if (native_texture == nullptr || tex2d == nullptr) {
		return;
	}

	auto * d3d_texture = static_cast<decltype(Peek_Legacy_Texture2D(*tex2d))>(native_texture);
	const unsigned levels = d3d_texture->GetLevelCount();
	CPUTextureMips.reserve(levels);
	for (unsigned level = 0; level < levels; ++level) {
		LegacySurfaceDesc desc;
		if (FAILED(d3d_texture->GetLevelDesc(level, &desc))) {
			CPUTextureMips.clear();
			return;
		}

		LegacyLockedRect locked = { 0 };
		if (FAILED(d3d_texture->LockRect(level, &locked, nullptr, kLegacyLockReadOnly))
			|| locked.pBits == nullptr) {
			CPUTextureMips.clear();
			return;
		}

		TextureMipSnapshot mip;
		mip.Width = desc.Width;
		mip.Height = desc.Height;
		mip.Pitch = static_cast<unsigned>(locked.Pitch);
		mip.Format = D3DFormat_To_WW3DFormat(desc.Format);
		const bool compressed =
			mip.Format == WW3D_FORMAT_DXT1 ||
			mip.Format == WW3D_FORMAT_DXT2 ||
			mip.Format == WW3D_FORMAT_DXT3 ||
			mip.Format == WW3D_FORMAT_DXT4 ||
			mip.Format == WW3D_FORMAT_DXT5;
		const unsigned rows = compressed ? DXT_SurfaceRows(mip.Height) : mip.Height;
		mip.Data.resize(rows * mip.Pitch);
		std::memcpy(&mip.Data[0], locked.pBits, mip.Data.size());
		DX8_ErrorCode(d3d_texture->UnlockRect(level));
		CPUTextureMips.push_back(mip);
	}
#endif
}


//**********************************************************************************************
//! Load locked surface
/*!
*/
void TextureBaseClass::Load_Locked_Surface()
{
	WWPROFILE(("TextureClass::Load_Locked_Surface()"));
	if (NativeCompatibilityTexture) {
#if defined(GGC_RENDER_BACKEND_BGFX)
		WWASSERT_PRINT(
			false,
			"TextureBaseClass::Load_Locked_Surface: standalone bgfx cannot release fake-D3D textures");
#else
		Legacy_Texture(NativeCompatibilityTexture)->Release();
#endif
	}
	NativeCompatibilityTexture=nullptr;
	TextureLoader::Request_Thumbnail(this);
	Initialized=false;
}


//**********************************************************************************************
//! Is missing texture
/*!
*/
bool TextureBaseClass::Is_Missing_Texture()
{
	if (IsMissingTexture) {
		return true;
	}
	if (Should_Use_CPU_Only_Surface_Textures()) {
		return false;
	}
	if (NativeCompatibilityTexture == nullptr) {
		return false;
	}

	bool flag = false;
	LegacyBaseTexture *missing_texture = Get_Legacy_Missing_Texture();

	if (Legacy_Texture(NativeCompatibilityTexture) == missing_texture)
		flag = true;

	if (missing_texture)
	{
		missing_texture->Release();
	}

	return flag;
}


//**********************************************************************************************
//! Set texture name
/*!
*/
void TextureBaseClass::Set_Texture_Name(const char * name)
{
	Name=name;
}




//**********************************************************************************************
//! Get priority
/*!
*/
unsigned int TextureBaseClass::Get_Priority()
{
	if (!NativeCompatibilityTexture)
	{
		WWASSERT_PRINT(0, "Get_Priority: NativeCompatibilityTexture is null!");
		return 0;
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"TextureBaseClass::Get_Priority: standalone bgfx cannot query fake-D3D texture priority");
	return 0;
#else
	return Legacy_Texture(NativeCompatibilityTexture)->GetPriority();
#endif
}


//**********************************************************************************************
//! Set priority
/*!
*/
unsigned int TextureBaseClass::Set_Priority(unsigned int priority)
{
	if (!NativeCompatibilityTexture)
	{
		WWASSERT_PRINT(0, "Set_Priority: NativeCompatibilityTexture is null!");
		return 0;
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"TextureBaseClass::Set_Priority: standalone bgfx cannot set fake-D3D texture priority");
	return 0;
#else
	return Legacy_Texture(NativeCompatibilityTexture)->SetPriority(priority);
#endif
}


//**********************************************************************************************
//! Get reduction mip levels
/*!
*/
unsigned TextureBaseClass::Get_Reduction() const
{
	// don't reduce if the texture is too small already or
	// has no mip map levels
	if (MipLevelCount==MIP_LEVELS_1) return 0;
	if (Width <= 32 || Height <= 32) return 0;

	int reduction=WW3D::Get_Texture_Reduction();

	// 'large texture extra reduction' causes textures above 256x256 to be reduced one more step.
	if (WW3D::Is_Large_Texture_Extra_Reduction_Enabled() && (Width > 256 || Height > 256)) {
		reduction++;
	}
	if (MipLevelCount && reduction>MipLevelCount) {
		reduction=MipLevelCount;
	}
	return reduction;
}



//**********************************************************************************************
//! Apply null texture state
/*!
*/
void TextureBaseClass::Apply_Null(unsigned int stage)
{
	// This function sets the render states for a "null" texture
	g_renderBackend->Bind_Texture_Immediate(stage, nullptr);
}

// ----------------------------------------------------------------------------
// Setting HSV_Shift value is always relative to the original texture. This function invalidates the
// texture surface and causes the texture to be reloaded. For thumbnailable textures, the hue shifting
// is done in the background loading thread.
// ----------------------------------------------------------------------------
void TextureBaseClass::Set_HSV_Shift(const Vector3 &hsv_shift)
{
	Invalidate();
	HSVShift=hsv_shift;
}

//**********************************************************************************************
//! Get total locked surface size
/*! KM
*/
int TextureBaseClass::_Get_Total_Locked_Surface_Size()
{
	int total_locked_surface_size=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		if (!tex->Initialized)
		{
			total_locked_surface_size+=tex->Get_Texture_Memory_Usage();
		}
	}
	return total_locked_surface_size;
}

//**********************************************************************************************
//! Get total texture size
/*! KM
*/
int TextureBaseClass::_Get_Total_Texture_Size()
{
	int total_texture_size=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		total_texture_size+=tex->Get_Texture_Memory_Usage();
	}
	return total_texture_size;
}

// ----------------------------------------------------------------------------


//**********************************************************************************************
//! Get total lightmap texture size
/*!
*/
int TextureBaseClass::_Get_Total_Lightmap_Texture_Size()
{
	int total_texture_size=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		if (tex->Is_Lightmap())
		{
			total_texture_size+=tex->Get_Texture_Memory_Usage();
		}
	}
	return total_texture_size;
}


//**********************************************************************************************
//! Get total procedural texture size
/*!
*/
int TextureBaseClass::_Get_Total_Procedural_Texture_Size()
{
	int total_texture_size=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		if (tex->Is_Procedural())
		{
			total_texture_size+=tex->Get_Texture_Memory_Usage();
		}
	}
	return total_texture_size;
}

//**********************************************************************************************
//! Get total texture count
/*!
*/
int TextureBaseClass::_Get_Total_Texture_Count()
{
	int texture_count=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		texture_count++;
	}

	return texture_count;
}

// ----------------------------------------------------------------------------


//**********************************************************************************************
//! Get total light map texture count
/*!
*/
int TextureBaseClass::_Get_Total_Lightmap_Texture_Count()
{
	int texture_count=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		if (ite.Peek_Value()->Is_Lightmap())
		{
			texture_count++;
		}
	}

	return texture_count;
}

//**********************************************************************************************
//! Get total procedural texture count
/*!
*/
int TextureBaseClass::_Get_Total_Procedural_Texture_Count()
{
	int texture_count=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		if (ite.Peek_Value()->Is_Procedural())
		{
			texture_count++;
		}
	}

	return texture_count;
}


//**********************************************************************************************
//! Get total locked surface count
/*!
*/
int TextureBaseClass::_Get_Total_Locked_Surface_Count()
{
	int texture_count=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		if (!tex->Initialized)
		{
			texture_count++;
		}
	}

	return texture_count;
}

/*************************************************************************
**                             TextureClass
*************************************************************************/
TextureClass::TextureClass
(
	unsigned width,
	unsigned height,
	WW3DFormat format,
	MipCountType mip_level_count,
	PoolType pool,
	bool rendertarget,
	bool allow_reduction
)
:	TextureBaseClass(width, height, mip_level_count, pool, rendertarget,allow_reduction),
	Filter(mip_level_count),
	TextureFormat(format)
{
	Initialized=true;
	IsProcedural=true;
	IsReducible=false;

	switch (format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default : break;
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	{
		if (rendertarget)
		{
			Poke_Legacy_Texture(*this, nullptr);
			LastAccessed=WW3D::Get_Sync_Time();
			return;
		}

		std::vector<TextureMipSnapshot> mips;
		if (Build_Blank_CPU_Texture_Mips(width, height, format, mip_level_count, mips))
		{
			Set_CPU_Texture_Snapshot(std::move(mips));
			Poke_Legacy_Texture(*this, nullptr);
			LastAccessed=WW3D::Get_Sync_Time();
			return;
		}

		Initialized=false;
		Poke_Legacy_Texture(*this, nullptr);
		WWASSERT_PRINT(
			false,
			"TextureClass(width,height): BGFX texture ownership cannot create this procedural texture; no legacy fallback is allowed");
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}
#endif

	const int legacy_pool = Legacy_Texture_Pool(pool);
	Poke_Legacy_Texture(*this,
		Create_Legacy_Texture
		(
			width,
			height,
			format,
			mip_level_count,
			legacy_pool,
			rendertarget
		)
	);

#if !defined(GGC_RENDER_BACKEND_BGFX)
	if (pool==POOL_DEFAULT)
	{
		Set_Dirty();
		DX8TextureTrackerClass *track=new DX8TextureTrackerClass
		(
			width,
			height,
			format,
			mip_level_count,
			this,
			rendertarget
		);
		TextureResourceManagerClass::Add(track);
	}
#endif
	LastAccessed=WW3D::Get_Sync_Time();
}



// ----------------------------------------------------------------------------
TextureClass::TextureClass
(
	const char *name,
	const char *full_path,
	MipCountType mip_level_count,
	WW3DFormat texture_format,
	bool allow_compression,
	bool allow_reduction
)
:	TextureBaseClass(0, 0, mip_level_count),
	Filter(mip_level_count),
	TextureFormat(texture_format)
{
	IsCompressionAllowed=allow_compression;
	InactivationTime=DEFAULT_INACTIVATION_TIME;		// Default inactivation time 30 seconds
	IsReducible=allow_reduction;

	switch (TextureFormat)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	case WW3D_FORMAT_U8V8:		// Bumpmap
	case WW3D_FORMAT_L6V5U5:	// Bumpmap
	case WW3D_FORMAT_X8L8V8U8:	// Bumpmap
		// If requesting bumpmap format that isn't available we'll just return the surface in whatever color
		// format the texture file is in. (This is illegal case, the format support should always be queried
		// before creating a bump texture!)
		if (!g_renderBackend || !g_renderBackend->Supports_Texture_Format(TextureFormat))
		{
			TextureFormat=WW3D_FORMAT_UNKNOWN;
		}
		// If bump format is valid, make sure compression is not allowed so that we don't even attempt to load
		// from a compressed file (quality isn't good enough for bump map). Also disable mipmapping.
		else
		{
			IsCompressionAllowed=false;
			MipLevelCount=MIP_LEVELS_1;
			Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
		}
		break;
	default:	break;
	}

	WWASSERT_PRINT(name && name[0], "TextureClass CTor: null or empty texture name");
	int len=strlen(name);
	for (int i=0;i<len;++i)
	{
		if (name[i]=='+')
		{
			IsLightmap=true;

			// Set bilinear filtering for lightmaps (they are very stretched and
			// low detail so we don't care for anisotropic or trilinear filtering...)
			Filter.Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			Filter.Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			if (mip_level_count!=MIP_LEVELS_1) Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_FAST);
			break;
		}
	}
	Set_Texture_Name(name);
	Set_Full_Path(full_path);
	WWASSERT(name[0]!='\0');
	if (!WW3D::Is_Texturing_Enabled())
	{
		Initialized=true;
		Poke_Legacy_Texture(*this, nullptr);
	}

	// Find original size from the thumbnail (but don't create thumbnail texture yet!)
	ThumbnailClass* thumb=ThumbnailManagerClass::Peek_Thumbnail_Instance_From_Any_Manager(Get_Full_Path());
	if (thumb)
	{
		Width=thumb->Get_Original_Texture_Width();
		Height=thumb->Get_Original_Texture_Height();
 		if (MipLevelCount!=MIP_LEVELS_1) {
 			MipLevelCount=(MipCountType)thumb->Get_Original_Texture_Mip_Level_Count();
 		}
	}

	LastAccessed=WW3D::Get_Sync_Time();

	// If the thumbnails are not enabled, init the texture at this point to avoid stalling when the
	// mesh is rendered.
	if (!WW3D::Get_Thumbnail_Enabled())
	{
		if (TextureLoader::Is_Main_Render_Thread())
		{
			Init();
		}
	}
}

// ----------------------------------------------------------------------------
TextureClass::TextureClass
(
	SurfaceClass *surface,
	MipCountType mip_level_count
)
:  TextureBaseClass(0,0,mip_level_count),
	Filter(mip_level_count),
	TextureFormat(surface->Get_Surface_Format())
{
	IsProcedural=true;
	Initialized=true;
	IsReducible=false;

	SurfaceClass::SurfaceDescription sd;
	surface->Get_Description(sd);
	Width=sd.Width;
	Height=sd.Height;
	switch (sd.Format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default: break;
	}

	const SurfaceClass::SurfaceImageData *surface_image = surface->Get_CPU_Surface_Image();
	std::vector<TextureMipSnapshot> mips;
	if (surface_image != nullptr &&
		Build_CPU_Texture_Mips_From_Surface(*surface_image, mip_level_count, mips)) {
		Set_CPU_Texture_Snapshot(std::move(mips));
	}

	LegacyBaseTexture *newTexture = nullptr;
	const bool source_has_legacy_surface = surface->Get_Native_Compatibility_Surface() != nullptr;
	const bool use_cpu_owned_texture =
		Should_Use_CPU_Only_Surface_Textures() &&
		Has_CPU_Texture_Mips();
	if (source_has_legacy_surface && !use_cpu_owned_texture)
	{
		newTexture = Create_Legacy_Texture_From_Surface
		(
			Peek_Legacy_Surface(*surface),
			mip_level_count
		);
	}

	if (newTexture != nullptr || (source_has_legacy_surface && !use_cpu_owned_texture) || !Has_CPU_Texture_Mips()) {
		Poke_Legacy_Texture(*this, newTexture);
	}
	if (!Has_CPU_Texture_Mips()) {
		if (Should_Use_CPU_Only_Surface_Textures())
		{
			WWASSERT_PRINT(
				0,
				"TextureClass(SurfaceClass): BGFX texture ownership missing CPU mips; no legacy texture fallback is allowed");
		}
		else
		{
			Refresh_CPU_Texture_Snapshot();
		}
	}
	LastAccessed=WW3D::Get_Sync_Time();
}

// ----------------------------------------------------------------------------
TextureClass::TextureClass(void *legacy_texture)
:	TextureBaseClass
	(
		0,
		0,
		Legacy_Texture_Mip_Count_For_Construct(legacy_texture)
	),
	Filter(Legacy_Texture_Mip_Count_For_Construct(legacy_texture))
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	(void)legacy_texture;
	WWASSERT_PRINT(
		false,
		"TextureClass(native): standalone bgfx cannot wrap fake-D3D textures");
	Initialized=false;
	IsProcedural=true;
	IsReducible=false;
	Poke_Legacy_Texture(*this, nullptr);
	LastAccessed=WW3D::Get_Sync_Time();
	return;
#else
	LegacyBaseTexture *d3d_texture = static_cast<LegacyBaseTexture *>(legacy_texture);
	Initialized=true;
	IsProcedural=true;
	IsReducible=false;

	Set_Legacy_Base_Texture(*this, d3d_texture);
	NativeCompatibilityTextureSurface *surface;
	DX8_ErrorCode(Peek_Legacy_Texture2D(*this)->GetSurfaceLevel(0,&surface));
	LegacySurfaceDesc d3d_desc;
	::ZeroMemory(&d3d_desc, sizeof(d3d_desc));
	DX8_ErrorCode(surface->GetDesc(&d3d_desc));
	Width=d3d_desc.Width;
	Height=d3d_desc.Height;
	TextureFormat=D3DFormat_To_WW3DFormat(d3d_desc.Format);
	switch (TextureFormat)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default: break;
	}

	LastAccessed=WW3D::Get_Sync_Time();
#endif
}

//**********************************************************************************************
//! Initialise the texture
/*!
*/
void TextureClass::Init()
{
	// If the texture has already been initialised we should exit now
	if (Initialized) return;

	if (Should_Block_Unmigrated_Bgfx_Texture_Type(Get_Asset_Type()))
	{
		WWASSERT_PRINT(
			false,
			"TextureClass::Init: cube/volume textures are not migrated to bgfx texture ownership; no legacy fallback is allowed");
		return;
	}

	WWPROFILE("TextureClass::Init");

	// If the texture has recently been inactivated, increase the inactivation time (this texture obviously
	// should not have been inactivated yet).
	if (InactivationTime && LastInactivationSyncTime)
	{
		if ((WW3D::Get_Sync_Time()-LastInactivationSyncTime)<InactivationTime)
		{
			ExtendedInactivationTime=3*InactivationTime;
		}
		LastInactivationSyncTime=0;
	}


	bool has_bgfx_cpu_thumbnail = false;
#if defined(GGC_RENDER_BACKEND_BGFX)
	has_bgfx_cpu_thumbnail =
		Has_CPU_Texture_Mips();
#endif
	if (!Peek_Legacy_Base_Texture(*this) && !has_bgfx_cpu_thumbnail)
	{
		if (!WW3D::Get_Thumbnail_Enabled() || MipLevelCount==MIP_LEVELS_1)
		{
//		if (MipLevelCount==MIP_LEVELS_1) {
			TextureLoader::Request_Foreground_Loading(this);
		}
		else
		{
			WW3DFormat format=TextureFormat;
			Load_Locked_Surface();
			TextureFormat=format;
		}
	}

	if (!Initialized)
	{
		TextureLoader::Request_Background_Loading(this);
	}

	LastAccessed=WW3D::Get_Sync_Time();
}

//**********************************************************************************************
//! Apply new surface to texture
/*!
*/
void TextureClass::Apply_Native_Compatibility_Texture
(
	void *native_texture,
	bool initialized,
	bool disable_auto_invalidation
)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	(void)native_texture;
	(void)initialized;
	(void)disable_auto_invalidation;
	WWASSERT_PRINT(
		false,
		"TextureClass::Apply_Native_Compatibility_Texture: standalone bgfx cannot apply fake-D3D textures");
	return;
#else
	LegacyBaseTexture *d3d_texture = Legacy_Texture(native_texture);
	Set_Legacy_Base_Texture(*this, d3d_texture);

	if (initialized) Initialized=true;
	if (disable_auto_invalidation) InactivationTime = 0;

	WWASSERT(d3d_texture);
	NativeCompatibilityTextureSurface *surface;
	DX8_ErrorCode(Peek_Legacy_Texture2D(*this)->GetSurfaceLevel(0,&surface));
	LegacySurfaceDesc d3d_desc;
	::ZeroMemory(&d3d_desc, sizeof(d3d_desc));
	DX8_ErrorCode(surface->GetDesc(&d3d_desc));
	if (initialized)
	{
		TextureFormat=D3DFormat_To_WW3DFormat(d3d_desc.Format);
		Width=d3d_desc.Width;
		Height=d3d_desc.Height;
	}
	surface->Release();

#endif
}


//**********************************************************************************************
//! Apply texture states
/*!
*/
void TextureClass::Apply(unsigned int stage)
{
	// Initialization needs to be done when texture is used if it hasn't been done before.
	// XBOX always initializes textures at creation time.
	if (!Initialized)
	{
		Init();

		/* was in battlefield// Non-thumbnailed textures are always initialized when used
		if (MipLevelCount==MIP_LEVELS_1)
		{
		}
		// Thumbnailed textures have delayed initialization and a background loading system
		else
		{
			// Limit the number of texture initializations per frame to reduce stuttering
			if (TexturesAppliedPerFrame<MAX_TEXTURES_APPLIED_PER_FRAME)
			{
				TexturesAppliedPerFrame++;
				Init();
			}
			else
			{
				// If texture can't be initialized in this frame, at least make sure we have the thumbnail.
				if (!Peek_Texture())
				{
					WW3DFormat format=TextureFormat;
					Load_Locked_Surface();
					TextureFormat=format;
				}
			}
		}*/
	}
	LastAccessed=WW3D::Get_Sync_Time();

	DX8_RECORD_TEXTURE(this);

	// Set texture itself
	if (WW3D::Is_Texturing_Enabled())
	{
		g_renderBackend->Bind_Texture_Immediate(stage, this);
	}
	else
	{
		g_renderBackend->Bind_Texture_Immediate(stage, nullptr);
	}

	Filter.Apply(stage);
}

//**********************************************************************************************
//! Get surface from mip level
/*!
*/
SurfaceClass *TextureClass::Get_Surface_Level(unsigned int level)
{
	const std::vector<TextureMipSnapshot> &mips = Get_CPU_Texture_Mips();
	if (level < mips.size()) {
		const TextureMipSnapshot &mip = mips[level];
		if (mip.Format != WW3D_FORMAT_UNKNOWN &&
			!Is_Block_Compressed_Texture_Format(mip.Format) &&
			!mip.Data.empty() &&
			mip.Width != 0 &&
			mip.Height != 0 &&
			mip.Pitch >= mip.Width * ::Get_Bytes_Per_Pixel(mip.Format) &&
			mip.Data.size() >= static_cast<size_t>(mip.Pitch) * mip.Height)
		{
			SurfaceClass::SurfaceImageData image;
			image.Format = mip.Format;
			image.Width = mip.Width;
			image.Height = mip.Height;
			image.Pitch = mip.Pitch;
			image.Data = mip.Data;

			SurfaceClass *surface = nullptr;
			if (Should_Use_CPU_Only_Texture_Level_Surfaces())
			{
				surface = NEW_REF(SurfaceClass, (image));
			}
			else
			{
				surface = NEW_REF(SurfaceClass, (mip.Width, mip.Height, mip.Format));
				surface->Copy(mip.Data.data(), mip.Pitch);
			}
			surface->Attach_Texture_Level_Owner(this, level);
			return surface;
		}
	}

	if (!Peek_Legacy_Texture2D(*this))
	{
		WWASSERT_PRINT(0, "Get_Surface_Level: NativeCompatibilityTexture is null!");
		return nullptr;
	}
	if (Should_Use_CPU_Only_Texture_Level_Surfaces())
	{
		WWASSERT_PRINT(
			0,
			"Get_Surface_Level: BGFX CPU texture-level surface is missing; no legacy surface fallback is allowed");
		return nullptr;
	}

	NativeCompatibilityTextureSurface *d3d_surface = nullptr;
	DX8_ErrorCode(Peek_Legacy_Texture2D(*this)->GetSurfaceLevel(level, &d3d_surface));
	SurfaceClass *surface = Create_Legacy_Surface_Wrapper(d3d_surface);
	d3d_surface->Release();
	surface->Attach_Texture_Level_Owner(this, level);
	surface->Capture_CPU_Surface_Snapshot();

	return surface;
}

TextureClass::MutableTextureMipView TextureClass::Begin_Mip_Write(unsigned int level)
{
	MutableTextureMipView view;
	if (TextureFormat == WW3D_FORMAT_UNKNOWN ||
		Is_Block_Compressed_Texture_Format(TextureFormat))
	{
		return view;
	}

	const unsigned bytes_per_pixel = ::Get_Bytes_Per_Pixel(TextureFormat);
	if (bytes_per_pixel == 0)
	{
		return view;
	}

	unsigned mip_width = 0;
	unsigned mip_height = 0;
	std::vector<TextureMipSnapshot> &mips = Mutable_CPU_Texture_Mips();
	if (level < mips.size() &&
		mips[level].Format != WW3D_FORMAT_UNKNOWN &&
		mips[level].Width != 0 &&
		mips[level].Height != 0)
	{
		mip_width = mips[level].Width;
		mip_height = mips[level].Height;
	}
	else
	{
		if (Width <= 0 || Height <= 0)
		{
			return view;
		}
		mip_width = std::max(1u, static_cast<unsigned>(Width) >> level);
		mip_height = std::max(1u, static_cast<unsigned>(Height) >> level);
	}

	const unsigned row_size = mip_width * bytes_per_pixel;
	if (mips.size() <= level)
	{
		mips.resize(level + 1);
	}
	TextureMipSnapshot &mip = mips[level];
	if (mip.Format != TextureFormat ||
		mip.Width != mip_width ||
		mip.Height != mip_height ||
		mip.Pitch < row_size ||
		mip.Data.size() < static_cast<size_t>(mip.Pitch) * mip.Height)
	{
		mip.Format = TextureFormat;
		mip.Width = mip_width;
		mip.Height = mip_height;
		mip.Pitch = row_size;
		mip.Data.assign(static_cast<size_t>(row_size) * mip_height, 0);
	}

	view.Format = mip.Format;
	view.Width = mip.Width;
	view.Height = mip.Height;
	view.Pitch = mip.Pitch;
	view.Data = mip.Data.data();
	return view;
}

void TextureClass::End_Mip_Write(unsigned int level)
{
	const std::vector<TextureMipSnapshot> &mips = Get_CPU_Texture_Mips();
	if (level >= mips.size() ||
		mips[level].Format == WW3D_FORMAT_UNKNOWN ||
		mips[level].Data.empty())
	{
		return;
	}

	Mark_CPU_Texture_Mips_Changed();
	auto *texture = Peek_Legacy_Texture2D(*this);
	if (texture != nullptr)
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		WWASSERT_PRINT(
			false,
			"TextureClass::End_Mip_Write: standalone bgfx cannot mirror writes to fake-D3D texture mips");
#else
		const TextureMipSnapshot &mip = mips[level];
		const unsigned bytes_per_pixel = ::Get_Bytes_Per_Pixel(mip.Format);
		const unsigned row_size = mip.Width * bytes_per_pixel;
		LegacyLockedRect lock_rect;
		::ZeroMemory(&lock_rect, sizeof(lock_rect));
		if (bytes_per_pixel != 0 &&
			row_size != 0 &&
			SUCCEEDED(texture->LockRect(level, &lock_rect, nullptr, 0)))
		{
			if (lock_rect.pBits != nullptr)
			{
				const unsigned char *src = mip.Data.data();
				unsigned char *dst = static_cast<unsigned char *>(lock_rect.pBits);
				for (unsigned row = 0; row < mip.Height; ++row)
				{
					memcpy(dst, src, row_size);
					src += mip.Pitch;
					dst += lock_rect.Pitch;
				}
			}
			DX8_ErrorCode(texture->UnlockRect(level));
		}
#endif
	}

	if (g_renderBackend != nullptr)
	{
		g_renderBackend->Invalidate_Cached_Texture(this);
	}
}

void TextureClass::Update_Surface_Level_From_Surface(unsigned int level, const SurfaceClass::SurfaceImageData &image)
{
	if (image.Format == WW3D_FORMAT_UNKNOWN ||
		image.Width == 0 ||
		image.Height == 0 ||
		image.Data.empty() ||
		Is_Block_Compressed_Texture_Format(image.Format))
	{
		return;
	}

	const unsigned bytes_per_pixel = ::Get_Bytes_Per_Pixel(image.Format);
	if (bytes_per_pixel == 0) {
		return;
	}

	const unsigned row_size = image.Width * bytes_per_pixel;
	TextureMipSnapshot mip;
	mip.Width = image.Width;
	mip.Height = image.Height;
	mip.Pitch = row_size;
	mip.Format = image.Format;
	mip.Data.resize(static_cast<size_t>(row_size) * image.Height);
	for (unsigned row = 0; row < image.Height; ++row)
	{
		memcpy(
			mip.Data.data() + row * mip.Pitch,
			image.Data.data() + row * image.Pitch,
			row_size);
	}
	Update_CPU_Texture_Mip_Snapshot(level, std::move(mip));

	auto *texture = Peek_Legacy_Texture2D(*this);
	if (texture != nullptr)
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		WWASSERT_PRINT(
			false,
			"TextureClass::Update_Surface_Level_From_Surface: standalone bgfx cannot update fake-D3D texture mips");
#else
		LegacyLockedRect lock_rect;
		::ZeroMemory(&lock_rect, sizeof(lock_rect));
		if (SUCCEEDED(texture->LockRect(level, &lock_rect, nullptr, 0)))
		{
			if (lock_rect.pBits != nullptr)
			{
				const unsigned char *src = image.Data.data();
				unsigned char *dst = static_cast<unsigned char *>(lock_rect.pBits);
				for (unsigned row = 0; row < image.Height; ++row)
				{
					memcpy(dst, src, row_size);
					src += image.Pitch;
					dst += lock_rect.Pitch;
				}
			}
			DX8_ErrorCode(texture->UnlockRect(level));
		}
#endif
	}

	if (g_renderBackend != nullptr) {
		g_renderBackend->Invalidate_Cached_Texture(this);
	}
}

//**********************************************************************************************
//! Get surface description for a mip level
/*!
*/
void TextureClass::Get_Level_Description( SurfaceClass::SurfaceDescription & desc, unsigned int level )
{
	const std::vector<TextureMipSnapshot> &mips = Get_CPU_Texture_Mips();
	if (level < mips.size())
	{
		const TextureMipSnapshot &mip = mips[level];
		if (mip.Format != WW3D_FORMAT_UNKNOWN && mip.Width != 0 && mip.Height != 0)
		{
			desc.Format = mip.Format;
			desc.Width = mip.Width;
			desc.Height = mip.Height;
			return;
		}
	}

#if !defined(GGC_RENDER_BACKEND_BGFX)
	SurfaceClass * surf = Get_Surface_Level(level);
	if (surf != nullptr) {
		surf->Get_Description(desc);
	}
	REF_PTR_RELEASE(surf);
#else
	desc.Format = WW3D_FORMAT_UNKNOWN;
	desc.Width = 0;
	desc.Height = 0;
#endif
}

unsigned int TextureClass::Get_Level_Count() const
{
	const std::vector<TextureMipSnapshot> &mips = Get_CPU_Texture_Mips();
	if (!mips.empty()) {
		return static_cast<unsigned int>(mips.size());
	}
	auto *texture = Peek_Legacy_Texture2D(*this);
#if defined(GGC_RENDER_BACKEND_BGFX)
	if (texture != nullptr)
	{
		WWASSERT_PRINT(
			false,
			"TextureClass::Get_Level_Count: standalone bgfx cannot query fake-D3D texture levels");
	}
	return 0;
#else
	return texture != nullptr ? texture->GetLevelCount() : 0;
#endif
}

bool TextureClass::Generate_Mip_Levels()
{
	const std::vector<TextureMipSnapshot> &mips = Get_CPU_Texture_Mips();
	if (!mips.empty())
	{
		const TextureMipSnapshot &base_mip = mips[0];
		const unsigned bytes_per_pixel = ::Get_Bytes_Per_Pixel(base_mip.Format);
		if (base_mip.Format != WW3D_FORMAT_UNKNOWN &&
			!Is_Block_Compressed_Texture_Format(base_mip.Format) &&
			base_mip.Width != 0 &&
			base_mip.Height != 0 &&
			bytes_per_pixel != 0 &&
			base_mip.Pitch >= base_mip.Width * bytes_per_pixel &&
			!base_mip.Data.empty())
		{
			SurfaceClass::SurfaceImageData base_image;
			base_image.Format = base_mip.Format;
			base_image.Width = base_mip.Width;
			base_image.Height = base_mip.Height;
			base_image.Pitch = base_mip.Pitch;
			base_image.Data = base_mip.Data;

			std::vector<TextureMipSnapshot> rebuilt_mips;
			if (Build_CPU_Texture_Mips_From_Surface(base_image, MipLevelCount, rebuilt_mips))
			{
				Set_CPU_Texture_Snapshot(std::move(rebuilt_mips));
					if (Peek_Legacy_Texture2D(*this) != nullptr && !Should_Use_CPU_Only_Surface_Textures())
					{
						Generate_Legacy_Texture_Mips(*this);
					}
				if (g_renderBackend != nullptr)
				{
					g_renderBackend->Invalidate_Cached_Texture(this);
				}
				return true;
			}
		}
	}

	if (Should_Use_CPU_Only_Surface_Textures())
	{
		WWASSERT_PRINT(
			0,
			"Generate_Mip_Levels: BGFX CPU mip source is missing; no legacy mip fallback is allowed");
		return false;
	}
	return Generate_Legacy_Texture_Mips(*this);
}

void TextureClass::Set_LOD(unsigned int lod) const
{
	auto *texture = Peek_Legacy_Texture2D(*this);
	if (texture != nullptr)
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		WWASSERT_PRINT(
			false,
			"TextureClass::Set_LOD: standalone bgfx cannot set fake-D3D texture LOD");
#else
		DX8_ErrorCode(texture->SetLOD(static_cast<DWORD>(lod)));
#endif
	}
}

//**********************************************************************************************
//! Get legacy surface from mip level
/*!
*/
void *TextureClass::Get_Native_Compatibility_Surface_Level(unsigned int level)
{
	if (Should_Use_CPU_Only_Texture_Level_Surfaces())
	{
		WWASSERT_PRINT(
			0,
			"TextureClass::Get_Native_Compatibility_Surface_Level: BGFX surface ownership is enabled; no legacy surface fallback is allowed");
		return nullptr;
	}

	if (!Peek_Legacy_Texture2D(*this))
	{
		WWASSERT_PRINT(0, "Get_Native_Compatibility_Surface_Level: native texture is null!");
		return nullptr;
	}

	NativeCompatibilityTextureSurface *d3d_surface = nullptr;
	DX8_ErrorCode(Peek_Legacy_Texture2D(*this)->GetSurfaceLevel(level, &d3d_surface));
	return d3d_surface;
}

//**********************************************************************************************
//! Get texture memory usage
/*!
*/
unsigned TextureClass::Get_Texture_Memory_Usage() const
{
	const std::vector<TextureMipSnapshot> &mips = Get_CPU_Texture_Mips();
	if (!mips.empty())
	{
		size_t size = 0;
		for (const TextureMipSnapshot &mip : mips) {
			size += mip.Data.size();
		}
		return static_cast<unsigned>(size);
	}

	int size=0;
	if (!Peek_Legacy_Texture2D(*this)) return 0;
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"TextureClass::Get_Texture_Memory_Usage: standalone bgfx cannot query fake-D3D texture levels");
	return 0;
#else
	for (unsigned i=0;i<Peek_Legacy_Texture2D(*this)->GetLevelCount();++i)
	{
		LegacySurfaceDesc desc;
		DX8_ErrorCode(Peek_Legacy_Texture2D(*this)->GetLevelDesc(i,&desc));
		size+=desc.Size;
	}
	return size;
#endif
}


// Utility functions
TextureClass* Load_Texture(ChunkLoadClass & cload)
{
	// Assume failure
	TextureClass *newtex = nullptr;

	char name[256];
	if (cload.Open_Chunk () && (cload.Cur_Chunk_ID () == W3D_CHUNK_TEXTURE))
	{

		W3dTextureInfoStruct texinfo;
		bool hastexinfo = false;

		/*
		** Read in the texture filename, and a possible texture info structure.
		*/
		while (cload.Open_Chunk()) {
			switch (cload.Cur_Chunk_ID()) {
				case W3D_CHUNK_TEXTURE_NAME:
					cload.Read(&name,cload.Cur_Chunk_Length());
					break;

				case W3D_CHUNK_TEXTURE_INFO:
					cload.Read(&texinfo,sizeof(W3dTextureInfoStruct));
					hastexinfo = true;
					break;
			};
			cload.Close_Chunk();
		}
		cload.Close_Chunk();

		/*
		** Get the texture from the asset manager
		*/
		if (hastexinfo)
		{

			MipCountType mipcount;

			bool no_lod = ((texinfo.Attributes & W3DTEXTURE_NO_LOD) == W3DTEXTURE_NO_LOD);

			if (no_lod)
			{
				mipcount = MIP_LEVELS_1;
			}
			else
			{
				switch (texinfo.Attributes & W3DTEXTURE_MIP_LEVELS_MASK) {

					case W3DTEXTURE_MIP_LEVELS_ALL:
						mipcount = MIP_LEVELS_ALL;
						break;

					case W3DTEXTURE_MIP_LEVELS_2:
						mipcount = MIP_LEVELS_2;
						break;

					case W3DTEXTURE_MIP_LEVELS_3:
						mipcount = MIP_LEVELS_3;
						break;

					case W3DTEXTURE_MIP_LEVELS_4:
						mipcount = MIP_LEVELS_4;
						break;

					default:
						WWASSERT (false);
						mipcount = MIP_LEVELS_ALL;
						break;
				}
			}

			WW3DFormat format=WW3D_FORMAT_UNKNOWN;

			switch (texinfo.Attributes & W3DTEXTURE_TYPE_MASK)
			{

				case W3DTEXTURE_TYPE_COLORMAP:
					// Do nothing.
					break;

				case W3DTEXTURE_TYPE_BUMPMAP:
				{
					if (g_renderBackend && g_renderBackend->Supports_Bump_Envmap())
					{
						// No mipmaps to bumpmap for now
						mipcount=MIP_LEVELS_1;

						if (g_renderBackend->Supports_Texture_Format(WW3D_FORMAT_U8V8)) format=WW3D_FORMAT_U8V8;
						else if (g_renderBackend->Supports_Texture_Format(WW3D_FORMAT_X8L8V8U8)) format=WW3D_FORMAT_X8L8V8U8;
						else if (g_renderBackend->Supports_Texture_Format(WW3D_FORMAT_L6V5U5)) format=WW3D_FORMAT_L6V5U5;
					}
					break;
				}

				default:
					WWASSERT (false);
					break;
			}

			newtex = WW3DAssetManager::Get_Instance()->Get_Texture (name, mipcount, format);

			if (no_lod)
			{
				newtex->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
			}
			bool u_clamp = ((texinfo.Attributes & W3DTEXTURE_CLAMP_U) != 0);
			newtex->Get_Filter().Set_U_Addr_Mode(u_clamp ? TextureFilterClass::TEXTURE_ADDRESS_CLAMP : TextureFilterClass::TEXTURE_ADDRESS_REPEAT);
			bool v_clamp = ((texinfo.Attributes & W3DTEXTURE_CLAMP_V) != 0);
			newtex->Get_Filter().Set_V_Addr_Mode(v_clamp ? TextureFilterClass::TEXTURE_ADDRESS_CLAMP : TextureFilterClass::TEXTURE_ADDRESS_REPEAT);

		} else
		{
			newtex = WW3DAssetManager::Get_Instance()->Get_Texture(name);
		}

		Apply_Texture_Compatibility_Filter_Overrides(newtex, name);

		WWASSERT(newtex);
	}

	// Return a pointer to the new texture
	return newtex;
}

// Utility function used by Save_Texture
void setup_texture_attributes(TextureClass * tex, W3dTextureInfoStruct * texinfo)
{
	texinfo->Attributes = 0;

	if (tex->Get_Filter().Get_Mip_Mapping() == TextureFilterClass::FILTER_TYPE_NONE) texinfo->Attributes |= W3DTEXTURE_NO_LOD;
	if (tex->Get_Filter().Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP) texinfo->Attributes |= W3DTEXTURE_CLAMP_U;
	if (tex->Get_Filter().Get_V_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP) texinfo->Attributes |= W3DTEXTURE_CLAMP_V;
}


void Save_Texture(TextureClass * texture,ChunkSaveClass & csave)
{
	const char * filename;
	W3dTextureInfoStruct texinfo;
	memset(&texinfo,0,sizeof(texinfo));

	filename = texture->Get_Full_Path();

	setup_texture_attributes(texture, &texinfo);

	csave.Begin_Chunk(W3D_CHUNK_TEXTURE_NAME);
	csave.Write(filename,strlen(filename)+1);
	csave.End_Chunk();

	if ((texinfo.Attributes != 0) || (texinfo.AnimType != 0) || (texinfo.FrameCount != 0)) {
		csave.Begin_Chunk(W3D_CHUNK_TEXTURE_INFO);
		csave.Write(&texinfo, sizeof(texinfo));
		csave.End_Chunk();
	}
}


/*!
 *	KJM depth stencil texture constructor
 */
ZTextureClass::ZTextureClass
(
	unsigned width,
	unsigned height,
	WW3DZFormat zformat,
	MipCountType mip_level_count,
	PoolType pool
)
:	TextureBaseClass(width,height, mip_level_count, pool),
	DepthStencilTextureFormat(zformat)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	{
		Poke_Legacy_Texture(*this, nullptr);
		Initialized=true;
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}
#endif

	const int legacy_pool = Legacy_Texture_Pool(pool);

	Poke_Legacy_Texture(*this,
		Create_Legacy_ZTexture
	(
		width,
		height,
		zformat,
		mip_level_count,
		legacy_pool
	)
	);

#if !defined(GGC_RENDER_BACKEND_BGFX)
	if (pool==POOL_DEFAULT)
	{
		Set_Dirty();
		DX8ZTextureTrackerClass *track=new DX8ZTextureTrackerClass
		(
			width,
			height,
			zformat,
			mip_level_count,
			this
		);
		TextureResourceManagerClass::Add(track);
	}
#endif
	Initialized=true;
	IsProcedural=true;
	IsReducible=false;

	LastAccessed=WW3D::Get_Sync_Time();
}


//**********************************************************************************************
//! Apply depth stencil texture
/*! KM
*/
void ZTextureClass::Apply(unsigned int stage)
{
	g_renderBackend->Bind_Texture_Immediate(stage, this);
}

//**********************************************************************************************
//! Apply new surface to texture
/*! KM
*/
void ZTextureClass::Apply_Native_Compatibility_Texture
(
	void *native_texture,
	bool initialized,
	bool disable_auto_invalidation
)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	(void)native_texture;
	(void)initialized;
	(void)disable_auto_invalidation;
	WWASSERT_PRINT(
		false,
		"ZTextureClass::Apply_Native_Compatibility_Texture: standalone bgfx cannot apply fake-D3D depth textures");
	return;
#else
	LegacyBaseTexture *d3d_texture = Legacy_Texture(native_texture);
	Set_Legacy_Base_Texture(*this, d3d_texture);

	if (initialized) Initialized=true;
	if (disable_auto_invalidation) InactivationTime = 0;

	WWASSERT(Peek_Legacy_Texture2D(*this));
	NativeCompatibilityTextureSurface *surface;
	DX8_ErrorCode(Peek_Legacy_Texture2D(*this)->GetSurfaceLevel(0,&surface));
	LegacySurfaceDesc d3d_desc;
	::ZeroMemory(&d3d_desc, sizeof(d3d_desc));
	DX8_ErrorCode(surface->GetDesc(&d3d_desc));
	if (initialized)
	{
		DepthStencilTextureFormat=D3DFormat_To_WW3DZFormat(d3d_desc.Format);
		Width=d3d_desc.Width;
		Height=d3d_desc.Height;
	}
	surface->Release();
#endif
}

//**********************************************************************************************
//! Get legacy surface from mip level
/*!
*/
void *ZTextureClass::Get_Native_Compatibility_Surface_Level(unsigned int level)
{
	if (Should_Use_CPU_Only_Texture_Level_Surfaces())
	{
		WWASSERT_PRINT(
			0,
			"ZTextureClass::Get_Native_Compatibility_Surface_Level: BGFX surface ownership is enabled; no legacy depth surface fallback is allowed");
		return nullptr;
	}

	if (!Peek_Legacy_Texture2D(*this))
	{
		WWASSERT_PRINT(0, "Get_Native_Compatibility_Surface_Level: native texture is null!");
		return nullptr;
	}

	NativeCompatibilityTextureSurface *d3d_surface = nullptr;
	DX8_ErrorCode(Peek_Legacy_Texture2D(*this)->GetSurfaceLevel(level, &d3d_surface));
	return d3d_surface;
}

//**********************************************************************************************
//! Get texture memory usage
/*!
*/
unsigned ZTextureClass::Get_Texture_Memory_Usage() const
{
	int size=0;
	if (!Peek_Legacy_Texture2D(*this)) return 0;
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"ZTextureClass::Get_Texture_Memory_Usage: standalone bgfx cannot query fake-D3D depth texture levels");
	return 0;
#else
	for (unsigned i=0;i<Peek_Legacy_Texture2D(*this)->GetLevelCount();++i)
	{
		LegacySurfaceDesc desc;
		DX8_ErrorCode(Peek_Legacy_Texture2D(*this)->GetLevelDesc(i,&desc));
		size+=desc.Size;
	}
	return size;
#endif
}



/*************************************************************************
**                             CubeTextureClass
*************************************************************************/
CubeTextureClass::CubeTextureClass
(
	unsigned width,
	unsigned height,
	WW3DFormat format,
	MipCountType mip_level_count,
	PoolType pool,
	bool rendertarget,
	bool allow_reduction
)
: TextureClass(width, height, format, mip_level_count, pool, rendertarget)
{
	Initialized=true;
	IsProcedural=true;
	IsReducible=false;

	switch (format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default : break;
	}

	const int legacy_pool = Legacy_Texture_Pool(pool);

	if (Should_Block_Unmigrated_Bgfx_Texture_Type(Get_Asset_Type()))
	{
		Initialized=false;
		Poke_Legacy_Texture(*this, nullptr);
		WWASSERT_PRINT(
			false,
			"CubeTextureClass: bgfx texture ownership has no cube texture implementation; no legacy fallback is allowed");
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}

	Poke_Legacy_Texture(*this,
		Create_Legacy_Cube_Texture
		(
			width,
			height,
			format,
			mip_level_count,
			legacy_pool,
			rendertarget
		)
	);

#if !defined(GGC_RENDER_BACKEND_BGFX)
	if (pool==POOL_DEFAULT)
	{
		Set_Dirty();
		DX8TextureTrackerClass *track=new DX8TextureTrackerClass
		(
			width,
			height,
			format,
			mip_level_count,
			this,
			rendertarget
		);
		TextureResourceManagerClass::Add(track);
	}
#endif
	LastAccessed=WW3D::Get_Sync_Time();
}



// ----------------------------------------------------------------------------
CubeTextureClass::CubeTextureClass
(
	const char *name,
	const char *full_path,
	MipCountType mip_level_count,
	WW3DFormat texture_format,
	bool allow_compression,
	bool allow_reduction
)
:	TextureClass(0,0,mip_level_count, POOL_MANAGED, false, texture_format)
{
	IsCompressionAllowed=allow_compression;
	InactivationTime=DEFAULT_INACTIVATION_TIME;		// Default inactivation time 30 seconds

	switch (TextureFormat)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	case WW3D_FORMAT_U8V8:		// Bumpmap
	case WW3D_FORMAT_L6V5U5:	// Bumpmap
	case WW3D_FORMAT_X8L8V8U8:	// Bumpmap
		// If requesting bumpmap format that isn't available we'll just return the surface in whatever color
		// format the texture file is in. (This is illegal case, the format support should always be queried
		// before creating a bump texture!)
		if (!g_renderBackend || !g_renderBackend->Supports_Texture_Format(TextureFormat))
		{
			TextureFormat=WW3D_FORMAT_UNKNOWN;
		}
		// If bump format is valid, make sure compression is not allowed so that we don't even attempt to load
		// from a compressed file (quality isn't good enough for bump map). Also disable mipmapping.
		else
		{
			IsCompressionAllowed=false;
			MipLevelCount=MIP_LEVELS_1;
			Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
		}
		break;
	default:	break;
	}

	WWASSERT_PRINT(name && name[0], "TextureClass CTor: null or empty texture name");
	int len=strlen(name);
	for (int i=0;i<len;++i)
	{
		if (name[i]=='+')
		{
			IsLightmap=true;

			// Set bilinear filtering for lightmaps (they are very stretched and
			// low detail so we don't care for anisotropic or trilinear filtering...)
			Filter.Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			Filter.Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			if (mip_level_count!=MIP_LEVELS_1) Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_FAST);
			break;
		}
	}
	Set_Texture_Name(name);
	Set_Full_Path(full_path);
	WWASSERT(name[0]!='\0');

	if (Should_Block_Unmigrated_Bgfx_Texture_Type(Get_Asset_Type()))
	{
		Initialized=false;
		Poke_Legacy_Texture(*this, nullptr);
		WWASSERT_PRINT(
			false,
			"CubeTextureClass: bgfx texture ownership has no cube texture implementation; no legacy fallback is allowed");
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}

	if (!WW3D::Is_Texturing_Enabled())
	{
		Initialized=true;
		Poke_Legacy_Texture(*this, nullptr);
	}

	// Find original size from the thumbnail (but don't create thumbnail texture yet!)
	ThumbnailClass* thumb=ThumbnailManagerClass::Peek_Thumbnail_Instance_From_Any_Manager(Get_Full_Path());
	if (thumb)
	{
		Width=thumb->Get_Original_Texture_Width();
		Height=thumb->Get_Original_Texture_Height();
 		if (MipLevelCount!=MIP_LEVELS_1) {
 			MipLevelCount=(MipCountType)thumb->Get_Original_Texture_Mip_Level_Count();
 		}
	}

	LastAccessed=WW3D::Get_Sync_Time();

	// If the thumbnails are not enabled, init the texture at this point to avoid stalling when the
	// mesh is rendered.
	if (!WW3D::Get_Thumbnail_Enabled())
	{
		if (TextureLoader::Is_Main_Render_Thread())
		{
			Init();
		}
	}
}

//**********************************************************************************************
//! Apply new surface to texture
/*!
*/
void CubeTextureClass::Apply_Native_Compatibility_Texture
(
	void *native_texture,
	bool initialized,
	bool disable_auto_invalidation
)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	(void)native_texture;
	(void)initialized;
	(void)disable_auto_invalidation;
	WWASSERT_PRINT(
		false,
		"CubeTextureClass::Apply_Native_Compatibility_Texture: standalone bgfx cannot apply fake-D3D cube textures");
	return;
#else
	LegacyBaseTexture *d3d_texture = Legacy_Texture(native_texture);
	Set_Legacy_Base_Texture(*this, d3d_texture);

	if (initialized) Initialized=true;
	if (disable_auto_invalidation) InactivationTime = 0;

	WWASSERT(d3d_texture);
	LegacySurfaceDesc d3d_desc;
	::ZeroMemory(&d3d_desc, sizeof(d3d_desc));
	DX8_ErrorCode(Peek_Legacy_Cube_Texture(*this)->GetLevelDesc(0,&d3d_desc));

	if (initialized)
	{
		TextureFormat=D3DFormat_To_WW3DFormat(d3d_desc.Format);
		Width=d3d_desc.Width;
		Height=d3d_desc.Height;
	}
#endif
}


/*************************************************************************
**                             VolumeTextureClass
*************************************************************************/
VolumeTextureClass::VolumeTextureClass
(
	unsigned width,
	unsigned height,
	unsigned depth,
	WW3DFormat format,
	MipCountType mip_level_count,
	PoolType pool,
	bool rendertarget,
	bool allow_reduction
)
: TextureClass(width, height, format, mip_level_count, pool, rendertarget),
  Depth(depth)
{
	Initialized=true;
	IsProcedural=true;
	IsReducible=false;

	switch (format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default : break;
	}

	const int legacy_pool = Legacy_Texture_Pool(pool);

	if (Should_Block_Unmigrated_Bgfx_Texture_Type(Get_Asset_Type()))
	{
		Initialized=false;
		Poke_Legacy_Texture(*this, nullptr);
		WWASSERT_PRINT(
			false,
			"VolumeTextureClass: bgfx texture ownership has no volume texture implementation; no legacy fallback is allowed");
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}

	Poke_Legacy_Texture(*this,
		Create_Legacy_Volume_Texture
		(
			width,
			height,
			depth,
			format,
			mip_level_count,
			legacy_pool
		)
	);

#if !defined(GGC_RENDER_BACKEND_BGFX)
	if (pool==POOL_DEFAULT)
	{
		Set_Dirty();
		DX8TextureTrackerClass *track=new DX8TextureTrackerClass
		(
			width,
			height,
			format,
			mip_level_count,
			this,
			rendertarget
		);
		TextureResourceManagerClass::Add(track);
	}
#endif
	LastAccessed=WW3D::Get_Sync_Time();
}



// ----------------------------------------------------------------------------
VolumeTextureClass::VolumeTextureClass
(
	const char *name,
	const char *full_path,
	MipCountType mip_level_count,
	WW3DFormat texture_format,
	bool allow_compression,
	bool allow_reduction
)
:	TextureClass(0,0,mip_level_count, POOL_MANAGED, false, texture_format),
	Depth(0)
{
	IsCompressionAllowed=allow_compression;
	InactivationTime=DEFAULT_INACTIVATION_TIME;		// Default inactivation time 30 seconds

	switch (TextureFormat)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	case WW3D_FORMAT_U8V8:		// Bumpmap
	case WW3D_FORMAT_L6V5U5:	// Bumpmap
	case WW3D_FORMAT_X8L8V8U8:	// Bumpmap
		// If requesting bumpmap format that isn't available we'll just return the surface in whatever color
		// format the texture file is in. (This is illegal case, the format support should always be queried
		// before creating a bump texture!)
		if (!g_renderBackend || !g_renderBackend->Supports_Texture_Format(TextureFormat))
		{
			TextureFormat=WW3D_FORMAT_UNKNOWN;
		}
		// If bump format is valid, make sure compression is not allowed so that we don't even attempt to load
		// from a compressed file (quality isn't good enough for bump map). Also disable mipmapping.
		else
		{
			IsCompressionAllowed=false;
			MipLevelCount=MIP_LEVELS_1;
			Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
		}
		break;
	default:	break;
	}

	WWASSERT_PRINT(name && name[0], "TextureClass CTor: null or empty texture name");
	int len=strlen(name);
	for (int i=0;i<len;++i)
	{
		if (name[i]=='+')
		{
			IsLightmap=true;

			// Set bilinear filtering for lightmaps (they are very stretched and
			// low detail so we don't care for anisotropic or trilinear filtering...)
			Filter.Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			Filter.Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			if (mip_level_count!=MIP_LEVELS_1) Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_FAST);
			break;
		}
	}
	Set_Texture_Name(name);
	Set_Full_Path(full_path);
	WWASSERT(name[0]!='\0');

	if (Should_Block_Unmigrated_Bgfx_Texture_Type(Get_Asset_Type()))
	{
		Initialized=false;
		Poke_Legacy_Texture(*this, nullptr);
		WWASSERT_PRINT(
			false,
			"VolumeTextureClass: bgfx texture ownership has no volume texture implementation; no legacy fallback is allowed");
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}

	if (!WW3D::Is_Texturing_Enabled())
	{
		Initialized=true;
		Poke_Legacy_Texture(*this, nullptr);
	}

	// Find original size from the thumbnail (but don't create thumbnail texture yet!)
	ThumbnailClass* thumb=ThumbnailManagerClass::Peek_Thumbnail_Instance_From_Any_Manager(Get_Full_Path());
	if (thumb)
	{
		Width=thumb->Get_Original_Texture_Width();
		Height=thumb->Get_Original_Texture_Height();
 		if (MipLevelCount!=MIP_LEVELS_1) {
 			MipLevelCount=(MipCountType)thumb->Get_Original_Texture_Mip_Level_Count();
 		}
	}

	LastAccessed=WW3D::Get_Sync_Time();

	// If the thumbnails are not enabled, init the texture at this point to avoid stalling when the
	// mesh is rendered.
	if (!WW3D::Get_Thumbnail_Enabled())
	{
		if (TextureLoader::Is_Main_Render_Thread())
		{
			Init();
		}
	}
}

//**********************************************************************************************
//! Apply new surface to texture
/*!
*/
void VolumeTextureClass::Apply_Native_Compatibility_Texture
(
	void *native_texture,
	bool initialized,
	bool disable_auto_invalidation
)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	(void)native_texture;
	(void)initialized;
	(void)disable_auto_invalidation;
	WWASSERT_PRINT(
		false,
		"VolumeTextureClass::Apply_Native_Compatibility_Texture: standalone bgfx cannot apply fake-D3D volume textures");
	return;
#else
	LegacyBaseTexture *d3d_texture = Legacy_Texture(native_texture);
	Set_Legacy_Base_Texture(*this, d3d_texture);

	if (initialized) Initialized=true;
	if (disable_auto_invalidation) InactivationTime = 0;

	WWASSERT(d3d_texture);
	LegacyVolumeDesc d3d_desc;
	::ZeroMemory(&d3d_desc, sizeof(d3d_desc));

	DX8_ErrorCode(Peek_Legacy_Volume_Texture(*this)->GetLevelDesc(0,&d3d_desc));

	if (initialized)
	{
		TextureFormat=D3DFormat_To_WW3DFormat(d3d_desc.Format);
		Width=d3d_desc.Width;
		Height=d3d_desc.Height;
		Depth=d3d_desc.Depth;
	}
#endif
}
