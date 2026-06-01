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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: TerrainTex.cpp ////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: TerrainTex.cpp
//
// Created:   John Ahlquist, April 2001
//
// Desc:      TextureClass overrides to perform custom texturing for the terrain.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//         Includes
//-----------------------------------------------------------------------------
#include <stdlib.h>

#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "W3DDevice/GameClient/TileData.h"
#include "Common/GlobalData.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/RenderBackend.h"

/******************************************************************************
						TerrainTextureClass
******************************************************************************/
static void InvalidateGeneratedTerrainTexture(TextureBaseClass *texture)
{
	if (g_renderBackend != nullptr)
	{
		// TheSuperHackers @bugfix bobtista 28/04/2026 Generated terrain
		// textures are populated through direct surface Lock writes.
		// Tell bgfx to re-upload after the atlas and mip chain are complete,
		// otherwise standalone can keep sampling an earlier partially-black
		// cache entry.
		g_renderBackend->Invalidate_Cached_Texture(texture);
	}
}

static RenderBackendTextureSampleFilter GetTerrainMinMagFilter()
{
	return (TheGlobalData && (TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex)) ?
		RB_TEXTURE_SAMPLE_LINEAR :
		RB_TEXTURE_SAMPLE_POINT;
}

static RenderBackendTextureSampleFilter GetTerrainMipFilter()
{
	return (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) ?
		RB_TEXTURE_SAMPLE_LINEAR :
		RB_TEXTURE_SAMPLE_POINT;
}

static void ApplyTerrainFilter(unsigned int stage)
{
	const RenderBackendTextureSampleFilter min_mag_filter = GetTerrainMinMagFilter();
	g_renderBackend->Set_Texture_Sample_Filter(stage, min_mag_filter, min_mag_filter, GetTerrainMipFilter());
}

static void SetTerrainTexcoordSource(unsigned int stage, unsigned int uv_index)
{
	g_renderBackend->Set_Texture_Coord_Source(stage, RB_TEXCOORD_MESH_UV, uv_index);
}

static void DisableTerrainTextureTransform(unsigned int stage)
{
	g_renderBackend->Set_Texture_Transform_Mode(stage, 0, false);
}

void TerrainTextureClass::UpdateTerrainAtlasRegions(WorldHeightMap *htMap, unsigned int textureWidth, unsigned int textureHeight, WW3DFormat textureFormat)
{
	Clear_Atlas_Regions();
	if (htMap == nullptr)
	{
		return;
	}

	const Int borderPixels = TILE_OFFSET / 2;
	if (textureFormat != WW3D_FORMAT_A1R5G5B5)
	{
		return;
	}

	for (Int texClass = 0; texClass < htMap->m_numTextureClasses; texClass++)
	{
		Int width = htMap->m_textureClasses[texClass].width * TILE_PIXEL_EXTENT;
		ICoord2D origin = htMap->m_textureClasses[texClass].positionInTexture;
		if (origin.x <= 0)
		{
			continue;
		}

		Int x = origin.x - borderPixels;
		Int y = origin.y - borderPixels;
		Int regionWidth = width + borderPixels * 2;
		Int regionHeight = width + borderPixels * 2;
		if (x < 0)
		{
			regionWidth += x;
			x = 0;
		}
		if (y < 0)
		{
			regionHeight += y;
			y = 0;
		}
		if (x + regionWidth > static_cast<Int>(textureWidth))
		{
			regionWidth = static_cast<Int>(textureWidth) - x;
		}
		if (y + regionHeight > static_cast<Int>(textureHeight))
		{
			regionHeight = static_cast<Int>(textureHeight) - y;
		}
		if (regionWidth <= 0 || regionHeight <= 0)
		{
			continue;
		}

		Add_Atlas_Region(static_cast<unsigned>(x), static_cast<unsigned>(y),
			static_cast<unsigned>(regionWidth), static_cast<unsigned>(regionHeight));
	}
}

