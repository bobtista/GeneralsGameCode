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
 *                     $Archive:: /Commando/Code/Tools/W3DView/ViewerScene.h                  $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 3/14/01 6:41p                                               $*
 *                                                                                             *
 *                    $Revision:: 6                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef HAVE_WWVEGAS
    // Use real WWVegas headers when available (Generals/GeneralsMD builds)
    #include "scene.h"
    #include "aabox.h"
    #include "sphere.h"
    #include "robjlist.h"  // For RefRenderObjListClass typedef
#else
    // Use stubs for Core-only build
    #include "GameEngineStubs.h"
#endif


class RenderObjIterator;

///////////////////////////////////////////////////////////////////////////
//
//	ViewerSceneClass
//
// TheSuperHackers @refactor bobtista 01/01/2025 Use GameEngineStubs for all platforms (Core build)
// ViewerSceneClass inherits from SceneClass (stubbed in GameEngineStubs.h)
class ViewerSceneClass : public SceneClass
{
	public:

		///////////////////////////////////////////////////////////////////
		//
		//	Public constructors/destructors
		//
		ViewerSceneClass (void)
			: m_AllowLODSwitching (false)		{ }

		virtual ~ViewerSceneClass (void)		{ }


		///////////////////////////////////////////////////////////////////
		//
		//	Public methods
		//

		//
		//	Overrides from SimpleSceneClass
		//
		virtual void				Visibility_Check (CameraClass *pcamera);
		virtual void				Add_Render_Object(RenderObjClass * obj);
		virtual void				Remove_Render_Object(RenderObjClass * obj);  // Added for ViewerScene.cpp
		virtual void				Customized_Render(RenderInfoClass & rinfo);

		//
		//	Inline accessors
		//
		virtual void				Allow_LOD_Switching (bool onoff)			{ m_AllowLODSwitching = onoff; }
		virtual bool				Are_LODs_Switching (void)					{ return m_AllowLODSwitching; }

		//
		// General methods
		//
		virtual void				Add_To_Lineup (RenderObjClass *obj);
		virtual void				Clear_Lineup (void);
		virtual AABoxClass		Get_Line_Up_Bounding_Box (void);
		bool							Can_Line_Up (RenderObjClass *obj);
		bool							Can_Line_Up (int class_id);
#ifndef _WIN32
		// Stub implementations for non-Windows (defined in ViewerScene_Stubs.cpp)
#endif
		void							Recalculate_Fog_Planes (void);
		virtual SphereClass		Get_Bounding_Sphere (void);
		// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementations for Core build (SceneClass methods stubbed)
		void							Get_Fog_Range(float* near_range, float* far_range);
		void							Set_Fog_Range(float near_range, float far_range);

		//
		// Line-Up list iteration
		//
		virtual SceneIterator *	Create_Line_Up_Iterator (void);
		virtual void				Destroy_Line_Up_Iterator (SceneIterator *iterator);
		// TheSuperHackers @refactor bobtista 01/01/2025 SimpleSceneClass is stubbed, so we need Create_Iterator and Destroy_Iterator on all platforms
		virtual SceneIterator *	Create_Iterator (void);
		virtual void				Destroy_Iterator (SceneIterator *iterator);

	private:

		///////////////////////////////////////////////////////////////////
		//
		//	Private member data
		//
		bool							m_AllowLODSwitching;
		RefRenderObjListClass	LineUpList;
		RefRenderObjListClass	LightList;
		// TheSuperHackers @refactor bobtista 01/01/2025 For Core build, ViewerSceneClass needs RenderList and Visibility_Checked
		// (SceneClass methods are stubbed, so we need our own member variables)
		RefRenderObjListClass	RenderList;
		bool							Visibility_Checked;
};
