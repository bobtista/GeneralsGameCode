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
 *                     $Archive:: /Commando/Code/ww3d2/surfaceclass.cpp                       $*
 *                                                                                             *
 *              Original Author:: Nathaniel Hoffman                                            *
 *                                                                                             *
 *                      $Author:: Greg_h2                                                     $*
 *                                                                                             *
 *                     $Modtime:: 8/30/01 2:01p                                               $*
 *                                                                                             *
 *                    $Revision:: 25                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   SurfaceClass::Clear -- Clears a surface to 0                                              *
 *   SurfaceClass::Copy -- Copies a region from one surface to another of the same format      *
 *   SurfaceClass::FindBBAlpha -- Finds the bounding box of non zero pixels in the region (x0, *
 *   SurfaceClass::Is_Transparent_Column -- Tests to see if the column is transparent or not   *
 *   SurfaceClass::Copy -- Copies from a byte array to the surface                             *
 *   SurfaceClass::CreateCopy -- Creates a byte array copy of the surface                      *
 *   SurfaceClass::DrawHLine -- draws a horizontal line                                        *
 *   SurfaceClass::DrawPixel -- draws a pixel                                                  *
 *   SurfaceClass::Copy -- Copies a block of system ram to the surface                         *
 *   SurfaceClass::Hue_Shift -- changes the hue of the surface                                 *
 *   SurfaceClass::Is_Monochrome -- Checks if surface is monochrome or not                     *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "surfaceclass.h"
#include "texture.h"
#include "textureloader.h"
#include "dx8formatconv.h"
#include "dx8texturelegacytypes.h"
#include "texturecompat.h"
#if !defined(GGC_RENDER_BACKEND_BGFX)
#include "texturecompatibilityinterop.h"
#include "dx8wrapper.h"
#endif
#include "vector2i.h"
#include "colorspace.h"
#include "bound.h"

#if defined(GGC_RENDER_BACKEND_BGFX)
#define DX8_ErrorCode(hr) ((void)(hr))
#endif

namespace
{
	using LegacyRect = RECT;

	constexpr unsigned kLegacyLockReadOnly = 0x00000010L;
	constexpr unsigned kLegacySurfaceCopyNoFilter = 1;
	constexpr unsigned kLegacySurfaceCopyTriangleFilter = 4;

	bool Is_Block_Compressed_Format(WW3DFormat format)
	{
		return format == WW3D_FORMAT_DXT1 ||
			format == WW3D_FORMAT_DXT2 ||
			format == WW3D_FORMAT_DXT3 ||
			format == WW3D_FORMAT_DXT4 ||
			format == WW3D_FORMAT_DXT5;
	}

	bool Can_Store_CPU_Surface_Data(const SurfaceClass::SurfaceDescription &desc)
	{
		return desc.Width != 0 &&
			desc.Height != 0 &&
			desc.Format != WW3D_FORMAT_UNKNOWN &&
			!Is_Block_Compressed_Format(desc.Format) &&
			::Get_Bytes_Per_Pixel(desc.Format) != 0;
	}

	bool Should_Use_CPU_Surface_Snapshots()
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		return true;
#else
		return false;
#endif
	}

	bool Should_Use_CPU_Only_Surface_Storage()
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		return true;
#else
		return false;
#endif
	}
}

struct SurfaceCompatibilityState
{
	void *NativeSurface = nullptr;
};

#define NativeCompatibilitySurface CompatibilityState->NativeSurface
#define NATIVE_COMPATIBILITY_SURFACE static_cast<LegacySurface *>(NativeCompatibilitySurface)
#define OTHER_NATIVE_COMPATIBILITY_SURFACE(surface) static_cast<LegacySurface *>((surface)->NativeCompatibilitySurface)

static LegacySurfaceCopyRect To_Legacy_Surface_Copy_Rect(const LegacyRect& rect)
{
	return {rect.left, rect.top, rect.right, rect.bottom};
}

void Convert_Pixel(Vector3 &rgb, const SurfaceClass::SurfaceDescription &sd, const unsigned char * pixel)
{
	const float scale=1/255.0f;
	switch (sd.Format)
	{
	case WW3D_FORMAT_A8R8G8B8:
	case WW3D_FORMAT_X8R8G8B8:
	case WW3D_FORMAT_R8G8B8:
		{
			rgb.X=pixel[2]; // R
			rgb.Y=pixel[1]; // G
			rgb.Z=pixel[0]; // B
		}
		break;
	case WW3D_FORMAT_A4R4G4B4:
		{
			unsigned short tmp;
			tmp=*(unsigned short*)&pixel[0];
			rgb.X=((tmp&0x0f00)>>4);   // R
			rgb.Y=((tmp&0x00f0));		// G
			rgb.Z=((tmp&0x000f)<<4);	// B
		}
		break;
	case WW3D_FORMAT_A1R5G5B5:
		{
			unsigned short tmp;
			tmp=*(unsigned short*)&pixel[0];
			rgb.X=(tmp>>7)&0xf8; // R
			rgb.Y=(tmp>>2)&0xf8; // G
			rgb.Z=(tmp<<3)&0xf8; // B
		}
		break;
	case WW3D_FORMAT_R5G6B5:
		{
			unsigned short tmp;
			tmp=*(unsigned short*)&pixel[0];
			rgb.X=(tmp>>8)&0xf8;
			rgb.Y=(tmp>>3)&0xfc;
			rgb.Z=(tmp<<3)&0xf8;
		}
		break;

	default:
		// TODO: Implement other pixel formats
		WWASSERT(0);
	}
	rgb*=scale;
}