void TerrainTextureClass::WriteTerrainAtlasMipLevel(WorldHeightMap *htMap, unsigned int level)
{
	if (htMap == nullptr || level == 0)
	{
		return;
	}

	const Int tilePixelExtent = TILE_PIXEL_EXTENT >> level;
	if (tilePixelExtent <= 0)
	{
		return;
	}

	MutableTextureMipView mip = Begin_Mip_Write(level);
	if (!mip.Is_Valid() || mip.Format != WW3D_FORMAT_A1R5G5B5)
	{
		return;
	}

	const Int surface_pitch = static_cast<Int>(mip.Pitch);
	UnsignedByte *surface_bits = mip.Data;

	const Int pixelBytes = 2;
	for (Int tileNdx = 0; tileNdx < htMap->m_numBitmapTiles; tileNdx++)
	{
		TileData *pTile = htMap->getSourceTile(tileNdx);
		if (!pTile)
		{
			continue;
		}
		UnsignedByte *pTileData = pTile->getRGBDataForWidth(tilePixelExtent);
		if (pTileData == nullptr)
		{
			continue;
		}

		ICoord2D position = pTile->m_tileLocationInTexture;
		if (position.x <= 0)
		{
			continue;
		}

		const Int mipColumn = position.x >> level;
		const Int mipRow = position.y >> level;
		for (Int j = 0; j < tilePixelExtent; j++)
		{
			const Int row = mipRow + j;
			if (row < 0 || row >= static_cast<Int>(mip.Height))
			{
				continue;
			}
			UnsignedByte *pBGR = pTileData + (tilePixelExtent - 1 - j) * TILE_BYTES_PER_PIXEL * tilePixelExtent;
			UnsignedByte *pBGRX = surface_bits + row * surface_pitch + mipColumn * pixelBytes;
			for (Int i = 0; i < tilePixelExtent; i++)
			{
				const Int column = mipColumn + i;
				if (column >= 0 && column < static_cast<Int>(mip.Width))
				{
					*((Short*)pBGRX) = 0x8000 + ((pBGR[2]>>3)<<10) + ((pBGR[1]>>3)<<5) + (pBGR[0]>>3);
				}
				pBGRX += pixelBytes;
				pBGR += TILE_BYTES_PER_PIXEL;
			}
		}
	}

	const Int borderPixels = ((TILE_OFFSET / 2) >> level) > 0 ? ((TILE_OFFSET / 2) >> level) : 1;
	for (Int texClass = 0; texClass < htMap->m_numTextureClasses; texClass++)
	{
		Int width = htMap->m_textureClasses[texClass].width * tilePixelExtent;
		ICoord2D origin = htMap->m_textureClasses[texClass].positionInTexture;
		if (origin.x <= 0)
		{
			continue;
		}

		origin.x >>= level;
		origin.y >>= level;
		if (origin.x - borderPixels < 0
			|| origin.y - borderPixels < 0
			|| origin.x + width + borderPixels > static_cast<Int>(mip.Width)
			|| origin.y + width + borderPixels > static_cast<Int>(mip.Height))
		{
			continue;
		}

		for (Int y = 0; y < width; y++)
		{
			UnsignedByte *row = surface_bits + (origin.y + y) * surface_pitch;
			for (Int b = 1; b <= borderPixels; b++)
			{
				memcpy(row + (origin.x - b) * pixelBytes,
					   row + (origin.x + width - b) * pixelBytes,
					   pixelBytes);
			}
			for (Int b = 0; b < borderPixels; b++)
			{
				memcpy(row + (origin.x + width + b) * pixelBytes,
					   row + (origin.x + b) * pixelBytes,
					   pixelBytes);
			}
		}

		const Int copyBytes = (width + borderPixels * 2) * pixelBytes;
		for (Int b = 1; b <= borderPixels; b++)
		{
			UnsignedByte *dst = surface_bits + (origin.y - b) * surface_pitch + (origin.x - borderPixels) * pixelBytes;
			UnsignedByte *src = surface_bits + (origin.y + width - b) * surface_pitch + (origin.x - borderPixels) * pixelBytes;
			memcpy(dst, src, copyBytes);
		}
		for (Int b = 0; b < borderPixels; b++)
		{
			UnsignedByte *dst = surface_bits + (origin.y + width + b) * surface_pitch + (origin.x - borderPixels) * pixelBytes;
			UnsignedByte *src = surface_bits + (origin.y + b) * surface_pitch + (origin.x - borderPixels) * pixelBytes;
			memcpy(dst, src, copyBytes);
		}
	}

	End_Mip_Write(level);
}

void TerrainTextureClass::WriteTerrainAtlasMipLevels(WorldHeightMap *htMap)
{
	const unsigned int mipCount = Get_Level_Count();
	for (unsigned int level = 1; level < mipCount; level++)
	{
		WriteTerrainAtlasMipLevel(htMap, level);
	}
}

//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// TerrainTextureClass::TerrainTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to create a 16 bit per pixel D3D
texture of the desired height and mip level. */
//=============================================================================
TerrainTextureClass::TerrainTextureClass(int height) :
	TextureClass(TEXTURE_WIDTH, height,
		WW3D_FORMAT_A1R5G5B5, MIP_LEVELS_3 )
{
}

//=============================================================================
// TerrainTextureClass::TerrainTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to create a 16 bit per pixel D3D
texture of the desired height and mip level. */
//=============================================================================
TerrainTextureClass::TerrainTextureClass(int height, int width) :
	TextureClass(width, height,
		WW3D_FORMAT_A1R5G5B5, MIP_LEVELS_ALL )
{
}


//=============================================================================
// TerrainTextureClass::update
//=============================================================================
/** Sets the tile bitmap data into the texture.  The tiles are placed with 4
	pixel borders around them, so that when the tiles are scaled and bilinearly
	interpolated, you don't get seams between the tiles.  */
