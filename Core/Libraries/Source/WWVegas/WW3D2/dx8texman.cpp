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
 *                 Project Name : DX8 Texture Manager                                          *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx8texman.cpp                          $*
 *                                                                                             *
 *              Original Author:: Hector Yee                                                   *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/27/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 4                                                           $*
 *                                                                                             *
 * 06/27/02 KM Texture class abstraction																			*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   DX8TextureManagerClass::Shutdown -- Shuts down the texture manager                        *
 *   DX8TextureManagerClass::Add -- Adds a texture to be managed                               *
 *   DX8TextureManagerClass::Remove -- Removes a texture from being managed                    *
 *   DX8TextureManagerClass::Release_Textures -- Releases the internal d3d texture             *
 *   DX8TextureManagerClass::Recreate_Textures -- Reallocates lost textures                    *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


// This class manages textures that are in the default pool
// ensuring that they are released on device loss
// and created on device reset

// Note: It does NOT addref to textures because it is called in the texture
// destructor

#include "dx8texman.h"
#include "dx8wrapper.h"

void DX8TextureTrackerClass::Recreate() const
{
	WWASSERT(Texture->Peek_D3D_Base_Texture()==nullptr);
	Texture->Poke_Texture
	(
		DX8Wrapper::_Create_DX8_Texture
		(
			Width,
			Height,
			Format,
			Mip_level_count,
			D3DPOOL_DEFAULT,
			RenderTarget
		)
	);
}

void DX8ZTextureTrackerClass::Recreate() const
{
	WWASSERT(Texture->Peek_D3D_Base_Texture()==nullptr);
	Texture->Poke_Texture
	(
		DX8Wrapper::_Create_DX8_ZTexture
		(
			Width,
			Height,
			ZFormat,
			Mip_level_count,
			D3DPOOL_DEFAULT
		)
	);
}