// Note: This function must never overwrite the original alpha
void Convert_Pixel(unsigned char * pixel,const SurfaceClass::SurfaceDescription &sd, const Vector3 &rgb)
{
	unsigned char r,g,b;
	r=(unsigned char) (rgb.X*255.0f);
	g=(unsigned char) (rgb.Y*255.0f);
	b=(unsigned char) (rgb.Z*255.0f);
	switch (sd.Format)
	{
	case WW3D_FORMAT_A8R8G8B8:
	case WW3D_FORMAT_X8R8G8B8:
	case WW3D_FORMAT_R8G8B8:
		pixel[0]=b;
		pixel[1]=g;
		pixel[2]=r;
		break;
	case WW3D_FORMAT_A4R4G4B4:
		{
			unsigned short tmp;
			tmp=*(unsigned short*)&pixel[0];
			tmp&=0xF000;
			tmp|=(r&0xF0) << 4;
			tmp|=(g&0xF0);
			tmp|=(b&0xF0) >> 4;
			*(unsigned short*)&pixel[0]=tmp;
		}
		break;
	case WW3D_FORMAT_A1R5G5B5:
		{
			unsigned short tmp;
			tmp=*(unsigned short*)&pixel[0];
			tmp&=0x8000;
			tmp|=(r&0xF8) << 7;
			tmp|=(g&0xF8) << 2;
			tmp|=(b&0xF8) >> 3;
			*(unsigned short*)&pixel[0]=tmp;
		}
		break;
	case WW3D_FORMAT_R5G6B5:
		{
			unsigned short tmp;
			tmp=(r&0xf8) << 8;
			tmp|=(g&0xfc) << 3;
			tmp|=(b&0xf8) >> 3;
			*(unsigned short*)&pixel[0]=tmp;
		}
		break;
	default:
		// TODO: Implement other pixel formats
		WWASSERT(0);
	}
}

/*************************************************************************
**                             SurfaceClass
*************************************************************************/
SurfaceClass::SurfaceClass(unsigned width, unsigned height, WW3DFormat format):
	CompatibilityState(new SurfaceCompatibilityState),
	SurfaceFormat(format),
	Description{format, width, height},
	ImageData{format, width, height, 0, {}},
	RefreshCPUAfterUnlock(false),
	CPULockActive(false),
	CPUImagePossiblyStale(false),
	TextureOwner(nullptr),
	TextureOwnerLevel(0)
{
	WWASSERT(width);
	WWASSERT(height);
	if (Should_Use_CPU_Only_Surface_Storage() && Can_Store_CPU_Surface_Data(Description))
	{
		Allocate_CPU_Surface_Snapshot();
		return;
	}
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"SurfaceClass(width,height,format): standalone bgfx cannot create legacy surface fallback");
	return;
#else
	NativeCompatibilitySurface = Create_Legacy_Surface(width, height, format);
	Update_Description_From_Native_Compatibility_Surface();
	Capture_CPU_Surface_Snapshot();
#endif
}

SurfaceClass::SurfaceClass(const char *filename):
	CompatibilityState(new SurfaceCompatibilityState),
	SurfaceFormat(WW3D_FORMAT_UNKNOWN),
	Description{WW3D_FORMAT_UNKNOWN, 0, 0},
	ImageData{WW3D_FORMAT_UNKNOWN, 0, 0, 0, {}},
	RefreshCPUAfterUnlock(false),
	CPULockActive(false),
	CPUImagePossiblyStale(false),
	TextureOwner(nullptr),
	TextureOwnerLevel(0)
{
	if (Should_Use_CPU_Only_Surface_Storage())
	{
		const bool loaded =
			TextureLoader::Load_Surface_Image_Immediate(filename, WW3D_FORMAT_UNKNOWN, false, ImageData);
		WWASSERT_PRINT(
			loaded,
			"BGFX CPU surface image load failed; no legacy surface fallback is allowed");
		if (!loaded) {
			return;
		}
		Description.Format = ImageData.Format;
		Description.Width = ImageData.Width;
		Description.Height = ImageData.Height;
		SurfaceFormat = Description.Format;
		return;
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(false, "SurfaceClass(filename): standalone bgfx must use CPU surface path");
#else
	NativeCompatibilitySurface = Create_Legacy_Surface_From_File(filename);
	Update_Description_From_Native_Compatibility_Surface();
	Capture_CPU_Surface_Snapshot();
#endif
}

SurfaceClass::SurfaceClass(const SurfaceImageData &image):
	CompatibilityState(new SurfaceCompatibilityState),
	SurfaceFormat(image.Format),
	Description{image.Format, image.Width, image.Height},
	ImageData{image.Format, image.Width, image.Height, 0, {}},
	RefreshCPUAfterUnlock(false),
	CPULockActive(false),
	CPUImagePossiblyStale(false),
	TextureOwner(nullptr),
	TextureOwnerLevel(0)
{
	WWASSERT(Can_Store_CPU_Surface_Data(Description));
	const unsigned int pixel_size = ::Get_Bytes_Per_Pixel(Description.Format);
	const unsigned int row_size = Description.Width * pixel_size;
	WWASSERT(image.Pitch >= row_size);
	WWASSERT(image.Data.size() >= static_cast<size_t>(image.Pitch) * Description.Height);

	ImageData.Pitch = row_size;
	ImageData.Data.resize(static_cast<size_t>(row_size) * Description.Height);
	for (unsigned int row = 0; row < Description.Height; ++row)
	{
		memcpy(
			ImageData.Data.data() + row * ImageData.Pitch,
			image.Data.data() + row * image.Pitch,
			row_size);
	}
}

SurfaceClass::SurfaceClass(void *native_compatibility_surface)	:
	CompatibilityState(new SurfaceCompatibilityState),
	SurfaceFormat(WW3D_FORMAT_UNKNOWN),
	Description{WW3D_FORMAT_UNKNOWN, 0, 0},
	ImageData{WW3D_FORMAT_UNKNOWN, 0, 0, 0, {}},
	RefreshCPUAfterUnlock(false),
	CPULockActive(false),
	CPUImagePossiblyStale(false),
	TextureOwner(nullptr),
	TextureOwnerLevel(0)
{
	Attach_Native_Compatibility_Surface(native_compatibility_surface);
}

SurfaceClass::~SurfaceClass()
{
	if (TextureOwner != nullptr) {
		TextureOwner->Release_Ref();
		TextureOwner = nullptr;
	}
	if (NativeCompatibilitySurface) {
#if defined(GGC_RENDER_BACKEND_BGFX)
		WWASSERT_PRINT(
			false,
			"SurfaceClass::~SurfaceClass: standalone bgfx cannot release fake-D3D surfaces");
#else
		NATIVE_COMPATIBILITY_SURFACE->Release();
#endif
		NativeCompatibilitySurface = nullptr;
	}
	delete CompatibilityState;
	CompatibilityState = nullptr;
}

void *SurfaceClass::Get_Native_Compatibility_Surface() const
{
	return CompatibilityState != nullptr ? CompatibilityState->NativeSurface : nullptr;
}

void SurfaceClass::Set_Native_Compatibility_Surface(void *surface)
{
	WWASSERT(CompatibilityState != nullptr);
	CompatibilityState->NativeSurface = surface;
}

void SurfaceClass::Get_Description(SurfaceDescription &surface_desc)
{
	surface_desc = Description;
}

unsigned int SurfaceClass::Get_Bytes_Per_Pixel()
{
	SurfaceDescription surfaceDesc;
	Get_Description(surfaceDesc);
	return ::Get_Bytes_Per_Pixel(surfaceDesc.Format);
}

SurfaceClass::LockedSurfacePtr SurfaceClass::Lock(int *pitch)
{
	if (Has_Compatible_CPU_Surface_Snapshot(Description))
	{
		*pitch = ImageData.Pitch;
		RefreshCPUAfterUnlock = false;
		CPULockActive = true;
		return static_cast<LockedSurfacePtr>(ImageData.Data.data());
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"SurfaceClass::Lock: standalone bgfx requires a CPU surface snapshot");
	*pitch = 0;
	return nullptr;
#else
	LegacyLockedRect lock_rect;
	::ZeroMemory(&lock_rect, sizeof(lock_rect));
	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->LockRect(&lock_rect, nullptr, 0));
	*pitch = lock_rect.Pitch;
	RefreshCPUAfterUnlock = Should_Use_CPU_Surface_Snapshots() && Has_CPU_Surface_Snapshot();
	return static_cast<LockedSurfacePtr>(lock_rect.pBits);
#endif
}