//=============================================================================
int TerrainTextureClass::update(WorldHeightMap *htMap)
{
	MutableTextureMipView mip = Begin_Mip_Write(0);
	if (!mip.Is_Valid()) {
		return 0;
	}
	if (mip.Width < TEXTURE_WIDTH) {
		return 0;
	}

	const Int surface_pitch = static_cast<Int>(mip.Pitch);
	UnsignedByte *surface_bits = mip.Data;

	Int tilePixelExtent = TILE_PIXEL_EXTENT;
	Int tilesPerRow = mip.Width/(2*TILE_PIXEL_EXTENT+TILE_OFFSET);
	tilesPerRow *= 2;
//	Int numRows = surface_desc.Height/(tilePixelExtent+TILE_OFFSET);
#ifdef RTS_DEBUG
	//DEBUG_ASSERTCRASH(tilesPerRow*numRows >= htMap->m_numBitmapTiles, ("Too many tiles."));
	DEBUG_ASSERTCRASH((Int)mip.Width >= tilePixelExtent*tilesPerRow, ("Bitmap too small."));
#endif
	if (mip.Format == WW3D_FORMAT_A1R5G5B5) {
#if 0
		UnsignedInt cellX, cellY;
		for (cellX = 0; cellX < surface_desc.Width; cellX++) {
			for (cellY = 0; cellY < surface_desc.Height; cellY++) {
				UnsignedByte *pBGR = ((UnsignedByte *)locked_rect.pBits)+(cellY*surface_desc.Width+cellX)*2;
				*((Short*)pBGR) = (((255-2*cellY)>>3)<<10) + ((4*cellX)>>4);
			}
		}
#endif
		Int tileNdx;
		Int pixelBytes = 2;
		for (tileNdx=0; tileNdx < htMap->m_numBitmapTiles; tileNdx++) {
			TileData *pTile = htMap->getSourceTile(tileNdx);
			if (!pTile) continue;
			ICoord2D position = pTile->m_tileLocationInTexture;
			if (position.x<=0) continue; // all real tile offsets start at 2.  jba.

			Int i,j;
			for (j=0; j<tilePixelExtent; j++) {
				UnsignedByte *pBGR = pTile->getRGBDataForWidth(tilePixelExtent);
				pBGR += (tilePixelExtent-1-j)*TILE_BYTES_PER_PIXEL*tilePixelExtent; // invert to match.
				Int row = position.y+j;
				UnsignedByte *pBGRX = surface_bits + row * surface_pitch;

				Int column = position.x;
				pBGRX += column*pixelBytes;
				for (i=0; i<tilePixelExtent; i++) {
					*((Short*)pBGRX) = 0x8000 + ((pBGR[2]>>3)<<10) + ((pBGR[1]>>3)<<5) + (pBGR[0]>>3);
					pBGRX +=pixelBytes;
					pBGR +=TILE_BYTES_PER_PIXEL;
				}
			}
		}
		// Now draw the 4 pixel border around each tile class.
		Int texClass;
		for (texClass=0; texClass<htMap->m_numTextureClasses; texClass++) {
			Int width = htMap->m_textureClasses[texClass].width;
			ICoord2D origin = htMap->m_textureClasses[texClass].positionInTexture;
			if (origin.x<=0) continue;
			width *= TILE_PIXEL_EXTENT;
			// Duplicate 4 columns of pixels before and after.
			Int j;
			for (j=0; j<width; j++) {
				Int row = origin.y+j;
				UnsignedByte *pBGRX = surface_bits + row * surface_pitch;

				Int column = origin.x;
				pBGRX += column*pixelBytes;
				// copy before
				memcpy(pBGRX-(4)*pixelBytes, pBGRX+(width-4)*pixelBytes, 4*pixelBytes);
				// copy after
				memcpy(pBGRX+(width*pixelBytes), pBGRX, 4*pixelBytes);
			}

			// Duplicate 4 rows of pixels before and after.
			for (j=0; j<4; j++) {
				// copy before.
				Int row = origin.y-j-1;
				UnsignedByte *pBGRX = surface_bits + row * surface_pitch;
				UnsignedByte *target = pBGRX+(origin.x-4)*pixelBytes;
				memcpy(target, target + width * surface_pitch, (width+8)*pixelBytes);
				// copy after.
				row = origin.y+j;
				pBGRX = surface_bits + row * surface_pitch;
				target = pBGRX+(origin.x-4)*pixelBytes;
				memcpy(target + width * surface_pitch, target, (width+8)*pixelBytes);
			}

		}

	}
	End_Mip_Write(0);
	Generate_Mip_Levels();
	UpdateTerrainAtlasRegions(htMap, mip.Width, mip.Height, mip.Format);
	WriteTerrainAtlasMipLevels(htMap);
	InvalidateGeneratedTerrainTexture(this);
	if (WW3D::Get_Texture_Reduction()) {
		Set_LOD(WW3D::Get_Texture_Reduction());
	}
	return(mip.Height);
}

//=============================================================================
// TerrainTextureClass::setLOD
//=============================================================================
/** Sets the lod of the texture to be loaded into the video card.  */
//=============================================================================
void TerrainTextureClass::setLOD(Int LOD)
{
	Set_LOD(static_cast<unsigned int>(LOD));
}
//=============================================================================
// TerrainTextureClass::update
//=============================================================================
/** Sets the tile bitmap data into the texture.  The tiles are placed with 4
	pixel borders around them, so that when the tiles are scaled and bilinearly
	interpolated, you don't get seams between the tiles.  */
