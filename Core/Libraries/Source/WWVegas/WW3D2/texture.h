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
 *                     $Archive:: /Commando/Code/ww3d2/texture.h                              $*
 *                                                                                             *
 *                  $Org Author:: Jani_p                                                      $*
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 08/05/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 46                                                          $*
 *                                                                                             *
 * 05/16/02 KM Base texture class to abstract major texture types, e.g. 3d, z, cube, etc.
 * 06/27/02 KM Texture class abstraction																			*
 * 08/05/02 KM Texture class redesign (revisited)
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "always.h"
#include "chunkio.h"
#include "surfaceclass.h"
#include "ww3dformat.h"
#include "wwstring.h"
#include "vector3.h"
#include "texturefilter.h"
#include "IRenderBackend.h"
#include <vector>

class TextureLoader;
class LoaderThreadClass;
class TextureLoadTaskClass;
class TextureClass;
class CubeTextureClass;
class VolumeTextureClass;
class TextureCompatibilityInterop;
struct TextureCompatibilityState;

class TextureBaseClass : public RefCountClass
{
	friend class TextureLoader;
	friend class LoaderThreadClass;
	friend class DX8TextureTrackerClass;  //(gth) so it can poke the native texture,
	friend class DX8ZTextureTrackerClass;
	friend class TextureCompatibilityInterop;

public:

	enum PoolType
	{
		POOL_DEFAULT=0,
		POOL_MANAGED,
		POOL_SYSTEMMEM
	};

	enum TexAssetType
	{
		TEX_REGULAR,
		TEX_CUBEMAP,
		TEX_VOLUME
	};

	// base constructor for derived classes
	TextureBaseClass
	(
		unsigned width,
		unsigned height,
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		PoolType pool=POOL_MANAGED,
		bool rendertarget=false,
		bool reducible=true
	);

	virtual ~TextureBaseClass() override;

	virtual TexAssetType Get_Asset_Type() const=0;

	// Names
	void	Set_Texture_Name(const char * name);
	void	Set_Full_Path(const char * path)			{ FullPath = path; }
	const StringClass& Get_Texture_Name() const		{ return Name; }
	const StringClass& Get_Full_Path() const			{ if (FullPath.Is_Empty ()) return Name; return FullPath; }

	unsigned Get_ID() const { return texture_id; }	// Each textrure has a unique id

	// The number of Mip levels in the texture
	unsigned int Get_Mip_Level_Count() const
	{
		return MipLevelCount;
	}

	// Note! Width and Height may be zero and may change if texture uses mipmaps
	int Get_Width() const
	{
		return Width;
	}
	int Get_Height() const
	{
		return Height;
	}

	// Time, after which the texture is invalidated if not used. Set to zero to indicate infinite.
	// Time is in milliseconds.
	void Set_Inactivation_Time(unsigned time) { InactivationTime=time; }
	int Get_Inactivation_Time() const { return InactivationTime; }

	// Texture priority affects texture management and caching.
	unsigned int Get_Priority();
	unsigned int Set_Priority(unsigned int priority);	// Returns previous priority

	// Debug utility functions for returning the texture memory usage
	virtual unsigned Get_Texture_Memory_Usage() const=0;

	bool Is_Initialized() const { return Initialized; }
	bool Is_Lightmap() const { return IsLightmap; }
	bool Is_Procedural() const { return IsProcedural; }
	bool Is_Render_Target() const { return IsRenderTarget; }
	bool Is_Reducible() const { return IsReducible; } //can texture be reduced in resolution for LOD purposes?

	static int _Get_Total_Locked_Surface_Size();
	static int _Get_Total_Texture_Size();
	static int _Get_Total_Lightmap_Texture_Size();
	static int _Get_Total_Procedural_Texture_Size();
	static int _Get_Total_Locked_Surface_Count();
	static int _Get_Total_Texture_Count();
	static int _Get_Total_Lightmap_Texture_Count();
	static int _Get_Total_Procedural_Texture_Count();

	virtual void Init()=0;

	// This utility function processes the texture reduction (used during rendering)
	void Invalidate();