SurfaceClass::LockedSurfacePtr SurfaceClass::Lock(int *pitch, const Vector2i &min, const Vector2i &max)
{
	if (Has_Compatible_CPU_Surface_Snapshot(Description))
	{
		const unsigned int pixel_size = ::Get_Bytes_Per_Pixel(Description.Format);
		*pitch = ImageData.Pitch;
		RefreshCPUAfterUnlock = false;
		CPULockActive = true;
		return static_cast<LockedSurfacePtr>(
			ImageData.Data.data() +
			min.J * ImageData.Pitch +
			min.I * pixel_size);
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"SurfaceClass::Lock(rect): standalone bgfx requires a CPU surface snapshot");
	*pitch = 0;
	return nullptr;
#else
	LegacyLockedRect lock_rect;
	::ZeroMemory(&lock_rect, sizeof(lock_rect));

	LegacyRect rect;
	rect.left = min.I;
	rect.top = min.J;
	rect.right = max.I;
	rect.bottom = max.J;
	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->LockRect(&lock_rect, &rect, 0));

	*pitch = lock_rect.Pitch;
	RefreshCPUAfterUnlock = Should_Use_CPU_Surface_Snapshots() && Has_CPU_Surface_Snapshot();
	return static_cast<LockedSurfacePtr>(lock_rect.pBits);
#endif
}

void SurfaceClass::Unlock()
{
	if (CPULockActive)
	{
		CPULockActive = false;
		Upload_CPU_Surface_Snapshot_To_Native_Compatibility_Surface();
		return;
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"SurfaceClass::Unlock: standalone bgfx cannot unlock fake-D3D surfaces");
	return;
#else
	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->UnlockRect());
	if (RefreshCPUAfterUnlock) {
		RefreshCPUAfterUnlock = false;
		Capture_CPU_Surface_Snapshot();
	}
#endif
}

/***********************************************************************************************
 * SurfaceClass::Clear -- Clears a surface to 0                                                *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   2/13/2001  hy : Created.                                                                  *
 *=============================================================================================*/
void SurfaceClass::Clear()
{
	SurfaceDescription sd;
	Get_Description(sd);

	// size of each pixel in bytes
	unsigned int size=::Get_Bytes_Per_Pixel(sd.Format);
	if (Has_Compatible_CPU_Surface_Snapshot(sd))
	{
		const unsigned int row_size = size * sd.Width;
		for (unsigned int row = 0; row < sd.Height; ++row)
		{
			memset(ImageData.Data.data() + row * ImageData.Pitch, 0, row_size);
		}
		Upload_CPU_Surface_Snapshot_To_Native_Compatibility_Surface();
		return;
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"SurfaceClass::Clear: standalone bgfx requires a CPU surface snapshot");
	return;
#else
	LegacyLockedRect lock_rect;
	::ZeroMemory(&lock_rect, sizeof(lock_rect));
	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->LockRect(&lock_rect,nullptr,0));
	unsigned int i;
	unsigned char *mem=(unsigned char *) lock_rect.pBits;

	for (i=0; i<sd.Height; i++)
	{
		memset(mem,0,size*sd.Width);
		mem+=lock_rect.Pitch;
	}

	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->UnlockRect());
	Refresh_CPU_Surface_Snapshot_If_Present();
#endif
}


/***********************************************************************************************
 * SurfaceClass::Copy -- Copies from a byte array to the surface                               *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   3/15/2001  hy : Created.                                                                  *
 *=============================================================================================*/
void SurfaceClass::Copy(const unsigned char *other)
{
	SurfaceDescription sd;
	Get_Description(sd);

	// size of each pixel in bytes
	unsigned int size=::Get_Bytes_Per_Pixel(sd.Format);
	Copy(other, sd.Width * size);
}


/***********************************************************************************************
 * SurfaceClass::Copy -- Copies from a pitched byte array to the surface                       *
 *=============================================================================================*/
void SurfaceClass::Copy(const unsigned char *other, unsigned int pitch)
{
	SurfaceDescription sd;
	Get_Description(sd);

	// size of each pixel in bytes
	unsigned int size=::Get_Bytes_Per_Pixel(sd.Format);
	if (Has_Compatible_CPU_Surface_Snapshot(sd))
	{
		const unsigned int row_size = size * sd.Width;
		for (unsigned int row = 0; row < sd.Height; ++row)
		{
			memcpy(ImageData.Data.data() + row * ImageData.Pitch, other + row * pitch, row_size);
		}
		Upload_CPU_Surface_Snapshot_To_Native_Compatibility_Surface();
		return;
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"SurfaceClass::Copy(bytes): standalone bgfx requires a CPU surface snapshot");
	return;
#else
	LegacyLockedRect lock_rect;
	::ZeroMemory(&lock_rect, sizeof(lock_rect));
	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->LockRect(&lock_rect,nullptr,0));
	unsigned int i;
	unsigned char *mem=(unsigned char *) lock_rect.pBits;

	for (i=0; i<sd.Height; i++)
	{
		memcpy(mem,&other[i*pitch],size*sd.Width);
		mem+=lock_rect.Pitch;
	}

	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->UnlockRect());
	Refresh_CPU_Surface_Snapshot_If_Present();