//=============================================================================
Bool TerrainTextureClass::updateFlat(WorldHeightMap *htMap, Int xCell, Int yCell, Int cellWidth, Int pixelsPerCell)
{
	MutableTextureMipView mip = Begin_Mip_Write(0);
	if (!mip.Is_Valid())
	{
		return false;
	}

	DEBUG_ASSERTCRASH((Int)mip.Width == cellWidth*pixelsPerCell, ("Bitmap too small."));
	DEBUG_ASSERTCRASH((Int)mip.Height == cellWidth*pixelsPerCell, ("Bitmap too small."));
	if (mip.Width != cellWidth*pixelsPerCell) {
		return false;
	}

	const Int surface_pitch = static_cast<Int>(mip.Pitch);
	UnsignedByte *surface_bits = mip.Data;

	if (mip.Format == WW3D_FORMAT_A1R5G5B5) {

		Int pixelBytes = 2;
		Int cellX, cellY;
#if 0
		UnsignedInt X, Y;
		for (X = 0; X < surface_desc.Width; X++) {
			for (Y = 0; Y < surface_desc.Height; Y++) {
				UnsignedByte *pBGR = ((UnsignedByte *)locked_rect.pBits)+(Y*surface_desc.Width+X)*pixelBytes;
				*((Short*)pBGR) = (((255-2*Y)>>3)<<10) + ((2*X)>>4);
			}
		}
#endif
		for (cellX = 0; cellX < cellWidth; cellX++) {
			for (cellY = 0; cellY < cellWidth; cellY++) {
				UnsignedByte *pBGR = htMap->getPointerToTileData(xCell+cellX, yCell+cellY, pixelsPerCell);
				if (pBGR == nullptr) continue; // past end of defined terrain. [3/24/2003]
				Int k, l;
				for (k=pixelsPerCell-1; k>=0; k--) {
					UnsignedByte *pBGRX = surface_bits + (pixelsPerCell*(cellWidth-cellY-1)+k)*surface_pitch +
						cellX*pixelsPerCell*pixelBytes;
					for (l=0; l<pixelsPerCell; l++) {
						*((Short*)pBGRX) = 0x8000 + ((pBGR[2]>>3)<<10) + ((pBGR[1]>>3)<<5) + (pBGR[0]>>3);
						pBGRX +=pixelBytes;
						pBGR +=TILE_BYTES_PER_PIXEL;
					}
				}
			}
		}
	}

	End_Mip_Write(0);
	Generate_Mip_Levels();
	InvalidateGeneratedTerrainTexture(this);
	return(mip.Height);
}

//=============================================================================
// TerrainTextureClass::Apply
//=============================================================================
/** Sets the texture as the current texture, and does some custom setup
(standard D3D setup, but beyond the scope of W3D).  */
//=============================================================================
void TerrainTextureClass::Apply(unsigned int stage)
{
	// Do the base apply.
	TextureClass::Apply(stage);
}

/******************************************************************************
						AlphaTerrainTextureClass
******************************************************************************/
//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// AlphaTerrainTextureClass::AlphaTerrainTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to create a throwaway 8x8 texture,
then shares the base texture resource. This way the base tiles pass, drawn
using TerrainTextureClass shares the same texture with the blended edges pass,
saving lots of texture memory, and preventing seams between blended tiles. */
//=============================================================================
AlphaTerrainTextureClass::AlphaTerrainTextureClass( TextureClass *pBaseTex ):
	TextureClass(8, 8,
		WW3D_FORMAT_A1R5G5B5, MIP_LEVELS_1 )
{
	Copy_Atlas_Regions_From(pBaseTex);
	Share_Texture_Storage_With(pBaseTex);
}