		struct TextureMipSnapshot
		{
			unsigned Width;
			unsigned Height;
			unsigned Pitch;
			WW3DFormat Format;
			std::vector<unsigned char> Data;
		};
		const std::vector<TextureMipSnapshot>& Get_CPU_Texture_Mips() const { return CPUTextureMips; }
		bool Has_CPU_Texture_Mips() const { return !CPUTextureMips.empty(); }
		void Release_CPU_Texture_Mips() { CPUTextureMips.clear(); CPUTextureMips.shrink_to_fit(); }
		unsigned Get_CPU_Texture_Revision() const { return CPUTextureRevision; }
		void Refresh_CPU_Texture_Snapshot();
		void Share_Texture_Storage_With(const TextureBaseClass *source);
		bool Has_Compatibility_Texture() const;

	PoolType Get_Pool() const { return Pool; }

	bool Is_Missing_Texture();
	void Mark_Missing_Texture(bool missing) { IsMissingTexture = missing; }

	// Support for self managed textures
	bool Is_Dirty() { WWASSERT(Pool==POOL_DEFAULT); return Dirty; };
	void Set_Dirty() { WWASSERT(Pool==POOL_DEFAULT); Dirty=true; }
	void Clean() { Dirty=false; };

	void Set_HSV_Shift(const Vector3 &hsv_shift);
	const Vector3& Get_HSV_Shift() { return HSVShift; }

	bool Is_Compression_Allowed() const { return IsCompressionAllowed; }

	unsigned Get_Reduction() const;

	MipCountType MipLevelCount;

	// Inactivate textures that haven't been used in a while. Pass zero to use textures'
	// own inactive times (default). In urgent need to free up texture memory, try
	// calling with relatively small (just few seconds) time override to free up everything
	// but the currently used textures.
	static void Invalidate_Old_Unused_Textures(unsigned inactive_time_override);

	// Apply this texture's settings into D3D
	virtual void Apply(unsigned int stage)=0;

	// Apply a Null texture's settings into D3D
	static void Apply_Null(unsigned int stage);

	virtual TextureClass* As_TextureClass() { return nullptr; }
	virtual CubeTextureClass* As_CubeTextureClass() { return nullptr; }
	virtual VolumeTextureClass* As_VolumeTextureClass() { return nullptr; }

protected:

	void Load_Locked_Surface();
	void Set_CPU_Texture_Snapshot(std::vector<TextureMipSnapshot> &&mips);
	void Update_CPU_Texture_Mip_Snapshot(unsigned int level, TextureMipSnapshot &&mip);
	std::vector<TextureMipSnapshot>& Mutable_CPU_Texture_Mips() { return CPUTextureMips; }
	void Mark_CPU_Texture_Mips_Changed();

	bool Initialized;

	// For debug purposes the texture sets this true if it is a lightmap texture
	bool IsLightmap;
	bool IsRenderTarget;
	bool IsCompressionAllowed;
	bool IsProcedural;
	bool IsReducible;
	bool IsMissingTexture;


	unsigned InactivationTime;	// In milliseconds
	unsigned ExtendedInactivationTime;	// This is set by the engine, if needed
	unsigned LastInactivationSyncTime;
	mutable unsigned LastAccessed;

	// If this is non-zero, the texture will have a hue shift done at the next init (this
	// value should only be changed by Set_HSV_Shift() function, which also invalidates the
	// texture).
	Vector3 HSVShift;

	int Width;
	int Height;

private:
	virtual void Apply_Native_Compatibility_Texture(void *native_texture, bool initialized, bool disable_auto_invalidation = false)=0;

		TextureCompatibilityState *CompatibilityState;
		void *Get_Native_Compatibility_Texture() const;
		void Set_Native_Compatibility_Texture(void *native_texture);
		std::vector<TextureMipSnapshot> CPUTextureMips;
		unsigned CPUTextureRevision;
		void Capture_CPU_Texture_Snapshot(void *native_texture);
		void Clear_CPU_Texture_Snapshot();
		bool PreserveCPUTextureSnapshotOnNextLegacySet;

		// TheSuperHackers @refactor bobtista 21/04/2026 backend-neutral
	// resource handle. Populated by the asset loader after it calls
	// g_renderBackend->Create_Texture(). Parallel to the native compatibility
	// state used by legacy/reference builds so existing compatibility code keeps
	// working. Readers that want to stay backend-neutral should prefer
	// m_backendHandle.
	RenderResource m_backendHandle;