#endif
}


/***********************************************************************************************
 * SurfaceClass::Copy -- Copies a block of system ram to the surface                           *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   5/2/2001   hy : Created.                                                                  *
 *=============================================================================================*/
void SurfaceClass::Copy(const Vector2i &min, const Vector2i &max, const unsigned char *other)
{
	SurfaceDescription sd;
	Get_Description(sd);

	// size of each pixel in bytes
	unsigned int size=::Get_Bytes_Per_Pixel(sd.Format);
	if (Has_Compatible_CPU_Surface_Snapshot(sd))
	{
		int dx=max.I-min.I;
		for (int i=min.J; i<max.J; i++)
		{
			unsigned char *dst =
				ImageData.Data.data() +
				i * ImageData.Pitch +
				min.I * size;
			memcpy(dst,&other[(i*sd.Width+min.I)*size],size*dx);
		}
		Upload_CPU_Surface_Snapshot_To_Native_Compatibility_Surface();
		return;
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"SurfaceClass::Copy(rect,bytes): standalone bgfx requires a CPU surface snapshot");
	return;
#else
	LegacyLockedRect lock_rect;
	::ZeroMemory(&lock_rect, sizeof(lock_rect));
	LegacyRect rect;
	rect.left=min.I;
	rect.right=max.I;
	rect.top=min.J;
	rect.bottom=max.J;
	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->LockRect(&lock_rect,&rect,0));
	int i;
	unsigned char *mem=(unsigned char *) lock_rect.pBits;
	int dx=max.I-min.I;

	for (i=min.J; i<max.J; i++)
	{
		memcpy(mem,&other[(i*sd.Width+min.I)*size],size*dx);
		mem+=lock_rect.Pitch;
	}

	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->UnlockRect());
	Refresh_CPU_Surface_Snapshot_If_Present();
#endif
}


/***********************************************************************************************
 * SurfaceClass::CreateCopy -- Creates a byte array copy of the surface                        *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   3/16/2001  hy : Created.                                                                  *
 *=============================================================================================*/
unsigned char *SurfaceClass::CreateCopy(int *width,int *height,int*size,bool flip)
{
	SurfaceDescription sd;
	Get_Description(sd);

	// size of each pixel in bytes
	unsigned int mysize=::Get_Bytes_Per_Pixel(sd.Format);

	*width=sd.Width;
	*height=sd.Height;
	*size=mysize;

	unsigned char *other=W3DNEWARRAY unsigned char [sd.Height*sd.Width*mysize];

	if (Has_Compatible_CPU_Surface_Snapshot(sd))
	{
		for (unsigned int i = 0; i < sd.Height; i++)
		{
			const unsigned char *src = ImageData.Data.data() + i * ImageData.Pitch;
			if (flip)
			{
				memcpy(&other[(sd.Height-i-1)*sd.Width*mysize],src,mysize*sd.Width);
			} else
			{
				memcpy(&other[i*sd.Width*mysize],src,mysize*sd.Width);
			}
		}
		return other;
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"SurfaceClass::CreateCopy: standalone bgfx requires a CPU surface snapshot");
	delete [] other;
	return nullptr;
#else
	LegacyLockedRect lock_rect;
	::ZeroMemory(&lock_rect, sizeof(lock_rect));
	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->LockRect(&lock_rect,nullptr,kLegacyLockReadOnly));
	unsigned int i;
	unsigned char *mem=(unsigned char *) lock_rect.pBits;

	for (i=0; i<sd.Height; i++)
	{
		if (flip)
		{
			memcpy(&other[(sd.Height-i-1)*sd.Width*mysize],mem,mysize*sd.Width);
		} else
		{
			memcpy(&other[i*sd.Width*mysize],mem,mysize*sd.Width);
		}
		mem+=lock_rect.Pitch;
	}

	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->UnlockRect());

	return other;
#endif
}

const SurfaceClass::SurfaceImageData *SurfaceClass::Get_CPU_Surface_Image() const
{
	SurfaceClass *self = const_cast<SurfaceClass *>(this);
	self->Ensure_CPU_Surface_Snapshot_Current();
	return self->Has_CPU_Surface_Snapshot() ? &ImageData : nullptr;
}


/***********************************************************************************************
 * SurfaceClass::Copy -- Copies a region from one surface to another                           *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   2/13/2001  hy : Created.                                                                  *
 *=============================================================================================*/