//=============================================================================
// AlphaTerrainTextureClass::Apply
//=============================================================================
/** Sets the texture as the current texture, and does some custom setup.
This may be applied in either single pass, as the second texture in the pipe,
or multipass.  If stage==0, we are doing multipass and we set up the pipe
for a single texture.  If stage==1, then we are doing a single pass, and we
set up the pipe so that we blend onto the base texture in stage 0.
(standard D3D setup, but beyond the scope of W3D). */
//=============================================================================
void AlphaTerrainTextureClass::Apply(unsigned int stage)
{
	// Do the base apply.
	TextureClass::Apply(stage);

	// Since we are using multiple distinct tiles, the textures doesn't wrap, so clamp it.
	ApplyTerrainFilter(stage);
	g_renderBackend->Set_Texture_Address_Mode(0, RB_TEXTURE_ADDRESS_CLAMP, RB_TEXTURE_ADDRESS_CLAMP, RB_TEXTURE_ADDRESS_WRAP);
	// Now setup the texture pipeline.
	if (stage==0) {
		// Modulate the diffuse color with the texture as lighting comes from diffuse.
		g_renderBackend->Set_Texture_Color_Argument(0, 1, RB_TEXARG_TEXTURE);
		g_renderBackend->Set_Texture_Color_Argument(0, 2, RB_TEXARG_DIFFUSE);
		g_renderBackend->Set_Texture_Color_Operation(0, RB_TEXOP_MODULATE);
		g_renderBackend->Set_Texture_Alpha_Operation(0, RB_TEXOP_MODULATE);
		SetTerrainTexcoordSource(0, 1);
		// Blend the result using the alpha. (came from diffuse mod texture)
		g_renderBackend->Set_Alpha_Blend_Enable(true);
		g_renderBackend->Set_Blend_Factors(RB_BLEND_SRC_ALPHA, RB_BLEND_INV_SRC_ALPHA);
		// Disable stage 2.
		g_renderBackend->Set_Texture_Color_Operation(1, RB_TEXOP_DISABLE);
		g_renderBackend->Set_Texture_Alpha_Operation(1, RB_TEXOP_DISABLE);
	}	else if (stage==1) {

		if (TheGlobalData && !TheGlobalData->m_multiPassTerrain)
		{
			///@todo: Remove 8-Stage Nvidia hack after drivers are fixed.
			//This method is a backdoor specific to Nvidia based cards.  It will fail on
			//other hardware.  Allows single pass blend of 2 textures and post modulate diffuse.
			SetTerrainTexcoordSource(0, 0);
			g_renderBackend->Set_Texture_Color_Argument(0, 1, RB_TEXARG_TEXTURE);
			g_renderBackend->Set_Texture_Color_Argument(0, 2, RB_TEXARG_DIFFUSE);
			g_renderBackend->Set_Texture_Color_Operation(0, RB_TEXOP_MODULATE);
			g_renderBackend->Set_Texture_Alpha_Argument(0, 1, RB_TEXARG_TEXTURE);
			g_renderBackend->Set_Texture_Alpha_Argument(0, 2, RB_TEXARG_DIFFUSE);
			g_renderBackend->Set_Texture_Alpha_Operation(0, RB_TEXOP_MODULATE);

			SetTerrainTexcoordSource(1, 1);
			g_renderBackend->Set_Texture_Color_Argument(1, 1, RB_TEXARG_DIFFUSE | RB_TEXARG_COMPLEMENT | RB_TEXARG_ALPHAREPLICATE);
			g_renderBackend->Set_Texture_Color_Argument(1, 2, RB_TEXARG_DIFFUSE);
			g_renderBackend->Set_Texture_Color_Operation(1, RB_TEXOP_ADD);
			g_renderBackend->Set_Texture_Alpha_Argument(1, 1, RB_TEXARG_TFACTOR | RB_TEXARG_COMPLEMENT);
			g_renderBackend->Set_Texture_Alpha_Argument(1, 2, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Alpha_Operation(1, RB_TEXOP_ADD);

			g_renderBackend->Set_Texture(2, nullptr);
			SetTerrainTexcoordSource(2, 2);
			g_renderBackend->Set_Texture_Color_Argument(2, 1, RB_TEXARG_TEXTURE);
			g_renderBackend->Set_Texture_Color_Argument(2, 2, RB_TEXARG_TEXTURE);
			g_renderBackend->Set_Texture_Color_Operation(2, RB_TEXOP_MODULATE);
			g_renderBackend->Set_Texture_Alpha_Argument(2, 1, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Alpha_Argument(2, 2, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Alpha_Operation(2, RB_TEXOP_MODULATE);

			g_renderBackend->Set_Texture(3, nullptr);
			SetTerrainTexcoordSource(3, 3);
			g_renderBackend->Set_Texture_Color_Argument(3, 1, RB_TEXARG_DIFFUSE | RB_TEXARG_ALPHAREPLICATE);
			g_renderBackend->Set_Texture_Color_Argument(3, 2, RB_TEXARG_DIFFUSE);
			g_renderBackend->Set_Texture_Color_Operation(3, RB_TEXOP_SELECTARG1);
			g_renderBackend->Set_Texture_Alpha_Argument(3, 1, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Alpha_Argument(3, 2, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Alpha_Operation(3, RB_TEXOP_SELECTARG1);

			g_renderBackend->Set_Texture(4, nullptr);
			SetTerrainTexcoordSource(4, 4);
			g_renderBackend->Set_Texture_Color_Argument(4, 1, RB_TEXARG_CURRENT);
			g_renderBackend->Set_Texture_Color_Argument(4, 2, RB_TEXARG_DIFFUSE);
			g_renderBackend->Set_Texture_Color_Operation(4, RB_TEXOP_MODULATE);
			g_renderBackend->Set_Texture_Alpha_Argument(4, 1, RB_TEXARG_CURRENT);
			g_renderBackend->Set_Texture_Alpha_Argument(4, 2, RB_TEXARG_DIFFUSE);
			g_renderBackend->Set_Texture_Alpha_Operation(4, RB_TEXOP_MODULATE);

			g_renderBackend->Set_Texture(5, nullptr);
			SetTerrainTexcoordSource(5, 5);
			g_renderBackend->Set_Texture_Color_Argument(5, 1, RB_TEXARG_DIFFUSE);
			g_renderBackend->Set_Texture_Color_Argument(5, 2, RB_TEXARG_DIFFUSE);
			g_renderBackend->Set_Texture_Color_Operation(5, RB_TEXOP_ADD);
			g_renderBackend->Set_Texture_Alpha_Argument(5, 1, RB_TEXARG_TFACTOR | RB_TEXARG_COMPLEMENT);
			g_renderBackend->Set_Texture_Alpha_Argument(5, 2, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Alpha_Operation(5, RB_TEXOP_ADD);

			g_renderBackend->Set_Texture(6, nullptr);
			SetTerrainTexcoordSource(6, 6);
			g_renderBackend->Set_Texture_Color_Argument(6, 1, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Color_Argument(6, 2, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Color_Operation(6, RB_TEXOP_MODULATE);
			g_renderBackend->Set_Texture_Alpha_Argument(6, 1, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Alpha_Argument(6, 2, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Alpha_Operation(6, RB_TEXOP_MODULATE);

			g_renderBackend->Set_Texture(7, nullptr);
			SetTerrainTexcoordSource(7, 7);
			g_renderBackend->Set_Texture_Color_Argument(7, 1, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Color_Argument(7, 2, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Color_Operation(7, RB_TEXOP_SELECTARG1);
			g_renderBackend->Set_Texture_Alpha_Argument(7, 1, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Alpha_Argument(7, 2, RB_TEXARG_TFACTOR);
			g_renderBackend->Set_Texture_Alpha_Operation(7, RB_TEXOP_SELECTARG1);
		}
		else
		{
			g_renderBackend->Set_Texture_Color_Argument(0, 1, RB_TEXARG_TEXTURE);
			g_renderBackend->Set_Texture_Color_Operation(0, RB_TEXOP_SELECTARG1);
			g_renderBackend->Set_Texture_Alpha_Argument(0, 1, RB_TEXARG_TEXTURE);
			g_renderBackend->Set_Texture_Alpha_Operation(0, RB_TEXOP_SELECTARG1);

			g_renderBackend->Set_Texture_Color_Argument(1, 1, RB_TEXARG_TEXTURE);
			g_renderBackend->Set_Texture_Color_Argument(1, 2, RB_TEXARG_CURRENT);
			g_renderBackend->Set_Texture_Color_Operation(1, RB_TEXOP_MODULATE);
			g_renderBackend->Set_Texture_Alpha_Argument(1, 1, RB_TEXARG_TEXTURE);
			g_renderBackend->Set_Texture_Alpha_Operation(1, RB_TEXOP_SELECTARG1);
		}
	}
}


/******************************************************************************
						LightMapTerrainTextureClass
******************************************************************************/
//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// LightMapTerrainTextureClass::LightMapTerrainTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to load the .tga texture. */
//=============================================================================
LightMapTerrainTextureClass::LightMapTerrainTextureClass(AsciiString name, MipCountType mipLevelCount) :
TextureClass(name.isEmpty()?"TSNoiseUrb.tga":name.str(),name.isEmpty()?"TSNoiseUrb.tga":name.str(), mipLevelCount )
{
	Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_BEST);
	Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_BEST);
	Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_REPEAT);
	Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_REPEAT);
}

#define STRETCH_FACTOR ((float)(1/(63.0*MAP_XY_FACTOR/2))) /* covers 63/2 tiles */

//=============================================================================
// LightMapTerrainTextureClass::Apply
//=============================================================================
/** Sets the texture as the current texture, and does some custom setup.
The LightMapTerrainTextureClass may be applied by itself, or with the
CloudMapTerrainTextureClass.  This may be applied in either single pass,
as the second texture in the pipe,
or multipass.  If stage==0, we are doing multipass and we set up the pipe
for a single texture.  If stage==1, then we are doing a single pass, and we
set up the pipe so that we blend onto the cloud map texture in stage 0.
Also, texture is mapped using the x/y coordinates of the map, saving us
yet another set of uv coordinates.
(standard D3D setup, but beyond the scope of W3D). */
//=============================================================================
void LightMapTerrainTextureClass::Apply(unsigned int stage)
{
	TextureClass::Apply(stage);
}









/******************************************************************************
						AlphaEdgeTextureClass
******************************************************************************/
//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

/**
* AlphaEdgeTextureClass - Generates the alpha edge blending for terrain.
*
*/
AlphaEdgeTextureClass::AlphaEdgeTextureClass( int height, MipCountType mipLevelCount) :
//	TextureClass("EdgingTemplate.tga","EdgingTemplate.tga", mipLevelCount )
	TextureClass(TEXTURE_WIDTH, height, WW3D_FORMAT_A8R8G8B8, mipLevelCount )
{

}

int AlphaEdgeTextureClass::update256(WorldHeightMap *htMap)
{
	return 1;
}

int AlphaEdgeTextureClass::update(WorldHeightMap *htMap)
{
	MutableTextureMipView mip = Begin_Mip_Write(0);
	if (!mip.Is_Valid())
	{
		return 0;
	}

	const Int surface_pitch = static_cast<Int>(mip.Pitch);
	UnsignedByte *surface_bits = mip.Data;
	Int tilePixelExtent = TILE_PIXEL_EXTENT; // blend tiles are 1/4 tiles.
//	Int tilesPerRow = surface_desc.Width / (tilePixelExtent+8);

//	Int numRows = surface_desc.Height/(tilePixelExtent+8);

	if (mip.Format == WW3D_FORMAT_A8R8G8B8) {
#if 1
#if 1
		Int cellX, cellY;
		for (cellX = 0; (UnsignedInt)cellX < mip.Width; cellX++) {
			for (cellY = 0; cellY < mip.Height; cellY++) {
				UnsignedByte *pBGR = surface_bits + cellY * surface_pitch + cellX * 4;
				pBGR[2] = 255-cellY/2;
				pBGR[0] = cellX/2;
				pBGR[3] = cellX/2;  // alpha.
				pBGR[3] = 128;  // alpha.
			}
		}
#endif
#if 1
		Int tileNdx;
		Int pixelBytes = 4;
		for (tileNdx=0; tileNdx < htMap->m_numEdgeTiles; tileNdx++) {
			TileData *pTile = htMap->getEdgeTile(tileNdx);
			if (!pTile) continue;
			ICoord2D position = pTile->m_tileLocationInTexture;
			if (position.x<=0) continue; // all real edge offsets start at 4.  jba.
			Int i,j;
			Int column = position.x;
			for (j=0; j<tilePixelExtent; j++) {
				Int row = position.y+j;
				UnsignedByte *pBGR = htMap->getEdgeTile(tileNdx)->getRGBDataForWidth(tilePixelExtent);
				pBGR += (tilePixelExtent-1-j)*TILE_BYTES_PER_PIXEL*tilePixelExtent; // invert to match.
				UnsignedByte *pBGRX = surface_bits + row * surface_pitch;
				pBGRX += column*pixelBytes;

				for (i=0; i<tilePixelExtent; i++) {
					pBGRX[0] = pBGR[0];  //r
					pBGRX[1] = pBGR[1];	//g
					pBGRX[2] = pBGR[2];	//b
					if (pBGR[0]==0 && pBGR[1]==0 && pBGR[2]==0) {
						pBGRX[3] = 0x80;
					} else if (pBGR[0]==0xff && pBGR[1]==0xff && pBGR[2]==0xff) {
						pBGRX[3] = 0x00;
					}	else {
						pBGRX[3] = 0xff;
					}

					pBGRX += pixelBytes;
					pBGR += TILE_BYTES_PER_PIXEL;
				}
			}
		}
#endif
#endif
	}
	End_Mip_Write(0);
	Generate_Mip_Levels();
	InvalidateGeneratedTerrainTexture(this);
	return(mip.Height);
}

void AlphaEdgeTextureClass::Apply(unsigned int stage)
{
	// Do the base apply.
	TextureClass::Apply(stage);
}


/******************************************************************************
						CloudMapTerrainTextureClass
******************************************************************************/
//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// CloudMapTerrainTextureClass::CloudMapTerrainTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to load the .tga texture, and sets
up the "sliding" parameters for the clouds to slide over the terrain. */
//=============================================================================
//@todo - Allow adjustment of the cloud slide rate, and lose the hard coded "cloudmap.tga"
CloudMapTerrainTextureClass::CloudMapTerrainTextureClass(MipCountType mipLevelCount) :
	TextureClass("TSCloudMed.tga","TSCloudMed.tga", mipLevelCount )
{
	Get_Filter().Set_Mip_Mapping( TextureFilterClass::FILTER_TYPE_FAST );
	m_xSlidePerSecond = -0.02f;
	m_ySlidePerSecond =  1.50f * m_xSlidePerSecond;
	m_curTick = 0;
	m_xOffset = 0;
	m_yOffset = 0;

}

//=============================================================================
// CloudMapTerrainTextureClass::Apply
//=============================================================================
/** Sets the texture as the current texture, and does some custom setup.
The CloudMapTerrainTextureClass may be applied by itself, or with the
LightMapTerrainTexture.  This may be applied in either single pass,
as the first texture in the pipe with LightMapTerrainTextureClass as the
second stage of the pape, or multipass.  We setup for stage 0, assuming that
we are the only texture, as LightMapTerrainTexture will adjust for multitexture
if it is applied to stage 1.
Also, texture is mapped using the x/y coordinates of the map, saving us
yet another set of uv coordinates.
(standard D3D setup, but beyond the scope of W3D). */
//=============================================================================
void CloudMapTerrainTextureClass::Apply(unsigned int stage)
{


	// Do the base apply.
	TextureClass::Apply(stage);
}

//=============================================================================
// CloudMapTerrainTextureClass::restore
//=============================================================================
/** Cleans up any custom settings to the texturing pipeline that may not be
understood by w3d. */
//=============================================================================
void CloudMapTerrainTextureClass::restore()
{
	g_renderBackend->Set_Texture_Color_Argument(0, 1, RB_TEXARG_TEXTURE);
	g_renderBackend->Set_Texture_Color_Argument(0, 2, RB_TEXARG_DIFFUSE);
	g_renderBackend->Set_Texture_Color_Operation(0, RB_TEXOP_MODULATE);
	g_renderBackend->Set_Texture_Alpha_Operation(0, RB_TEXOP_DISABLE);

	g_renderBackend->Set_Texture_Address_Mode(0, RB_TEXTURE_ADDRESS_WRAP, RB_TEXTURE_ADDRESS_WRAP, RB_TEXTURE_ADDRESS_WRAP);
	SetTerrainTexcoordSource(0, 0);
	DisableTerrainTextureTransform(0);

	g_renderBackend->Set_Texture_Color_Argument(1, 1, RB_TEXARG_TEXTURE);
	g_renderBackend->Set_Texture_Color_Argument(1, 2, RB_TEXARG_DIFFUSE);
	g_renderBackend->Set_Texture_Color_Operation(1, RB_TEXOP_MODULATE);
	g_renderBackend->Set_Texture_Alpha_Operation(1, RB_TEXOP_DISABLE);

	g_renderBackend->Set_Texture_Address_Mode(1, RB_TEXTURE_ADDRESS_WRAP, RB_TEXTURE_ADDRESS_WRAP, RB_TEXTURE_ADDRESS_WRAP);
	SetTerrainTexcoordSource(1, 0);
	DisableTerrainTextureTransform(1);
	g_renderBackend->Set_Alpha_Blend_Enable(false);
	g_renderBackend->Set_Blend_Factors(RB_BLEND_SRC_ALPHA, RB_BLEND_INV_SRC_ALPHA);


	if (TheGlobalData && !TheGlobalData->m_multiPassTerrain)
	{
		///@todo: Remove 8-Stage Nvidia hack after drivers are fixed.
		//This method is a backdoor specific to Nvidia based cards.  It will fail on
		//other hardware.  Allows single pass blend of 2 textures and post modulate diffuse.
		Int i;
		for (i=0; i<8; i++) {
			g_renderBackend->Set_Texture_Color_Operation(i, RB_TEXOP_DISABLE);
			SetTerrainTexcoordSource(i, i);
			g_renderBackend->Set_Texture_Color_Argument(i, 1, RB_TEXARG_TEXTURE);
			g_renderBackend->Set_Texture_Color_Argument(i, 2, RB_TEXARG_DIFFUSE);
			g_renderBackend->Set_Texture_Alpha_Argument(i, 1, RB_TEXARG_TEXTURE);
			g_renderBackend->Set_Texture_Alpha_Argument(i, 2, RB_TEXARG_DIFFUSE);
			g_renderBackend->Set_Texture_Alpha_Operation(i, RB_TEXOP_DISABLE);

			g_renderBackend->Set_Texture(i, nullptr);
		}
	}
}

/******************************************************************************
						ScorchTextureClass
******************************************************************************/
//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// ScorchTextureClass::ScorchTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to load the .tga texture. */
//=============================================================================
/// @todo - get "EXScorch01.tga" from not hard coded location.
ScorchTextureClass::ScorchTextureClass(MipCountType mipLevelCount) :
	TextureClass("EXScorch01.tga","EXScorch01.tga", mipLevelCount )
// Hack to disable texture reduction.
//	TextureClass("EXScorch01.tga","EXScorch01.tga", mipLevelCount,WW3D_FORMAT_UNKNOWN,true,false)
{
}

//=============================================================================
// ScorchTextureClass::Apply
//=============================================================================
/** Sets the texture as the current texture, and does some custom setup.
The ScorchTextureClass is applied by iteself, as it's mesh is a subset of the
terrain mesh.
(standard D3D setup, but beyond the scope of W3D). */
//=============================================================================
void ScorchTextureClass::Apply(unsigned int stage)
{
	// Do the base apply.
	TextureClass::Apply(stage);
	// Setup bilinear or trilinear filtering as specified in global data.
	ApplyTerrainFilter(stage);

	DisableTerrainTextureTransform(0);
	g_renderBackend->Set_Texture_Address_Mode(0, RB_TEXTURE_ADDRESS_CLAMP, RB_TEXTURE_ADDRESS_CLAMP, RB_TEXTURE_ADDRESS_WRAP);
	// Now setup the texture pipeline.

	g_renderBackend->Set_Texture_Color_Argument(0, 1, RB_TEXARG_TEXTURE);
	g_renderBackend->Set_Texture_Color_Argument(0, 2, RB_TEXARG_DIFFUSE);
	g_renderBackend->Set_Texture_Color_Operation(0, RB_TEXOP_MODULATE);
	g_renderBackend->Set_Texture_Alpha_Operation(0, RB_TEXOP_SELECTARG1);
	SetTerrainTexcoordSource(0, 0);
	g_renderBackend->Set_Alpha_Blend_Enable(true);
	g_renderBackend->Set_Blend_Factors(RB_BLEND_SRC_ALPHA, RB_BLEND_INV_SRC_ALPHA);

	g_renderBackend->Set_Texture_Color_Operation(1, RB_TEXOP_DISABLE);
	g_renderBackend->Set_Texture_Alpha_Operation(1, RB_TEXOP_DISABLE);
}