	// Name
	StringClass Name;
	StringClass	FullPath;

	// Unique id
	unsigned texture_id;

	// Support for self-managed textures

	PoolType Pool;
	bool Dirty;

	friend class TextureLoadTaskClass;
	friend class CubeTextureLoadTaskClass;
	friend class VolumeTextureLoadTaskClass;
	TextureLoadTaskClass* TextureLoadTask;
	TextureLoadTaskClass* ThumbnailLoadTask;

};


/*************************************************************************
**                             TextureClass
**
** This is our regular texture class. For legacy reasons it contains some
** information beyond the D3D texture itself, such as texture addressing
** modes.
**
*************************************************************************/
class TextureClass : public TextureBaseClass
{
	W3DMPO_CODE(TextureClass)

public:
	struct TextureAtlasRegion
	{
		unsigned X;
		unsigned Y;
		unsigned Width;
		unsigned Height;
	};
	struct MutableTextureMipView
	{
		WW3DFormat Format = WW3D_FORMAT_UNKNOWN;
		unsigned Width = 0;
		unsigned Height = 0;
		unsigned Pitch = 0;
		unsigned char *Data = nullptr;
		bool Is_Valid() const { return Data != nullptr && Width != 0 && Height != 0 && Pitch != 0; }
	};


	// Create texture with desired height, width and format.
	TextureClass
	(
		unsigned width,
		unsigned height,
		WW3DFormat format,
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		PoolType pool=POOL_MANAGED,
		bool rendertarget=false,
		bool allow_reduction=true
	);

	// Create texture from a file. If format is specified the texture is converted to that format.
	// Note that the format must be supported by the current device and that a texture can't exist
	// in the system with the same name in multiple formats.
	TextureClass
	(
		const char *name,
		const char *full_path=nullptr,
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		WW3DFormat texture_format=WW3D_FORMAT_UNKNOWN,
		bool allow_compression=true,
		bool allow_reduction=true
	);

	// Create texture from a surface.
	TextureClass
	(
		SurfaceClass *surface,
		MipCountType mip_level_count=MIP_LEVELS_ALL
	);

	// default constructors for derived classes (cube & vol)
	TextureClass
	(
		unsigned width,
		unsigned height,
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		PoolType pool=POOL_MANAGED,
		bool rendertarget=false,
		WW3DFormat format=WW3D_FORMAT_UNKNOWN,
		bool allow_reduction=true
	)
	: TextureBaseClass(width,height,mip_level_count,pool,rendertarget,allow_reduction), TextureFormat(format), Filter(mip_level_count) { }

	virtual TexAssetType Get_Asset_Type() const override { return TEX_REGULAR; }

	virtual void Init() override;
	void Clear_Atlas_Regions() { AtlasRegions.clear(); }
	void Copy_Atlas_Regions_From(const TextureClass *texture)
	{
		if (texture != nullptr) {
			AtlasRegions = texture->AtlasRegions;
		} else {
			AtlasRegions.clear();
		}
	}
	void Add_Atlas_Region(unsigned x, unsigned y, unsigned width, unsigned height)
	{
		TextureAtlasRegion region;
		region.X = x;
		region.Y = y;
		region.Width = width;
		region.Height = height;
		AtlasRegions.push_back(region);
	}
	bool Has_Atlas_Regions() const { return !AtlasRegions.empty(); }
	const std::vector<TextureAtlasRegion> &Get_Atlas_Regions() const { return AtlasRegions; }

	// Get the surface of one of the mipmap levels (defaults to highest-resolution one)
	SurfaceClass *Get_Surface_Level(unsigned int level = 0);
	MutableTextureMipView Begin_Mip_Write(unsigned int level = 0);
	void End_Mip_Write(unsigned int level = 0);
	void Update_Surface_Level_From_Surface(unsigned int level, const SurfaceClass::SurfaceImageData &image);
	void Get_Level_Description( SurfaceClass::SurfaceDescription & desc, unsigned int level = 0 );
	unsigned int Get_Level_Count() const;
	bool Generate_Mip_Levels();
	void Set_LOD(unsigned int lod) const;

	TextureFilterClass& Get_Filter() { return Filter; }

	WW3DFormat Get_Texture_Format() const { return TextureFormat; }

	virtual void Apply(unsigned int stage) override;