void SurfaceClass::Copy(
	unsigned int dstx, unsigned int dsty,
	unsigned int srcx, unsigned int srcy,
	unsigned int width, unsigned int height,
	const SurfaceClass *other)
{
	WWASSERT(other);
	WWASSERT(width);
	WWASSERT(height);

	SurfaceDescription sd,osd;
	Get_Description(sd);
	const_cast <SurfaceClass*>(other)->Get_Description(osd);

	LegacyRect src;
	src.left=srcx;
	src.right=srcx+width;
	src.top=srcy;
	src.bottom=srcy+height;

	if (src.right>int(osd.Width)) src.right=int(osd.Width);
	if (src.bottom>int(osd.Height)) src.bottom=int(osd.Height);

	if (sd.Format==osd.Format && sd.Width==osd.Width && sd.Height==osd.Height)
	{
		if (srcx >= osd.Width || srcy >= osd.Height || dstx >= sd.Width || dsty >= sd.Height) {
			return;
		}

		unsigned int copy_width = width;
		unsigned int copy_height = height;
		copy_width = MIN(copy_width, osd.Width - srcx);
		copy_width = MIN(copy_width, sd.Width - dstx);
		copy_height = MIN(copy_height, osd.Height - srcy);
		copy_height = MIN(copy_height, sd.Height - dsty);

		if (copy_width == 0 || copy_height == 0) {
			return;
		}

		const unsigned int pixel_size = ::Get_Bytes_Per_Pixel(sd.Format);
		const unsigned int row_size = copy_width * pixel_size;
		const bool dst_has_cpu_snapshot = Has_Compatible_CPU_Surface_Snapshot(sd);
		const bool src_has_cpu_snapshot = other->Has_Compatible_CPU_Surface_Snapshot(osd);

		if (Should_Use_CPU_Only_Surface_Storage() &&
			(pixel_size == 0 || !dst_has_cpu_snapshot || !src_has_cpu_snapshot))
		{
			WWASSERT_PRINT(
				false,
				"SurfaceClass::Copy: BGFX surface ownership missing CPU snapshots; no legacy surface copy fallback is allowed");
			return;
		}

		if (other == this)
		{
			if (dst_has_cpu_snapshot)
			{
				unsigned char *base = ImageData.Data.data();
				for (unsigned int y = 0; y < copy_height; ++y)
				{
					unsigned char *dst_row = base + (dsty + y) * ImageData.Pitch + dstx * pixel_size;
					unsigned char *src_row = base + (srcy + y) * ImageData.Pitch + srcx * pixel_size;
					memmove(dst_row, src_row, row_size);
				}
				Upload_CPU_Surface_Snapshot_To_Native_Compatibility_Surface();
				return;
			}

			LegacyLockedRect lock_rect;
			::ZeroMemory(&lock_rect, sizeof(lock_rect));
			DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->LockRect(&lock_rect, nullptr, 0));

			unsigned char *base = static_cast<unsigned char *>(lock_rect.pBits);
			for (unsigned int y = 0; y < copy_height; ++y)
			{
				unsigned char *dst_row = base + (dsty + y) * lock_rect.Pitch + dstx * pixel_size;
				unsigned char *src_row = base + (srcy + y) * lock_rect.Pitch + srcx * pixel_size;
				memmove(dst_row, src_row, row_size);
			}

			DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->UnlockRect());
		}
		else
		{
			const bool can_copy_from_cpu = src_has_cpu_snapshot;
			LegacyLockedRect src_lock;
			::ZeroMemory(&src_lock, sizeof(src_lock));
			const unsigned char *src_mem = nullptr;
			unsigned int src_pitch = 0;
			if (can_copy_from_cpu) {
				src_mem = other->ImageData.Data.data() + srcy * other->ImageData.Pitch + srcx * pixel_size;
				src_pitch = other->ImageData.Pitch;
			} else {
				LegacyRect src_rect;
				src_rect.left = srcx;
				src_rect.right = srcx + copy_width;
				src_rect.top = srcy;
				src_rect.bottom = srcy + copy_height;
				DX8_ErrorCode(OTHER_NATIVE_COMPATIBILITY_SURFACE(other)->LockRect(&src_lock, &src_rect, kLegacyLockReadOnly));
				src_mem = static_cast<const unsigned char *>(src_lock.pBits);
				src_pitch = src_lock.Pitch;
			}

			if (dst_has_cpu_snapshot)
			{
				unsigned char *dst_mem = ImageData.Data.data() + dsty * ImageData.Pitch + dstx * pixel_size;
				for (unsigned int y = 0; y < copy_height; ++y)
				{
					memcpy(dst_mem, src_mem, row_size);
					src_mem += src_pitch;
					dst_mem += ImageData.Pitch;
				}
				if (!can_copy_from_cpu) {
					DX8_ErrorCode(OTHER_NATIVE_COMPATIBILITY_SURFACE(other)->UnlockRect());
				}
				Upload_CPU_Surface_Snapshot_To_Native_Compatibility_Surface();
				return;
			}

			LegacyRect dst_rect;
			dst_rect.left = dstx;
			dst_rect.right = dstx + copy_width;
			dst_rect.top = dsty;
			dst_rect.bottom = dsty + copy_height;

			LegacyLockedRect dst_lock;
			::ZeroMemory(&dst_lock, sizeof(dst_lock));
			DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->LockRect(&dst_lock, &dst_rect, 0));

			unsigned char *dst_mem = static_cast<unsigned char *>(dst_lock.pBits);
			for (unsigned int y = 0; y < copy_height; ++y)
			{
				memcpy(dst_mem, src_mem, row_size);
				src_mem += src_pitch;
				dst_mem += dst_lock.Pitch;
			}
			DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->UnlockRect());
			if (!can_copy_from_cpu) {
				DX8_ErrorCode(OTHER_NATIVE_COMPATIBILITY_SURFACE(other)->UnlockRect());
			}
		}
	}
	else
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		WWASSERT_PRINT(
			false,
			"SurfaceClass::Copy: standalone bgfx does not support legacy format-converting surface copies");
		return;
#else
		if (Should_Use_CPU_Only_Surface_Storage())
		{
			WWASSERT_PRINT(
				false,
				"SurfaceClass::Copy: BGFX surface ownership does not support legacy format-converting surface copies");
			return;
		}

		LegacyRect dest;
		dest.left=dstx;
		dest.right=dstx+width;
		dest.top=dsty;
		dest.bottom=dsty+height;

		if (dest.right>int(sd.Width)) dest.right=int(sd.Width);
		if (dest.bottom>int(sd.Height)) dest.bottom=int(sd.Height);

		Copy_Legacy_Surface(
			NATIVE_COMPATIBILITY_SURFACE,
			To_Legacy_Surface_Copy_Rect(dest),
			OTHER_NATIVE_COMPATIBILITY_SURFACE(other),
			To_Legacy_Surface_Copy_Rect(src),
			kLegacySurfaceCopyNoFilter);
#endif
	}
	Refresh_CPU_Surface_Snapshot_If_Present();
}

/***********************************************************************************************
 * SurfaceClass::Copy -- Copies a region from one surface to another                           *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   2/13/2001  hy : Created.                                                                  *
 *=============================================================================================*/
void SurfaceClass::Stretch_Copy(
	unsigned int dstx, unsigned int dsty, unsigned int dstwidth, unsigned int dstheight,
	unsigned int srcx, unsigned int srcy, unsigned int srcwidth, unsigned int srcheight,
	const SurfaceClass *other)
{
	WWASSERT(other);

	SurfaceDescription sd,osd;
	Get_Description(sd);
	const_cast <SurfaceClass*>(other)->Get_Description(osd);

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"SurfaceClass::Stretch_Copy: standalone bgfx does not support legacy stretched surface copies");
#else
	if (Should_Use_CPU_Only_Surface_Storage())
	{
		WWASSERT_PRINT(
			false,
			"SurfaceClass::Stretch_Copy: BGFX surface ownership does not support legacy stretched surface copies");
		return;
	}

	LegacyRect src;
	src.left=srcx;
	src.right=srcx+srcwidth;
	src.top=srcy;
	src.bottom=srcy+srcheight;

	LegacyRect dest;
	dest.left=dstx;
	dest.right=dstx+dstwidth;
	dest.top=dsty;
	dest.bottom=dsty+dstheight;

	Copy_Legacy_Surface(
		NATIVE_COMPATIBILITY_SURFACE,
		To_Legacy_Surface_Copy_Rect(dest),
		OTHER_NATIVE_COMPATIBILITY_SURFACE(other),
		To_Legacy_Surface_Copy_Rect(src),
		kLegacySurfaceCopyTriangleFilter);
	Refresh_CPU_Surface_Snapshot_If_Present();
