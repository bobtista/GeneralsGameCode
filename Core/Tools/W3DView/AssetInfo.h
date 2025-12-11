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
 *                     $Archive:: /Commando/Code/Tools/W3DView/AssetInfo.h                    $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 3/09/99 2:50p                                               $*
 *                                                                                             *
 *                    $Revision:: 2                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef HAVE_WWVEGAS
    // Use real WWVegas headers when available (Generals/GeneralsMD builds)
    #include "rendobj.h"  // For RenderObjClass
    #include "assetmgr.h" // For WW3DAssetManager
    #include "htree.h"    // For HTreeClass
#else
    // Use stubs for Core-only build
    #include "GameEngineStubs.h"
    #ifndef _WIN32
    #include <string>  // For std::string
    #endif
#endif
// Windows type stubs (ensure DWORD is defined even when Qt is present)
#ifndef DWORD
typedef unsigned long DWORD;
#endif
// LPCTSTR is defined by windows.h when HAVE_WWVEGAS is defined - don't redefine
#if !defined(LPCTSTR) && !defined(_WINDEF_) && !defined(_WINUSER_) && !defined(_WINDOWS_)
typedef const char* LPCTSTR;
#endif
// CString: when HAVE_WWVEGAS is defined, use StringClass; otherwise use QString or std::string
#ifndef CString
#ifdef HAVE_WWVEGAS
    // StringClass is defined in WWVegas
    class StringClass;
    typedef StringClass CString;
#elif defined(QT_VERSION)
    #include <QString>
    typedef QString CString;
#else
    #include <string>
    typedef std::string CString;
#endif
#endif
#ifndef NULL
#define NULL nullptr
#endif
#include "Utils.h"
#include "AssetTypes.h"


/////////////////////////////////////////////////////////////////////////////
//
//  AssetInfoClass
//
//		Class used by the data tree to identify each individual
//		entry in the tree.
//
class AssetInfoClass
{
	public:

		//////////////////////////////////////////////////////////////
		//
		//  Public constructors/destructors
		//
		AssetInfoClass (void)
			: m_AssetType (TypeUnknown),
			  m_dwUserData (0L),
			  m_pRenderObj (NULL)			{ Initialize (); }

		AssetInfoClass (LPCTSTR passet_name, ASSET_TYPE type, RenderObjClass *prender_obj = NULL, DWORD user_data = 0L)
			: m_Name (passet_name),
			  m_AssetType (type),
			  m_dwUserData (user_data),
			  m_pRenderObj (NULL)			{ REF_PTR_SET (m_pRenderObj, prender_obj); Initialize (); }

		virtual ~AssetInfoClass (void)	{ REF_PTR_RELEASE (m_pRenderObj); }

		//////////////////////////////////////////////////////////////
		//
		//  Public methods
		//

		//
		//  Inline accessors
		//
		const CString &	Get_Name (void) const						{ return m_Name; }
		const CString &	Get_Hierarchy_Name (void) const			{ return m_HierarchyName; }
		const CString &	Get_Original_Name (void) const			{ return m_OriginalName; }
		ASSET_TYPE			Get_Type (void) const						{ return m_AssetType; }
		DWORD					Get_User_Number (void) const				{ return m_dwUserData; }
		const CString &	Get_User_String (void) const				{ return m_UserString; }
		RenderObjClass *	Get_Render_Obj (void) const				{ if (m_pRenderObj) m_pRenderObj->Add_Ref(); return m_pRenderObj; }
		RenderObjClass *	Peek_Render_Obj (void) const				{ return m_pRenderObj; }
		void					Set_Name (LPCTSTR pname)					{ m_Name = pname; }
		void					Set_Hierarchy_Name (LPCTSTR pname)		{ m_HierarchyName = pname; }
		void					Set_Type (ASSET_TYPE type)					{ m_AssetType = type; }
		void					Set_User_Number (DWORD user_data)		{ m_dwUserData = user_data; }
		void					Set_User_String (LPCTSTR string)			{ m_UserString = string; }
		void					Set_Render_Obj (RenderObjClass *pobj)	{ REF_PTR_SET (m_pRenderObj, pobj); }

		//
		//	Information methods
		//
		// TheSuperHackers @refactor bobtista 01/01/2025 Use cross-platform string length method
		#ifdef HAVE_WWVEGAS
		bool					Can_Asset_Have_Animations (void) const	{ return bool(m_HierarchyName.Get_Length() > 0); }
		#elif defined(QT_VERSION)
		bool					Can_Asset_Have_Animations (void) const	{ return bool(m_HierarchyName.length() > 0); }
		#else
		bool					Can_Asset_Have_Animations (void) const	{ return bool(m_HierarchyName.length() > 0); }
		#endif

	protected:

		//////////////////////////////////////////////////////////////
		//
		//  Protected methods
		//
		void					Initialize (void);


	private:

		//////////////////////////////////////////////////////////////
		//
		//  Private member data
		//
		CString				m_Name;
		CString				m_HierarchyName;
		CString				m_UserString;
		CString				m_OriginalName;
		ASSET_TYPE			m_AssetType;
		DWORD					m_dwUserData;
		RenderObjClass *	m_pRenderObj;
};
