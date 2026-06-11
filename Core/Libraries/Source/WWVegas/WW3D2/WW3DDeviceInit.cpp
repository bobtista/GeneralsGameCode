/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "always.h"

#include "WW3DDeviceInit.h"

#include "FixedFunctionState.h"
#include "boxrobj.h"
#include "dx8indexbuffer.h"
#include "dx8renderer.h"
#include "dx8vertexbuffer.h"
#include "missingtexture.h"
#include "pointgr.h"
#include "shattersystem.h"
#include "shdlib.h"
#include "sortingrenderer.h"
#include "texturefilter.h"
#include "textureloader.h"
#include "vertmaterial.h"
#include "ww3d.h"

void WW3DDeviceInit::Init_Subsystems()
{
	MissingTexture::_Init();
	TextureFilterClass::_Init_Filters(
		(TextureFilterClass::TextureFilterMode)WW3D::Get_Texture_Filter(),
		(TextureFilterClass::AnisotropicFilterMode)WW3D::Get_Anisotropy_Level()
	);
	TheDX8MeshRenderer.Init();
	SHD_INIT;
	BoxRenderObjClass::Init();
	VertexMaterialClass::Init();
	PointGroupClass::_Init(); // This needs the VertexMaterialClass to be initted
	ShatterSystem::Init();
	TextureLoader::Init();
}

void WW3DDeviceInit::Shutdown_Subsystems()
{
	FixedFunctionState::Release_Render_State();

	TextureLoader::Deinit();
	SortingRendererClass::Deinit();
	DynamicVBAccessClass::_Deinit();
	DynamicIBAccessClass::_Deinit();
	ShatterSystem::Shutdown();
	PointGroupClass::_Shutdown();
	VertexMaterialClass::Shutdown();
	BoxRenderObjClass::Shutdown();
	SHD_SHUTDOWN;
	TheDX8MeshRenderer.Shutdown();
	MissingTexture::_Deinit();
}