#endif
}

/***********************************************************************************************
 * SurfaceClass::FindBB -- Finds the bounding box of non zero pixels in the region             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   2/13/2001  hy : Created.                                                                  *
 *=============================================================================================*/
void SurfaceClass::FindBB(Vector2i *min,Vector2i*max)
{
	SurfaceDescription sd;
	Get_Description(sd);

	WWASSERT(Has_Alpha(sd.Format));

	int alphabits=Alpha_Bits(sd.Format);
	int mask=0;
	switch (alphabits)
	{
	case 1: mask=1;
		break;
	case 4: mask=0xf;
		break;
	case 8: mask=0xff;
		break;
	}

	if (Has_Compatible_CPU_Surface_Snapshot(sd))
	{
		int x,y;
		unsigned int size=::Get_Bytes_Per_Pixel(sd.Format);
		Vector2i realmin=*max;
		Vector2i realmax=*min;

		for (y = min->J; y < max->J; y++) {
			for (x = min->I; x < max->I; x++) {
				const unsigned char *alpha =
					ImageData.Data.data() +
					y * ImageData.Pitch +
					x * size;
				unsigned char myalpha=alpha[size-1];
				myalpha=(myalpha>>(8-alphabits)) & mask;
				if (myalpha) {
					realmin.I = MIN(realmin.I, x);
					realmax.I = MAX(realmax.I, x);
					realmin.J = MIN(realmin.J, y);
					realmax.J = MAX(realmax.J, y);
				}
			}
		}

		*max=realmax;
		*min=realmin;
		return;
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"SurfaceClass::FindBB: standalone bgfx requires a CPU surface snapshot");
	return;
#else
	LegacyLockedRect lock_rect;
	::ZeroMemory(&lock_rect, sizeof(lock_rect));
	LegacyRect rect;
	::ZeroMemory(&rect, sizeof(rect));

	rect.bottom=max->J;
	rect.top=min->J;
	rect.left=min->I;
	rect.right=max->I;

	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->LockRect(&lock_rect,&rect,kLegacyLockReadOnly));

	int x,y;
	unsigned int size=::Get_Bytes_Per_Pixel(sd.Format);
	Vector2i realmin=*max;
	Vector2i realmax=*min;

	// the assumption here is that whenever a pixel has alpha it's in the MSB
	for (y = min->J; y < max->J; y++) {
		for (x = min->I; x < max->I; x++) {

			// HY - this is not endian safe
			unsigned char *alpha =
				static_cast<unsigned char *>(lock_rect.pBits) +
				(y - min->J) * lock_rect.Pitch +
				(x - min->I) * size;
			unsigned char myalpha=alpha[size-1];
			myalpha=(myalpha>>(8-alphabits)) & mask;
			if (myalpha) {
				realmin.I = MIN(realmin.I, x);
				realmax.I = MAX(realmax.I, x);
				realmin.J = MIN(realmin.J, y);
				realmax.J = MAX(realmax.J, y);
			}
		}
	}

	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->UnlockRect());

	*max=realmax;
	*min=realmin;
#endif
}


/***********************************************************************************************
 * SurfaceClass::Is_Transparent_Column -- Tests to see if the column is transparent or not     *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   2/13/2001  hy : Created.                                                                  *
 *=============================================================================================*/
bool SurfaceClass::Is_Transparent_Column(unsigned int column)
{
	SurfaceDescription sd;
	Get_Description(sd);

	WWASSERT(column<sd.Width);
	WWASSERT(Has_Alpha(sd.Format));

	int alphabits=Alpha_Bits(sd.Format);
	int mask=0;
	switch (alphabits)
	{
	case 1: mask=1;
		break;
	case 4: mask=0xf;
		break;
	case 8: mask=0xff;
		break;
	}

	unsigned int size=::Get_Bytes_Per_Pixel(sd.Format);

	if (Has_Compatible_CPU_Surface_Snapshot(sd))
	{
		for (int y = 0; y < (int) sd.Height; y++)
		{
			const unsigned char *alpha =
				ImageData.Data.data() +
				y * ImageData.Pitch +
				column * size;
			unsigned char myalpha=alpha[size-1];
			myalpha=(myalpha>>(8-alphabits)) & mask;
			if (myalpha) {
				return false;
			}
		}
		return true;
	}

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"SurfaceClass::Is_Transparent_Column: standalone bgfx requires a CPU surface snapshot");
	return true;
#else
	LegacyLockedRect lock_rect;
	::ZeroMemory(&lock_rect, sizeof(lock_rect));
	LegacyRect rect;
	::ZeroMemory(&rect, sizeof(rect));

	rect.bottom=sd.Height;
	rect.top=0;
	rect.left=column;
	rect.right=column+1;

	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->LockRect(&lock_rect,&rect,kLegacyLockReadOnly));

	int y;

	// the assumption here is that whenever a pixel has alpha it's in the MSB
	for (y = 0; y < (int) sd.Height; y++)
	{
		// HY - this is not endian safe
		unsigned char *alpha =
			static_cast<unsigned char *>(lock_rect.pBits) +
			y * lock_rect.Pitch;
		unsigned char myalpha=alpha[size-1];
		myalpha=(myalpha>>(8-alphabits)) & mask;
		if (myalpha) {
			DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->UnlockRect());
			return false;
		}
	}

	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->UnlockRect());
	return true;
#endif
}

/***********************************************************************************************
 * SurfaceClass::Get_Pixel -- Returns the pixel's RGB valus to the caller                      *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   2/13/2001  hy : Created.                                                                  *
 *   1/10/2025  TheSuperHackers : Added bits and pitch to argument list for better performance *
 *=============================================================================================*/
void SurfaceClass::Get_Pixel(Vector3 &rgb, int x, int y, LockedSurfacePtr pBits, int pitch)
{
	SurfaceDescription sd;
	Get_Description(sd);

	unsigned int bytesPerPixel = ::Get_Bytes_Per_Pixel(sd.Format);
	unsigned char* dst = static_cast<unsigned char *>(pBits) + y * pitch + x * bytesPerPixel;
	Convert_Pixel(rgb,sd,dst);
}