	virtual unsigned Get_Texture_Memory_Usage() const override;

	virtual TextureClass* As_TextureClass() override { return this; }

protected:
	TextureClass(void *legacy_texture);

	WW3DFormat				TextureFormat;

	// legacy
	TextureFilterClass	Filter;
	std::vector<TextureAtlasRegion> AtlasRegions;

private:
	friend class TextureLoader;
	friend class TextureLoadTaskClass;
	friend class TextureCompatibilityInterop;
	void Apply_Native_Compatibility_Texture(void *native_texture, bool initialized, bool disable_auto_invalidation = false) override;
	void *Get_Native_Compatibility_Surface_Level(unsigned int level = 0);
};

class ZTextureClass : public TextureBaseClass
{
public:
	// Create a z texture with desired height, width and format
	ZTextureClass
	(
		unsigned width,
		unsigned height,
		WW3DZFormat zformat,
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		PoolType pool=POOL_MANAGED
	);

	WW3DZFormat Get_Texture_Format() const { return DepthStencilTextureFormat; }

	virtual TexAssetType Get_Asset_Type() const override { return TEX_REGULAR; }

	virtual void Init() override {}

	virtual void Apply(unsigned int stage) override;

	virtual unsigned Get_Texture_Memory_Usage() const override;

private:
	friend class TextureCompatibilityInterop;
	void Apply_Native_Compatibility_Texture(void *native_texture, bool initialized, bool disable_auto_invalidation = false) override;
	void *Get_Native_Compatibility_Surface_Level(unsigned int level = 0);

	WW3DZFormat DepthStencilTextureFormat;
};

class CubeTextureClass : public TextureClass
{
public:
	// Create texture with desired height, width and format.
	CubeTextureClass
	(
		unsigned width,
		unsigned height,
		WW3DFormat format,
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		PoolType pool=POOL_MANAGED,
		bool rendertarget=false,
		bool allow_reduction=true
	);

	// Create texture from a file. If format is specified the texture is converted to that format.
	// Note that the format must be supported by the current device and that a texture can't exist
	// in the system with the same name in multiple formats.
	CubeTextureClass
	(
		const char *name,
		const char *full_path=nullptr,
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		WW3DFormat texture_format=WW3D_FORMAT_UNKNOWN,
		bool allow_compression=true,
		bool allow_reduction=true
	);

	// Create texture from a surface.
	CubeTextureClass
	(
		SurfaceClass *surface,
		MipCountType mip_level_count=MIP_LEVELS_ALL
	);

	virtual TexAssetType Get_Asset_Type() const override { return TEX_CUBEMAP; }

	virtual CubeTextureClass* As_CubeTextureClass() override { return this; }

private:
	void Apply_Native_Compatibility_Texture(void *native_texture, bool initialized, bool disable_auto_invalidation = false) override;

};

class VolumeTextureClass : public TextureClass
{
public:
	// Create texture with desired height, width and format.
	VolumeTextureClass
	(
		unsigned width,
		unsigned height,
		unsigned depth,
		WW3DFormat format,
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		PoolType pool=POOL_MANAGED,
		bool rendertarget=false,
		bool allow_reduction=true
	);

	// Create texture from a file. If format is specified the texture is converted to that format.
	// Note that the format must be supported by the current device and that a texture can't exist
	// in the system with the same name in multiple formats.
	VolumeTextureClass
	(
		const char *name,
		const char *full_path=nullptr,
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		WW3DFormat texture_format=WW3D_FORMAT_UNKNOWN,
		bool allow_compression=true,
		bool allow_reduction=true
	);

	// Create texture from a surface.
	VolumeTextureClass
	(
		SurfaceClass *surface,
		MipCountType mip_level_count=MIP_LEVELS_ALL
	);

	virtual TexAssetType Get_Asset_Type() const override { return TEX_VOLUME; }

	virtual VolumeTextureClass* As_VolumeTextureClass() override { return this; }

protected:

	int Depth;

private:
	void Apply_Native_Compatibility_Texture(void *native_texture, bool initialized, bool disable_auto_invalidation = false) override;
};

// Utility functions for loading and saving texture descriptions from/to W3D files
TextureClass *Load_Texture(ChunkLoadClass & cload);
void Save_Texture(TextureClass * texture, ChunkSaveClass & csave);
