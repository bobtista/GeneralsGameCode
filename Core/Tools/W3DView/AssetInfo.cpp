/*
**	Command & Conquer Renegade(tm)
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
 *                 Project Name : W3DView                                                      *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/Tools/W3DView/AssetInfo.cpp                  $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 2/25/00 4:16p                                               $*
 *                                                                                             *
 *                    $Revision:: 4                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include StdAfx.h (Windows-only)
#ifdef _WIN32
#include "StdAfx.h"
#endif
#include "AssetInfo.h"
//#include "HModel.h"
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef _WIN32
// TheSuperHackers @refactor bobtista 01/01/2025 Use GameEngineStubs for all platforms (Core build)
#include "GameEngineStubs.h"
// #include "assetmgr.h"  // Game engine header - not available in Core build
#include "htree.h"
#else
#include "GameEngineStubs.h"
#endif

/////////////////////////////////////////////////////////////////
//
//	Initialize
//
void
AssetInfoClass::Initialize (void)
{
	// If this isn't a material, then try to get its hierarchy name (if there is one)
	if (m_AssetType != TypeMaterial) {

		// Assume we are wrapping an instance as apposed to an asset 'name'.
		RenderObjClass *prender_obj = m_pRenderObj;
		if (prender_obj)
			prender_obj->Add_Ref();

		// If we are wrapping an asset name, then create an instance of it.
		if (prender_obj == NULL) {
#ifdef _WIN32
			prender_obj = WW3DAssetManager::Get_Instance()->Create_Render_Obj (m_Name);
#else
			prender_obj = WW3DAssetManager::Get_Instance()->Create_Render_Obj (m_Name.c_str());
#endif
		}

		if (prender_obj != NULL) {

			// Get the hierarchy tree for this object (if one exists)
			const HTreeClass *phtree = prender_obj->Get_HTree ();
			if (phtree) {

				// Get the name of the hierarchy tree
				m_HierarchyName = phtree->Get_Name ();
			}
		}

		// Release our hold on the temporary object
		REF_PTR_RELEASE (prender_obj);
	}

	return ;
}