/***********************************************************************************************
 * SurfaceClass::Attach -- Attaches a surface pointer to the object, releasing the current ptr.*
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   3/27/2001  pds : Created.                                                                 *
 *=============================================================================================*/
void SurfaceClass::Attach_Native_Compatibility_Surface(void *surface)
{
	Detach ();
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		surface == nullptr,
		"SurfaceClass::Attach_Native_Compatibility_Surface: standalone bgfx cannot attach fake-D3D surfaces");
	NativeCompatibilitySurface = nullptr;
	return;
#else
	NativeCompatibilitySurface = static_cast<LegacySurface *>(surface);

	//
	//	Lock a reference onto the object
	//
	if (NativeCompatibilitySurface != nullptr) {
		NATIVE_COMPATIBILITY_SURFACE->AddRef ();
		Update_Description_From_Native_Compatibility_Surface();
	}
#endif
}

void SurfaceClass::Update_Description_From_Native_Compatibility_Surface()
{
	WWASSERT(NativeCompatibilitySurface != nullptr);

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"SurfaceClass::Update_Description_From_Native_Compatibility_Surface: standalone bgfx cannot read fake-D3D surface descriptions");
#else
	LegacySurfaceDesc d3d_desc;
	::ZeroMemory(&d3d_desc, sizeof(d3d_desc));
	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->GetDesc(&d3d_desc));

	Description.Format = D3DFormat_To_WW3DFormat(d3d_desc.Format);
	Description.Width = d3d_desc.Width;
	Description.Height = d3d_desc.Height;
	SurfaceFormat = Description.Format;
#endif
}

void SurfaceClass::Allocate_CPU_Surface_Snapshot()
{
	ImageData.Format = Description.Format;
	ImageData.Width = Description.Width;
	ImageData.Height = Description.Height;
	ImageData.Pitch = 0;
	ImageData.Data.clear();
	CPUImagePossiblyStale = false;

	if (!Can_Store_CPU_Surface_Data(Description)) {
		return;
	}

	const unsigned int pixel_size = ::Get_Bytes_Per_Pixel(Description.Format);
	const unsigned int row_size = Description.Width * pixel_size;
	ImageData.Pitch = row_size;
	ImageData.Data.resize(static_cast<size_t>(row_size) * Description.Height);
}

void SurfaceClass::Capture_CPU_Surface_Snapshot()
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		NativeCompatibilitySurface == nullptr,
		"SurfaceClass::Capture_CPU_Surface_Snapshot: standalone bgfx cannot read fake-D3D surface snapshots");
	return;
#else
	ImageData.Format = Description.Format;
	ImageData.Width = Description.Width;
	ImageData.Height = Description.Height;
	ImageData.Pitch = 0;
	ImageData.Data.clear();
	CPUImagePossiblyStale = false;

	if (!Should_Use_CPU_Surface_Snapshots() ||
		NativeCompatibilitySurface == nullptr ||
		!Can_Store_CPU_Surface_Data(Description)) {
		return;
	}

	const unsigned int pixel_size = ::Get_Bytes_Per_Pixel(Description.Format);
	const unsigned int row_size = Description.Width * pixel_size;
	ImageData.Pitch = row_size;
	ImageData.Data.resize(static_cast<size_t>(row_size) * Description.Height);

	LegacyLockedRect lock_rect;
	::ZeroMemory(&lock_rect, sizeof(lock_rect));
	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->LockRect(&lock_rect, nullptr, kLegacyLockReadOnly));

	const unsigned char *src = static_cast<const unsigned char *>(lock_rect.pBits);
	unsigned char *dst = ImageData.Data.data();
	for (unsigned int row = 0; row < Description.Height; ++row)
	{
		memcpy(dst, src, row_size);
		src += lock_rect.Pitch;
		dst += ImageData.Pitch;
	}

	DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->UnlockRect());
#endif
}

void SurfaceClass::Refresh_CPU_Surface_Snapshot_If_Present()
{
	if (Has_CPU_Surface_Snapshot()) {
		Capture_CPU_Surface_Snapshot();
	}
}

void SurfaceClass::Upload_CPU_Surface_Snapshot_To_Native_Compatibility_Surface()
{
	if (!Has_Compatible_CPU_Surface_Snapshot(Description)) {
		return;
	}

	if (NativeCompatibilitySurface != nullptr)
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		WWASSERT_PRINT(
			false,
			"SurfaceClass::Upload_CPU_Surface_Snapshot_To_Native_Compatibility_Surface: standalone bgfx cannot write fake-D3D surfaces");
#else
		const unsigned int pixel_size = ::Get_Bytes_Per_Pixel(Description.Format);
		const unsigned int row_size = Description.Width * pixel_size;

		LegacyLockedRect lock_rect;
		::ZeroMemory(&lock_rect, sizeof(lock_rect));
		DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->LockRect(&lock_rect, nullptr, 0));

		const unsigned char *src = ImageData.Data.data();
		unsigned char *dst = static_cast<unsigned char *>(lock_rect.pBits);
		for (unsigned int row = 0; row < Description.Height; ++row)
		{
			memcpy(dst, src, row_size);
			src += ImageData.Pitch;
			dst += lock_rect.Pitch;
		}

		DX8_ErrorCode(NATIVE_COMPATIBILITY_SURFACE->UnlockRect());
#endif
	}

	CPUImagePossiblyStale = false;
	if (TextureOwner != nullptr) {
		TextureOwner->Update_Surface_Level_From_Surface(TextureOwnerLevel, ImageData);
	}
}

void SurfaceClass::Ensure_CPU_Surface_Snapshot_Current()
{
	if (CPUImagePossiblyStale && Has_CPU_Surface_Snapshot()) {
		Capture_CPU_Surface_Snapshot();
	}
}

void SurfaceClass::Mark_CPU_Surface_Snapshot_Stale()
{
	if (Has_CPU_Surface_Snapshot()) {
		CPUImagePossiblyStale = true;
	}
}

void SurfaceClass::Attach_Texture_Level_Owner(TextureClass *texture, unsigned int level)
{
	if (TextureOwner != texture)
	{
		if (TextureOwner != nullptr) {
			TextureOwner->Release_Ref();
		}
		TextureOwner = texture;
		if (TextureOwner != nullptr) {
			TextureOwner->Add_Ref();
		}
	}
	TextureOwnerLevel = level;
}

