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

#include <d3d8.h>
#include "dx8wrapper.h"
#include "TARGA.h"
#include <nstrdup.h>
#include "w3d_file.h"
#include "assetmgr.h"
#include "dx8formatconv.h"
#include "dx8texturelegacytypes.h"
#include "dx8textureinterop.h"
#include "textureloader.h"
#include "missingtexture.h"
#include "ffactory.h"
#include "TextureResourceManager.h"
#if !defined(GGC_BGFX_STANDALONE)
#include "dx8texman.h"
#endif
#include "meshmatdesc.h"
#include "texturethumbnail.h"
#include "wwprofile.h"
#include "RenderBackend.h"
#include "IRenderBackend.h"
#include "DXTUtils.h"
#include <cstring>
#include <utility>

const unsigned DEFAULT_INACTIVATION_TIME=20000;

namespace
{
	constexpr unsigned kLegacyLockReadOnly = 0x00000010L;

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
		LegacyTexture(nullptr),
		CPUTextureRevision(0),
		m_backendHandle(kInvalidRenderResource),
	Initialized(false),
   Name(""),
	FullPath(""),
	texture_id(unused_texture_id++),
	IsLightmap(false),
	IsProcedural(false),
	IsReducible(reducible),
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
		// Phase 5 Stage 1: also release the backend-neutral resource.
		if (m_backendHandle != kInvalidRenderResource) {
			g_renderBackend->Destroy_Resource(m_backendHandle);
			m_backendHandle = kInvalidRenderResource;
		}
	}

	if (LegacyTexture)
	{
		Legacy_Texture(LegacyTexture)->Release();
		LegacyTexture = nullptr;
	}
	Clear_CPU_Texture_Snapshot();

	TextureResourceManagerClass::Remove(this);
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

	if (LegacyTexture)
	{
		Legacy_Texture(LegacyTexture)->Release();
		LegacyTexture = nullptr;
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

	if (LegacyTexture)
	{
		LegacyTexture->Release();
		LegacyTexture = nullptr;
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

void TextureBaseClass::Set_CPU_Texture_Snapshot(std::vector<TextureMipSnapshot> &&mips)
{
	CPUTextureMips = std::move(mips);
	PreserveCPUTextureSnapshotOnNextLegacySet = true;
	++CPUTextureRevision;
}

void TextureBaseClass::Capture_CPU_Texture_Snapshot(void *native_texture)
{
	PreserveCPUTextureSnapshotOnNextLegacySet = false;
	CPUTextureMips.clear();
	++CPUTextureRevision;

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
}


//**********************************************************************************************
//! Load locked surface
/*!
*/
void TextureBaseClass::Load_Locked_Surface()
{
	WWPROFILE(("TextureClass::Load_Locked_Surface()"));
	if (LegacyTexture) Legacy_Texture(LegacyTexture)->Release();
	LegacyTexture=nullptr;
	TextureLoader::Request_Thumbnail(this);
	Initialized=false;
}


//**********************************************************************************************
//! Is missing texture
/*!
*/
bool TextureBaseClass::Is_Missing_Texture()
{
	bool flag = false;
	LegacyBaseTexture *missing_texture = Get_Legacy_Missing_Texture();

	if (Legacy_Texture(LegacyTexture) == missing_texture)
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
	if (!LegacyTexture)
	{
		WWASSERT_PRINT(0, "Get_Priority: LegacyTexture is null!");
		return 0;
	}

	return Legacy_Texture(LegacyTexture)->GetPriority();
}


//**********************************************************************************************
//! Set priority
/*!
*/
unsigned int TextureBaseClass::Set_Priority(unsigned int priority)
{
	if (!LegacyTexture)
	{
		WWASSERT_PRINT(0, "Set_Priority: LegacyTexture is null!");
		return 0;
	}

	return Legacy_Texture(LegacyTexture)->SetPriority(priority);
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

#if !defined(GGC_BGFX_STANDALONE)
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
		if (TextureLoader::Is_DX8_Thread())
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

	LegacyBaseTexture *newTexture = Create_Legacy_Texture_From_Surface
	(
		Peek_Legacy_Surface(*surface),
		mip_level_count
	);
	Poke_Legacy_Texture(*this, newTexture);
	const SurfaceClass::SurfaceImageData *surface_image = surface->Get_CPU_Surface_Image();
	if (surface_image != nullptr && mip_level_count == MIP_LEVELS_1) {
		std::vector<TextureMipSnapshot> mips;
		TextureMipSnapshot mip;
		mip.Width = surface_image->Width;
		mip.Height = surface_image->Height;
		mip.Pitch = surface_image->Pitch;
		mip.Format = surface_image->Format;
		mip.Data = surface_image->Data;
		mips.push_back(std::move(mip));
		Set_CPU_Texture_Snapshot(std::move(mips));
	} else {
		Refresh_CPU_Texture_Snapshot();
	}
	LastAccessed=WW3D::Get_Sync_Time();
}

// ----------------------------------------------------------------------------
TextureClass::TextureClass(void *legacy_texture)
:	TextureBaseClass
	(
		0,
		0,
		((MipCountType)static_cast<LegacyBaseTexture *>(legacy_texture)->GetLevelCount())
	),
	Filter((MipCountType)static_cast<LegacyBaseTexture *>(legacy_texture)->GetLevelCount())
{
	LegacyBaseTexture *d3d_texture = static_cast<LegacyBaseTexture *>(legacy_texture);
	Initialized=true;
	IsProcedural=true;
	IsReducible=false;

	Set_Legacy_Base_Texture(*this, d3d_texture);
	LegacyTextureSurface *surface;
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
}

//**********************************************************************************************
//! Initialise the texture
/*!
*/
void TextureClass::Init()
{
	// If the texture has already been initialised we should exit now
	if (Initialized) return;

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


	if (!Peek_Legacy_Base_Texture(*this))
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
void TextureClass::Apply_Legacy_Surface
(
	void *native_texture,
	bool initialized,
	bool disable_auto_invalidation
)
{
	LegacyBaseTexture *d3d_texture = Legacy_Texture(native_texture);
	Set_Legacy_Base_Texture(*this, d3d_texture);

	if (initialized) Initialized=true;
	if (disable_auto_invalidation) InactivationTime = 0;

	WWASSERT(d3d_texture);
	LegacyTextureSurface *surface;
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
	if (!Peek_Legacy_Texture2D(*this))
	{
		WWASSERT_PRINT(0, "Get_Surface_Level: LegacyTexture is null!");
		return nullptr;
	}

	LegacyTextureSurface *d3d_surface = nullptr;
	DX8_ErrorCode(Peek_Legacy_Texture2D(*this)->GetSurfaceLevel(level, &d3d_surface));
	SurfaceClass *surface = Create_Legacy_Surface_Wrapper(d3d_surface);
	d3d_surface->Release();

	return surface;
}

//**********************************************************************************************
//! Get surface description for a mip level
/*!
*/
void TextureClass::Get_Level_Description( SurfaceClass::SurfaceDescription & desc, unsigned int level )
{
	SurfaceClass * surf = Get_Surface_Level(level);
	if (surf != nullptr) {
		surf->Get_Description(desc);
	}
	REF_PTR_RELEASE(surf);
}

unsigned int TextureClass::Get_Level_Count() const
{
	auto *texture = Peek_Legacy_Texture2D(*this);
	return texture != nullptr ? texture->GetLevelCount() : 0;
}

bool TextureClass::Generate_Mip_Levels()
{
	return Generate_Legacy_Texture_Mips(*this);
}

void TextureClass::Set_LOD(unsigned int lod) const
{
	auto *texture = Peek_Legacy_Texture2D(*this);
	if (texture != nullptr)
	{
		DX8_ErrorCode(texture->SetLOD(static_cast<DWORD>(lod)));
	}
}

//**********************************************************************************************
//! Get legacy surface from mip level
/*!
*/
void *TextureClass::Get_Legacy_Surface_Level(unsigned int level)
{
	if (!Peek_Legacy_Texture2D(*this))
	{
		WWASSERT_PRINT(0, "Get_Legacy_Surface_Level: native texture is null!");
		return nullptr;
	}

	LegacyTextureSurface *d3d_surface = nullptr;
	DX8_ErrorCode(Peek_Legacy_Texture2D(*this)->GetSurfaceLevel(level, &d3d_surface));
	return d3d_surface;
}

//**********************************************************************************************
//! Get texture memory usage
/*!
*/
unsigned TextureClass::Get_Texture_Memory_Usage() const
{
	int size=0;
	if (!Peek_Legacy_Texture2D(*this)) return 0;
	for (unsigned i=0;i<Peek_Legacy_Texture2D(*this)->GetLevelCount();++i)
	{
		LegacySurfaceDesc desc;
		DX8_ErrorCode(Peek_Legacy_Texture2D(*this)->GetLevelDesc(i,&desc));
		size+=desc.Size;
	}
	return size;
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

#if !defined(GGC_BGFX_STANDALONE)
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
void ZTextureClass::Apply_Legacy_Surface
(
	void *native_texture,
	bool initialized,
	bool disable_auto_invalidation
)
{
	LegacyBaseTexture *d3d_texture = Legacy_Texture(native_texture);
	Set_Legacy_Base_Texture(*this, d3d_texture);

	if (initialized) Initialized=true;
	if (disable_auto_invalidation) InactivationTime = 0;

	WWASSERT(Peek_Legacy_Texture2D(*this));
	LegacyTextureSurface *surface;
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
}

//**********************************************************************************************
//! Get legacy surface from mip level
/*!
*/
void *ZTextureClass::Get_Legacy_Surface_Level(unsigned int level)
{
	if (!Peek_Legacy_Texture2D(*this))
	{
		WWASSERT_PRINT(0, "Get_Legacy_Surface_Level: native texture is null!");
		return nullptr;
	}

	LegacyTextureSurface *d3d_surface = nullptr;
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
	for (unsigned i=0;i<Peek_Legacy_Texture2D(*this)->GetLevelCount();++i)
	{
		LegacySurfaceDesc desc;
		DX8_ErrorCode(Peek_Legacy_Texture2D(*this)->GetLevelDesc(i,&desc));
		size+=desc.Size;
	}
	return size;
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

#if !defined(GGC_BGFX_STANDALONE)
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
		if (TextureLoader::Is_DX8_Thread())
		{
			Init();
		}
	}
}

//**********************************************************************************************
//! Apply new surface to texture
/*!
*/
void CubeTextureClass::Apply_Legacy_Surface
(
	void *native_texture,
	bool initialized,
	bool disable_auto_invalidation
)
{
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

#if !defined(GGC_BGFX_STANDALONE)
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
		if (TextureLoader::Is_DX8_Thread())
		{
			Init();
		}
	}
}

//**********************************************************************************************
//! Apply new surface to texture
/*!
*/
void VolumeTextureClass::Apply_Legacy_Surface
(
	void *native_texture,
	bool initialized,
	bool disable_auto_invalidation
)
{
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
}