bool SurfaceClass::Has_Compatible_CPU_Surface_Snapshot(const SurfaceDescription &desc) const
{
	SurfaceClass *self = const_cast<SurfaceClass *>(this);
	self->Ensure_CPU_Surface_Snapshot_Current();
	return self->Has_CPU_Surface_Snapshot() &&
		Should_Use_CPU_Surface_Snapshots() &&
		self->ImageData.Format == desc.Format &&
		self->ImageData.Width == desc.Width &&
		self->ImageData.Height == desc.Height;
}


/***********************************************************************************************
 * SurfaceClass::Detach -- Releases the reference on the internal surface ptr, and NULLs it.	 .*
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   3/27/2001  pds : Created.                                                                 *
 *=============================================================================================*/
void SurfaceClass::Detach ()
{
	//
	//	Release the hold we have on the legacy surface object
	//
	if (NativeCompatibilitySurface != nullptr) {
#if defined(GGC_RENDER_BACKEND_BGFX)
		WWASSERT_PRINT(
			false,
			"SurfaceClass::Detach: standalone bgfx cannot release fake-D3D surfaces");
#else
		NATIVE_COMPATIBILITY_SURFACE->Release ();
#endif
	}

	NativeCompatibilitySurface = nullptr;
	Description.Width = 0;
	Description.Height = 0;
	ImageData.Data.clear();
	ImageData.Width = 0;
	ImageData.Height = 0;
	ImageData.Pitch = 0;
	RefreshCPUAfterUnlock = false;
}


/***********************************************************************************************
 * SurfaceClass::DrawPixel -- draws a pixel                                                    *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   1/10/2025  TheSuperHackers : Added bits and pitch to argument list for better performance *
 *=============================================================================================*/
void SurfaceClass::Draw_Pixel(const unsigned int x, const unsigned int y, unsigned int color,
	unsigned int bytesPerPixel, LockedSurfacePtr pBits, int pitch)
{
	unsigned char* dst = static_cast<unsigned char*>(pBits) + y * pitch + x * bytesPerPixel;
	memcpy(dst, &color, bytesPerPixel);
}



/***********************************************************************************************
 * SurfaceClass::DrawHLine -- draws a horizontal line                                          *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   4/9/2001   hy : Created.                                                                  *
 *   1/10/2025  TheSuperHackers : Added bits and pitch to argument list for better performance *
 *=============================================================================================*/
void SurfaceClass::Draw_H_Line(const unsigned int y, const unsigned int x1, const unsigned int x2,
	unsigned int color, unsigned int bytesPerPixel, LockedSurfacePtr pBits, int pitch)
{
	unsigned char* row = static_cast<unsigned char*>(pBits) + y * pitch;

	for (unsigned int x = x1; x <= x2; ++x)
	{
		unsigned char* dst = row + x * bytesPerPixel;
		memcpy(dst, &color, bytesPerPixel);
	}
}


/***********************************************************************************************
 * SurfaceClass::Is_Monochrome -- Checks if surface is monochrome or not                       *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   7/5/2001   hy : Created.                                                                  *
 *=============================================================================================*/
bool SurfaceClass::Is_Monochrome()
{
	unsigned int x,y;
	SurfaceDescription sd;
	Get_Description(sd);
	bool is_compressed = false;

	switch (sd.Format)
	{
		// these formats are always monochrome
		case WW3D_FORMAT_A8L8:
		case WW3D_FORMAT_A8:
		case WW3D_FORMAT_L8:
		case WW3D_FORMAT_A4L4:
			return true;
		break;
		// these formats cannot be determined to be monochrome or not
		case WW3D_FORMAT_UNKNOWN:
		case WW3D_FORMAT_A8P8:
		case WW3D_FORMAT_P8:
		case WW3D_FORMAT_U8V8:		// Bumpmap
		case WW3D_FORMAT_L6V5U5:	// Bumpmap
		case WW3D_FORMAT_X8L8V8U8:	// Bumpmap
			return false;
		break;
		// these formats need decompression first
		case WW3D_FORMAT_DXT1:
		case WW3D_FORMAT_DXT2:
		case WW3D_FORMAT_DXT3:
		case WW3D_FORMAT_DXT4:
		case WW3D_FORMAT_DXT5:
			is_compressed = true;
		break;
	}

	// if it's in some compressed texture format, be sure to decompress first
	if (is_compressed) {
		WW3DFormat new_format = Get_Valid_Texture_Format(sd.Format, false);
		SurfaceClass *new_surf = NEW_REF( SurfaceClass, (sd.Width, sd.Height, new_format) );
		new_surf->Copy(0, 0, 0, 0, sd.Width, sd.Height, this);
		bool result = new_surf->Is_Monochrome();
		REF_PTR_RELEASE(new_surf);
		return result;
	}

	int pitch,size;

	size=::Get_Bytes_Per_Pixel(sd.Format);
	unsigned char *bits=(unsigned char*) Lock(&pitch);

	Vector3 rgb;
	bool mono=true;

	for (y=0; y<sd.Height; y++)
	{
		for (x=0; x<sd.Width; x++)
		{
			Convert_Pixel(rgb,sd,&bits[x*size]);
			mono&=(rgb.X==rgb.Y);
			mono&=(rgb.X==rgb.Z);
			mono&=(rgb.Z==rgb.Y);
			if (!mono)
			{
				Unlock();
				return false;
			}
		}
		bits+=pitch;
	}

	Unlock();

	return true;
}

/***********************************************************************************************
 * SurfaceClass::Hue_Shift -- changes the hue of the surface                                   *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   7/3/2001   hy : Created.                                                                  *
 *=============================================================================================*/
void SurfaceClass::Hue_Shift(const Vector3 &hsv_shift)
{
	unsigned int x,y;
	SurfaceDescription sd;
	Get_Description(sd);
	int pitch,size;

	size=::Get_Bytes_Per_Pixel(sd.Format);
	unsigned char *bits=(unsigned char*) Lock(&pitch);

	Vector3 rgb;

	for (y=0; y<sd.Height; y++)
	{
		for (x=0; x<sd.Width; x++)
		{
			Convert_Pixel(rgb,sd,&bits[x*size]);
			Recolor(rgb,hsv_shift);
			rgb.X=Bound(rgb.X,0.0f,1.0f);
			rgb.Y=Bound(rgb.Y,0.0f,1.0f);
			rgb.Z=Bound(rgb.Z,0.0f,1.0f);
			Convert_Pixel(&bits[x*size],sd,rgb);
		}
		bits+=pitch;
	}

	Unlock();
}
