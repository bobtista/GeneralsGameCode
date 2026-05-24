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

// FILE: W3DWater.cpp /////////////////////////////////////////////////////////////////////////////
// Created:   Mark Wilczynski, June 2001
// Desc:      Draw reflective water surface.  Also handles drawing of waves/ripples
//			  on the surface.
///////////////////////////////////////////////////////////////////////////////////////////////////

#define SCROLL_UV

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////

#include "W3DDevice/GameClient/W3DWater.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "W3DDevice/GameClient/W3DWaterTracks.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "texture.h"
#include "assetmgr.h"
#include "rinfo.h"
#include "camera.h"
#include "scene.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/RenderBackend.h"
#include "WW3D2/renderdebugstats.h"
#include "WW3D2/statistics.h"
#include "light.h"
#include "simplevec.h"
#include "mesh.h"
#include "matinfo.h"
#include "WW3D2/dx8fvf.h"
#include "WW3D2/indexbuffer.h"
#include "WW3D2/renderbufferclasses.h"
#include "WW3D2/vertexbuffer.h"

#include "Common/FramePacer.h"
#include "Common/GameState.h"
#include "Common/GlobalData.h"
#include "Common/PerfTimer.h"
#include "Common/Xfer.h"
#include "Common/GameLOD.h"

#include "GameClient/Color.h"
#include "GameClient/Water.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptEngine.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DPoly.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "W3DDevice/GameClient/W3DCustomScene.h"



// TheSuperHackers @refactor bobtista 19/04/2026 Helper to bind
// a texture to both D3D8 and g_renderBackend so bgfx's texture cache stays
// in sync. For raw D3D8 textures without a TextureClass*, pass nullptr for
// tex to clear the bgfx cache (prevents stale texture artifacts).
static inline void W3DWater_BindTexture(unsigned stage, TextureClass * tex)
{
	if (g_renderBackend != nullptr)
		g_renderBackend->Bind_Texture_Immediate(stage, tex);
}

static inline bool W3DWater_UseBackendSeaBatch()
{
	return g_renderBackend != nullptr && g_renderBackend->Has_Shader_Pipeline();
}

static void W3DWater_FillWhiteTexture(TextureClass *texture)
{
	if (texture == nullptr)
	{
		return;
	}

	TextureClass::MutableTextureMipView mip = texture->Begin_Mip_Write(0);
	if (!mip.Is_Valid())
	{
		return;
	}

	if (mip.Format == WW3D_FORMAT_A4R4G4B4)
	{
		*reinterpret_cast<UnsignedShort *>(mip.Data) = 0xffff;
	}
	else if (mip.Format == WW3D_FORMAT_A8R8G8B8)
	{
		*reinterpret_cast<UnsignedInt *>(mip.Data) = 0xffffffff;
	}
	texture->End_Mip_Write(0);
}

static inline void W3DWater_SetTextureTransform(unsigned stage, const Matrix4x4 & matrix)
{
	if (g_renderBackend != nullptr)
		g_renderBackend->Set_Texture_Transform(stage, matrix);
}

static inline Matrix4x4 W3DWater_MakeScaleTextureMatrix(float sx, float sy, float sz)
{
	return Matrix4x4(
		sx, 0.0f, 0.0f, 0.0f,
		0.0f, sy, 0.0f, 0.0f,
		0.0f, 0.0f, sz, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
}

static inline Matrix4x4 W3DWater_MakeTranslationTextureMatrix(float x, float y, float z)
{
	return Matrix4x4(
		1.0f, 0.0f, 0.0f, x,
		0.0f, 1.0f, 0.0f, y,
		0.0f, 0.0f, 1.0f, z,
		0.0f, 0.0f, 0.0f, 1.0f);
}

static inline void W3DWater_SetNoiseTextureTransform(unsigned stage, float repeat, float origin)
{
	if (g_renderBackend == nullptr)
		return;

	Matrix4x4 view;
	g_renderBackend->Get_Transform(RB_TRANSFORM_VIEW, view);
	const Matrix4x4 destMatrix =
		W3DWater_MakeTranslationTextureMatrix(origin, origin, 0.0f) *
		W3DWater_MakeScaleTextureMatrix(repeat, repeat, 1.0f) *
		view.Inverse();
	W3DWater_SetTextureTransform(stage, destMatrix);
}

static inline void W3DWater_DisableTextureTransform(unsigned stage)
{
	if (g_renderBackend != nullptr)
		g_renderBackend->Set_Texture_Transform_Mode(stage, 0, false);
}

static inline void W3DWater_SetCameraSpaceTexcoord2(unsigned stage, unsigned uv_index)
{
	if (g_renderBackend != nullptr) {
		g_renderBackend->Set_Texture_Coord_Source(stage, RB_TEXCOORD_CAMERA_SPACE_POSITION, uv_index);
		g_renderBackend->Set_Texture_Transform_Mode(stage, 2, false);
	}
}

static inline void W3DWater_ResetMeshTexcoord(unsigned stage, unsigned uv_index)
{
	if (g_renderBackend != nullptr) {
		g_renderBackend->Set_Texture_Coord_Source(stage, RB_TEXCOORD_MESH_UV, uv_index);
		g_renderBackend->Set_Texture_Transform_Mode(stage, 0, false);
	}
}

static inline void W3DWater_SetStageAddress2D(unsigned stage, RenderBackendTextureAddressMode address_mode)
{
	g_renderBackend->Set_Texture_Address_Mode(stage, address_mode, address_mode, RB_TEXTURE_ADDRESS_WRAP);
}

static inline void W3DWater_SetStageMinMagFilter(unsigned stage,
	RenderBackendTextureSampleFilter min_filter,
	RenderBackendTextureSampleFilter mag_filter)
{
	g_renderBackend->Set_Texture_Min_Mag_Filter(stage, min_filter, mag_filter);
}

static inline void W3DWater_SetStageMipFilter(unsigned stage, RenderBackendTextureSampleFilter mip_filter)
{
	g_renderBackend->Set_Texture_Mip_Filter(stage, mip_filter);
}

#define MIPMAP_BUMP_TEXTURE

// DEFINES ////////////////////////////////////////////////////////////////////////////////////////
#define SKYPLANE_SIZE	(384.0f*MAP_XY_FACTOR)
#define SKYPLANE_HEIGHT	(30.0f)

#define SKYBODY_TEXTURE	"TSMoonLarg.tga"
#define SKYBODY_SIZE	45.0f		//extent or radius of sky body

#define SKYBODY_X	150.0f	//location of skybody
#define SKYBODY_Y	550.0f	//location of skybody

/* in the bay
#define SKYBODY_X	120.0f			//location of skybody
#define SKYBODY_Y	75.0f			//location of skybody
*/

#define SKYBODY_HEIGHT	SKYPLANE_HEIGHT	//altitude of sky body (z-buffer disabled, so can equal sky height).

//GeForce3 water system defines
#define PATCH_SIZE 15		//number of vertices on patch edge.  Large patches may waste vertices off edge of screen.
#define PATCH_UV_TILES	42	//number of times the bump map texture is tiled across patch (must be integer!).
#define PATCH_SCALE (4.0f * MAP_XY_FACTOR)	//horizontal scale factor. Adjust this and size to get desired vertex density.
#define SEA_REFLECTION_SIZE 256		//dimensions of reflection texture

#define SEA_BUMP_SCALE		(0.06f)		//scales the du/dv offsets stored in bump map (~ amount to perturb)
#define BUMP_SIZE (50.f)
#define REFLECTION_FACTOR 0.1f

#define PATCH_WIDTH (PATCH_SIZE-1)	//internal defines
#define PATCH_UV_SCALE	((Real)PATCH_UV_TILES/(Real)PATCH_WIDTH)

//3D Grid Mesh Water defines.
#define WATER_MESH_OPACITY		0.5f
#define WATER_MESH_X_VERTICES	128
#define WATER_MESH_Y_VERTICES	128
#define WATER_MESH_SPACING	MAP_XY_FACTOR	//same as terrain

#ifdef USE_MESH_NORMALS
#define WATER_MESH_FVF	DX8_FVF_XYZNDUV2
typedef VertexFormatXYZNDUV2 MaterMeshVertexFormat;
#else
#define WATER_MESH_FVF	DX8_FVF_XYZDUV2
typedef VertexFormatXYZDUV2 MaterMeshVertexFormat;
#endif

#define DRAW_WATER_WAKES
/// @todo: Fix clipping of objects that intersect the mirror surface
//#define CLIP_GEOMETRY_TO_PLANE	// this enables clipping of objects that intersect the mirror surfaces

// Some shader combinations that can be useful in rendering water:

// Modulate stage0 with stage1 texture.  Also modulate stage 0 with vertex color.
#define SC_DETAIL_BLEND ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE,\
	ShaderClass::SRCBLEND_SRC_ALPHA,ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, \
	ShaderClass::TEXTURING_ENABLE, 	ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, ShaderClass::DETAILCOLOR_DETAILBLEND, ShaderClass::DETAILALPHA_DISABLE) )

// Just a z-buffer fill, nothing is written to the color buffer.
#define SC_ZFILL_BLEND ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_DISABLE, ShaderClass::SRCBLEND_ZERO, \
	ShaderClass::DSTBLEND_ONE, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::DETAILCOLOR_SCALE, ShaderClass::DETAILALPHA_DISABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, \
	ShaderClass::DETAILCOLOR_SCALE, ShaderClass::DETAILALPHA_DISABLE) )

// No texturing, just vertex color with vertex alpha
#define SC_ZFILL_BLENDx ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE, \
	ShaderClass::SRCBLEND_ZERO, ShaderClass::DSTBLEND_SRC_COLOR, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, \
	ShaderClass::TEXTURING_DISABLE, ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

// Modulate blended with vertex alpha modulation
#define SC_ZFILL_MODULATE_TEX ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE,\
	ShaderClass::SRCBLEND_ZERO, ShaderClass::DSTBLEND_SRC_COLOR, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, \
	ShaderClass::TEXTURING_ENABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_DISABLE, ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

// Alpha blended with vertex alpha modulation
#define SC_ZFILL_ALPHA_TEX ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE,\
	ShaderClass::SRCBLEND_SRC_ALPHA, ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_DISABLE, ShaderClass::SECONDARY_GRADIENT_DISABLE, \
	ShaderClass::TEXTURING_ENABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_DISABLE, ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

// Alpha blended with vertex alpha modulation
#define SC_OPAQUE_TEXONLY ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE,\
	ShaderClass::SRCBLEND_ONE, ShaderClass::DSTBLEND_ZERO, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_DISABLE, ShaderClass::SECONDARY_GRADIENT_DISABLE, \
	ShaderClass::TEXTURING_ENABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_DISABLE, ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

// Alpha blended with vertex alpha modulation
#define SC_ZFILL_BLEND3 ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE,\
	ShaderClass::SRCBLEND_SRC_ALPHA, ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, \
	ShaderClass::TEXTURING_ENABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_DISABLE, ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

static ShaderClass zFillAlphaShader(SC_ZFILL_BLEND3);
static ShaderClass blendStagesShader(SC_DETAIL_BLEND);

WaterRenderObjClass *TheWaterRenderObj=nullptr; ///<global water rendering object

static Int getRiverVertexDiffuse(W3DShroud *shroud, Real x, Real y, Real shadeR, Real shadeG, Real shadeB, Int diffuse)
{
	if (!shroud)
		return diffuse;

	Int cellX = (Int)(x / shroud->getCellWidth());
	Int cellY = (Int)(y / shroud->getCellHeight());
	W3DShroudLevel level = shroud->getShroudLevel(cellX, cellY);
	Real shroudScale = (Real)level / 255.0f;
	return GameMakeColor(
		(Int)(shadeR * shroudScale),
		(Int)(shadeG * shroudScale),
		(Int)(shadeB * shroudScale),
		((diffuse >> 24) & 0xff) * shroudScale);
}

void doSkyBoxSet(Bool startDraw)
{
	if (TheWritableGlobalData)
		TheWritableGlobalData->m_drawSkyBox = startDraw;
}


#define DONUT_SIDES	90
#define INNER_RADIUS 200.0f
#define OUTER_RADIUS 250.0f
#define TEXTURE_REPEAT_COUNT 16
#define DONUT_HEIGHT	15.0f
//#define DO_FLAT_DONUT
#define AMP_SCALE	(30.0f/120.0f)
#define WAVE_FREQ	0.3f
#define AMP_SCALE2	(10.0f/120.0f)
#define NOISE_FREQ	(2.0f*PI/WAVE_FREQ)

#define NOISE_REPEAT_FACTOR ((float)(1.0f/(16.0f)))


static Bool wireframeForDebug = 0;

void WaterRenderObjClass::setupJbaWaterShader()
{
	if (!TheWaterTransparency->m_additiveBlend)
		g_renderBackend->Set_Shader(ShaderClass::_PresetAlphaShader);
	else
		g_renderBackend->Set_Shader(ShaderClass::_PresetAdditiveShader);

	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	g_renderBackend->Set_Material(vmat);
	REF_PTR_RELEASE(vmat);
	m_riverTexture->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_BEST);
	m_riverTexture->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_BEST);
	m_riverTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_BEST);


//	Setting *setting=&m_settings[m_tod];


	g_renderBackend->Apply_Render_State_Changes();	//force update of view and projection matrices
	g_renderBackend->Set_Texture_Alpha_Operation(0, RB_TEXOP_ADD);
	if (!m_riverAlphaEdge->Is_Initialized())
		m_riverAlphaEdge->Init();
	W3DWater_BindTexture(3, m_riverAlphaEdge);
	W3DWater_SetStageAddress2D(3, RB_TEXTURE_ADDRESS_WRAP);
	g_renderBackend->Set_Texture_Coord_Source(0, RB_TEXCOORD_MESH_UV, 0);
	g_renderBackend->Set_Texture_Coord_Source(1, RB_TEXCOORD_MESH_UV, 0);
	g_renderBackend->Set_Texture_Coord_Source(3, RB_TEXCOORD_MESH_UV, 1);

	Bool doSparkles = true;

	if (m_riverWaterPixelShader && doSparkles) {
		if (!m_waterSparklesTexture->Is_Initialized())
			m_waterSparklesTexture->Init();
		W3DWater_BindTexture(1, m_waterSparklesTexture);

		if (!m_waterNoiseTexture->Is_Initialized())
			m_waterNoiseTexture->Init();
		W3DWater_BindTexture(2, m_waterNoiseTexture);

		W3DWater_SetStageAddress2D(1, RB_TEXTURE_ADDRESS_WRAP);

		// Two output coordinates are used.
		W3DWater_SetCameraSpaceTexcoord2(2, 0);
		W3DWater_SetStageAddress2D(2, RB_TEXTURE_ADDRESS_WRAP);

		W3DWater_SetNoiseTextureTransform(2, NOISE_REPEAT_FACTOR, m_riverVOrigin);

	}
	W3DWater_SetStageMinMagFilter(0, RB_TEXTURE_SAMPLE_LINEAR, RB_TEXTURE_SAMPLE_LINEAR);
	W3DWater_SetStageMinMagFilter(1, RB_TEXTURE_SAMPLE_LINEAR, RB_TEXTURE_SAMPLE_LINEAR);
	W3DWater_SetStageMinMagFilter(2, RB_TEXTURE_SAMPLE_LINEAR, RB_TEXTURE_SAMPLE_LINEAR);
	W3DWater_SetStageMinMagFilter(3, RB_TEXTURE_SAMPLE_LINEAR, RB_TEXTURE_SAMPLE_LINEAR);
	if (m_riverWaterPixelShader){
		const float reflectionFactor[4] = { REFLECTION_FACTOR, REFLECTION_FACTOR, REFLECTION_FACTOR, 1.0f };
		g_renderBackend->Set_Pixel_Shader_Constant(0, &reflectionFactor, 1);
		g_renderBackend->Set_Pixel_Shader(m_riverWaterPixelShader);
	}
}




//-------------------------------------------------------------------------------------------------
/** Destructor. Releases w3d assets. */
//-------------------------------------------------------------------------------------------------
WaterRenderObjClass::~WaterRenderObjClass()
{
	REF_PTR_RELEASE(m_meshVertexMaterialClass);
	REF_PTR_RELEASE(m_vertexMaterialClass);
	REF_PTR_RELEASE(m_meshLight);
	REF_PTR_RELEASE(m_alphaClippingTexture);
	REF_PTR_RELEASE (m_skyBox);

	REF_PTR_RELEASE (m_riverTexture);
	REF_PTR_RELEASE (m_whiteTexture);
	REF_PTR_RELEASE (m_waterNoiseTexture);
	REF_PTR_RELEASE (m_riverAlphaEdge);
	REF_PTR_RELEASE (m_waterSparklesTexture);

	Int i;

	for(i=0; i<TIME_OF_DAY_COUNT; i++)
	{	REF_PTR_RELEASE(m_settings[i].skyTexture);
		REF_PTR_RELEASE(m_settings[i].waterTexture);
	}

	delete [] m_meshData;
	m_meshData = nullptr;
	m_meshDataSize = 0;

	//Release strings allocated inside global water settings.
	for  (i=0; i<TIME_OF_DAY_COUNT; i++)
	{	WaterSettings[i].m_skyTextureFile.clear();
		WaterSettings[i].m_waterTextureFile.clear();
	}
	deleteInstance((WaterTransparencySetting*)TheWaterTransparency.getNonOverloadedPointer());
	TheWaterTransparency = nullptr;
	ReleaseResources();

	delete m_waterTrackSystem;
}

//-------------------------------------------------------------------------------------------------
/** Constructor. Just nulls out some variables. */
//-------------------------------------------------------------------------------------------------
WaterRenderObjClass::WaterRenderObjClass()
{
	memset( &m_settings, 0, sizeof( m_settings ) );
	m_dx=0;
	m_dy=0;
	m_indexBuffer=nullptr;
	m_waterMeshIndexBuffer=nullptr;
	m_waterTrackSystem = nullptr;
	m_doWaterGrid = FALSE;
	m_meshVertexMaterialClass=nullptr;
	m_meshLight=nullptr;
	m_vertexMaterialClass=nullptr;
	m_alphaClippingTexture=nullptr;
	m_useCloudLayer=true;
	m_waterType = WATER_TYPE_0_TRANSLUCENT;
	m_tod=TIME_OF_DAY_AFTERNOON;
	m_pReflectionTexture=nullptr;
	m_skyBox=nullptr;
	m_meshData=nullptr;
	m_meshDataSize = 0;
	m_meshInMotion = FALSE;
	m_gridOrigin=Vector2(0,0);
	m_gridDirectionX=Vector2(1.0f,0.0f);
	m_gridDirectionY=Vector2(1.0f,0.0f);

	m_gridCellSize=WATER_MESH_SPACING;
	m_gridCellsX=WATER_MESH_X_VERTICES;
	m_gridCellsY=WATER_MESH_Y_VERTICES;
	m_gridWidth = m_gridCellsX * m_gridCellSize;
	m_gridHeight = m_gridCellsY * m_gridCellSize;

	m_riverVOrigin=0;
	m_riverTexture=nullptr;
	m_whiteTexture=nullptr;
	m_waterNoiseTexture=nullptr;
	m_riverAlphaEdge=nullptr;
	m_waterPixelShader=0;
	m_riverWaterPixelShader=0;
	m_trapezoidWaterPixelShader=0;
	m_waterSparklesTexture=nullptr;
	m_riverXOffset=0;
	m_riverYOffset=0;
}

//-------------------------------------------------------------------------------------------------
/** WW3D method that returns object bounding sphere used in frustum culling*/
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::Get_Obj_Space_Bounding_Sphere(SphereClass & sphere) const
{
	//Since this object is more of a system (containing lots of water pieces),
	//let's disable culling by making bounds huge.  Let each piece do it's own cull.
	Vector3	ObjSpaceCenter(0,0,0);
//	Vector3	ObjSpaceRadius(m_dx,m_dy,0);
	Vector3	ObjSpaceRadius(50000,50000,0);

	sphere.Init(ObjSpaceCenter,ObjSpaceRadius.Length());
}

//-------------------------------------------------------------------------------------------------
/** WW3D method that returns object bounding box used in collision detection*/
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::Get_Obj_Space_Bounding_Box(AABoxClass & box) const
{
	//Since this object is more of a system (containing lots of water pieces),
	//let's disable culling by making bounds huge.  Let each piece do it's own cull.

	Vector3	ObjSpaceCenter(0,0,0);
	Vector3	ObjSpaceExtents(50000,50000,0.001f*m_dy);	//since mirror is a plane, it has no thickness. Set to m_dy/1000.

	box.Init(ObjSpaceCenter,ObjSpaceExtents);
}

//-------------------------------------------------------------------------------------------------
/** returns the class id, so the scene can tell what kind of render object it has. */
//-------------------------------------------------------------------------------------------------
Int WaterRenderObjClass::Class_ID() const
{
	return RenderObjClass::CLASSID_UNKNOWN;
}

//-------------------------------------------------------------------------------------------------
/** Not used, but required virtual method. */
//-------------------------------------------------------------------------------------------------
RenderObjClass *	 WaterRenderObjClass::Clone() const
{
	assert(false);
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Fill water surface strip indices into one or both output buffers */
//-------------------------------------------------------------------------------------------------
static void W3DWater_FillStripIndices(Int sizeX, Int numIndices, UnsignedShort *legacyIndices, UnsignedShort *backendIndices)
{
	Int i,j,k;

	for (i=0,j=0,k=0; i<numIndices; j++)
	{
		for (;k<(sizeX*(j+1)); k++,i+=2)
		{
			if (legacyIndices != nullptr)
			{
				legacyIndices[i]=(UnsignedShort) k+sizeX;
				legacyIndices[i+1]=(UnsignedShort) k;
			}
			if (backendIndices != nullptr)
			{
				backendIndices[i]=(UnsignedShort) k+sizeX;
				backendIndices[i+1]=(UnsignedShort) k;
			}
		}
		//Generate 4 degenerate triangle to connect current strip to next strip/row of map
		//To do this, we just repeat the last index of first strip and first index of new strip.
		//Any triangles with repeated vertices will be skipped during rendering.
		if (i<numIndices) //check if there is at least 1 more strip to go
		{
			if (legacyIndices != nullptr)
			{
				legacyIndices[i]=k-1;
				legacyIndices[i+1]=k+sizeX;
			}
			if (backendIndices != nullptr)
			{
				backendIndices[i]=k-1;
				backendIndices[i+1]=k+sizeX;
			}
			i+=2;
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Create and fill a backend index buffer with water surface strip indices */
//-------------------------------------------------------------------------------------------------
bool WaterRenderObjClass::generateIndexBuffer(Int sizeX, Int sizeY)
{
	//Will need SizeY-1 strips, each of length SizeX*2 (2 indices per strip segment).
	//Will also need 2 extra indices to connect each strip to next one (except last strip)
	//Total index buffer size = (SizeY-1)*(SizeX*2+2) - 2 (drop the extra 2 indices from last strip)

	m_numIndices=(sizeY-1)*(sizeX*2+2) - 2;

	REF_PTR_RELEASE(m_waterMeshIndexBuffer);
	m_waterMeshIndexBuffer=NEW_REF(RenderIndexBufferClass,(m_numIndices));
	RenderIndexBufferClass::WriteLockClass lockBackendIndexBuffer(m_waterMeshIndexBuffer);
	W3DWater_FillStripIndices(sizeX, m_numIndices, nullptr, lockBackendIndexBuffer.Get_Index_Array());

	/*Old way
	Int step=1;
	Int psize=(size-1)/step;

	m_numIndices=psize*((psize+1)*2)+(psize*2)-2;


	Int x,z,s_toggle=1;
	for (z=step; z<size; z+=step)
	{
		if (s_toggle)
		{
			for (x=0; x<(size-step); x+=step)
			{
				*pIndices++=(WORD)((z-0)*size+(x));
				*pIndices++=(WORD)((z-step)*size+(x));
			}
				*pIndices++=(WORD)((z-0)*size+(size-1));
			*pIndices++=(WORD)((z-step)*size+(size-1));
			// insert additional degenerate to start next row
			*pIndices++=pIndices[-2];
			*pIndices++=pIndices[-1];
		}
		else
		{
			*pIndices++=(WORD)((z-step)*size+(size-1));
			*pIndices++=(WORD)((z-0)*size+(size-1));
			for (x=size-1; x>0; x-=step)
			{
				*pIndices++=(WORD)((z-step)*size+(x-step));
				*pIndices++=(WORD)((z-0)*size+(x-step));
			}
			// insert additional degenerate to start next row
			*pIndices++=pIndices[-1];
			*pIndices++=pIndices[-1];
		}

		s_toggle=!s_toggle;
	}
*/
	return true;
}

//-------------------------------------------------------------------------------------------------
/** Releases all w3d assets, to prepare for Reset device call. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::ReleaseResources()
{

	REF_PTR_RELEASE(m_indexBuffer);
	REF_PTR_RELEASE(m_waterMeshIndexBuffer);

	REF_PTR_RELEASE(m_pReflectionTexture);

	if (m_waterTrackSystem)
		m_waterTrackSystem->ReleaseResources();

	if (m_waterPixelShader)
		g_renderBackend->Delete_Pixel_Shader(m_waterPixelShader);

	if (m_trapezoidWaterPixelShader)
		g_renderBackend->Delete_Pixel_Shader(m_trapezoidWaterPixelShader);

	if (m_riverWaterPixelShader)
		g_renderBackend->Delete_Pixel_Shader(m_riverWaterPixelShader);

	m_waterPixelShader = 0;
	m_trapezoidWaterPixelShader=0;
	m_riverWaterPixelShader=0;
}

//-------------------------------------------------------------------------------------------------
/** (Re)allocates all W3D assets after a reset.. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::ReAcquireResources()
{
	m_indexBuffer=NEW_REF(RenderIndexBufferClass,(6));
	// Fill up the IB
	{
		RenderIndexBufferClass::WriteLockClass lockIdxBuffer(m_indexBuffer);
		UnsignedShort *ib=lockIdxBuffer.Get_Index_Array();
		//quad of 2 triangles:
		//	3-----2
		//  |    /|
		//  |  /  |
		//	|/    |
		//  0-----1
		ib[0]=3;
		ib[1]=0;
		ib[2]=2;
		ib[3]=2;
		ib[4]=0;
		ib[5]=1;
	}

	//We're using the same grid for either 3D Water Mesh or Pixel/Vertex shader.  Just
	//allocate the right size depending on usage
	if (m_meshData)
	{
		//Create new grid data
		if (!generateIndexBuffer(m_gridCellsX+1,m_gridCellsY+1))
			return;
	}
	else
	if (m_waterType == WATER_TYPE_2_PVSHADER)
	{
		// Sea water is submitted through backend transient batches.
	}

	if (m_waterTrackSystem)
		m_waterTrackSystem->ReAcquireResources();

	if (W3DShaderManager::getChipset() >= DC_GENERIC_PIXEL_SHADER_1_1)
	{
		unsigned long legacyHandle = 0;
		if (g_renderBackend->Create_Legacy_Pixel_Shader(RB_LEGACY_PIXEL_SHADER_RIVER_WATER, &legacyHandle))
			m_riverWaterPixelShader = legacyHandle;
		if (g_renderBackend->Create_Legacy_Pixel_Shader(RB_LEGACY_PIXEL_SHADER_REFLECTIVE_WATER, &legacyHandle))
			m_waterPixelShader = legacyHandle;
		if (g_renderBackend->Create_Legacy_Pixel_Shader(RB_LEGACY_PIXEL_SHADER_TRAPEZOID_WATER, &legacyHandle))
			m_trapezoidWaterPixelShader = legacyHandle;
	}

	//W3D Invalidate textures after losing the device and since we peek at the textures directly, it won't
	//know to reinit them for us.  Do it here manually:
	if (m_riverTexture && !m_riverTexture->Is_Initialized())
		m_riverTexture->Init();
	if (m_waterNoiseTexture && !m_waterNoiseTexture->Is_Initialized())
		m_waterNoiseTexture->Init();
	if (m_riverAlphaEdge && !m_riverAlphaEdge->Is_Initialized())
		m_riverAlphaEdge->Init();
	if (m_waterSparklesTexture && !m_waterSparklesTexture->Is_Initialized())
		m_waterSparklesTexture->Init();
	if (m_whiteTexture && !m_whiteTexture->Is_Initialized())
	{	m_whiteTexture->Init();
		W3DWater_FillWhiteTexture(m_whiteTexture);
	}
}

void WaterRenderObjClass::load()
{
	if (m_waterTrackSystem)
		m_waterTrackSystem->loadTracks();
}

//-------------------------------------------------------------------------------------------------
/** Initializes water with dimensions and parent scene.
	* During rendering, we will render a water surface of given dimensions
	* and reflect the parent scene in its surface.  For now, waters are
	* forced to be rectangles. */
//-------------------------------------------------------------------------------------------------
Int WaterRenderObjClass::init(Real waterLevel, Real dx, Real dy, SceneClass *parentScene, WaterType type)
{

	m_fBumpFrame=0;
	m_fBumpScale=SEA_BUMP_SCALE;

	m_dx=dx;
	m_dy=dy;
	m_level=waterLevel;

	m_LastUpdateTime=timeGetTime();
	m_uScrollPerMs=0.001f;
	m_vScrollPerMs=0.001f;
	m_uOffset=0;
	m_vOffset=0;

	m_parentScene=parentScene;
	m_waterType = type;

	/// Hack for now
	//m_waterType = WATER_TYPE_0_TRANSLUCENT;

	///@todo: calculate a real normal/distance for arbitrary planes.
	m_planeNormal=Vector3(0,0,1);		//water plane normal
	m_planeDistance=m_level;	//water plane distance(always at zero for now)

	m_meshLight=NEW_REF(LightClass,(LightClass::DIRECTIONAL));
	m_meshLight->Set_Ambient(Vector3(0.1f,0.1f,0.1f));
	m_meshLight->Set_Diffuse(Vector3(1.0f,1.0f,1.0f));
	m_meshLight->Set_Specular(Vector3(1.0f,1.0f,1.0f));
	m_meshLight->Set_Position(Vector3(1000,1000,1000));
	//testLight->Set_Spot_Direction(Vector3(TheGlobalData->m_terrainLightX,TheGlobalData->m_terrainLightY,TheGlobalData->m_terrainLightZ));
	m_meshLight->Set_Spot_Direction(Vector3(-0.57f,-0.57f,-0.57f));

	//Setup material for 3D Mesh water.
	m_meshVertexMaterialClass=NEW_REF(VertexMaterialClass,());
	m_meshVertexMaterialClass->Set_Shininess(20.0);
	m_meshVertexMaterialClass->Set_Ambient(1.0f,1.0f,1.0f);
	m_meshVertexMaterialClass->Set_Diffuse(1.0f,1.0f,1.0f);
	m_meshVertexMaterialClass->Set_Specular(0.5,0.5,0.5);
	m_meshVertexMaterialClass->Set_Opacity(WATER_MESH_OPACITY);
	m_meshVertexMaterialClass->Set_Lighting(true);

	//
	// assign the data from the WaterSettings[] global to the data for this
	// render object (we at present only have one water plane)
	//
	loadSetting( &m_settings[ TIME_OF_DAY_MORNING ], TIME_OF_DAY_MORNING );
	loadSetting( &m_settings[ TIME_OF_DAY_AFTERNOON ], TIME_OF_DAY_AFTERNOON );
	loadSetting( &m_settings[ TIME_OF_DAY_EVENING ], TIME_OF_DAY_EVENING );
	loadSetting( &m_settings[ TIME_OF_DAY_NIGHT ], TIME_OF_DAY_NIGHT );

	Set_Sort_Level(2);	//force water to be drawn after all other non translucent objects in scene.
	Set_Force_Visible(TRUE);	//water is always visible since it's a composite object made of multiple planes all over the map.

	ReAcquireResources();

	//Setup material for regular water
	m_vertexMaterialClass=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);



	m_shaderClass = zFillAlphaShader;//ShaderClass::_PresetAlphaShader;ShaderClass::_PresetOpaqueShader;//detailOpaqueShader;
	m_shaderClass.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);	//water should be visible from both sides

	//Assets used for all types of water
	m_alphaClippingTexture=WW3DAssetManager::Get_Instance()->Get_Texture(SKYBODY_TEXTURE);
	m_skyBox = ((W3DAssetManager*)W3DAssetManager::Get_Instance())->Create_Render_Obj( "new_skybox", TheGlobalData->m_skyBoxScale, 0);

	//Enable clamping on all textures used by the skybox (to reduce corner seams).
	if (m_skyBox && m_skyBox->Class_ID() == RenderObjClass::CLASSID_MESH)
	{
		MeshClass *mesh=(MeshClass*) m_skyBox;
		MaterialInfoClass	*material = mesh->Get_Material_Info();

		for (Int i=0; i<material->Texture_Count(); i++)
		{
			if (material->Peek_Texture(i))
			{
				material->Peek_Texture(i)->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
				material->Peek_Texture(i)->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
			}
		}

		REF_PTR_RELEASE(material);
	}

	m_riverTexture=WW3DAssetManager::Get_Instance()->Get_Texture(TheWaterTransparency->m_standingWaterTexture.str());

	//For some reason setting a null texture does not result in 0xffffffff for pixel shaders so using explicit "white" texture.
	m_whiteTexture=MSGNEW("TextureClass") TextureClass(1, 1, WW3D_FORMAT_A4R4G4B4, MIP_LEVELS_1);
	W3DWater_FillWhiteTexture(m_whiteTexture);

	m_waterNoiseTexture=WW3DAssetManager::Get_Instance()->Get_Texture("Noise0000.tga");
	m_riverAlphaEdge=WW3DAssetManager::Get_Instance()->Get_Texture("TWAlphaEdge.tga");
	m_waterSparklesTexture=WW3DAssetManager::Get_Instance()->Get_Texture("WaterSurfaceBubbles.tga");
#ifdef DRAW_WATER_WAKES
	m_waterTrackSystem = NEW WaterTracksRenderSystem;
	m_waterTrackSystem->init();
#endif

	return 0;
}

void WaterRenderObjClass::updateMapOverrides()
{
	if (m_riverTexture && TheWaterTransparency->m_standingWaterTexture.compareNoCase(m_riverTexture->Get_Texture_Name()) != 0)
	{
		REF_PTR_RELEASE(m_riverTexture);
		m_riverTexture = WW3DAssetManager::Get_Instance()->Get_Texture(TheWaterTransparency->m_standingWaterTexture.str());
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void WaterRenderObjClass::reset()
{

	// for vertex animated water mesh reset the values
	if( m_meshData)
	{
		Int i, j;
		WaterMeshData *pData;
		Int	mx = m_gridCellsX + 1;
		Int my = m_gridCellsY + 1;

		// go through each mesh point and adjust the height according to the velocity
		for( j = 0, pData = m_meshData; j < (my + 2); j++ )
		{

			for( i = 0; i < (mx + 2); i++ )
			{

				// areset grid values for this cell
				pData->velocity = 0.0f;
				pData->height = 0.0f;
				pData->preferredHeight = 0.0f;
				pData->status = WaterRenderObjClass::AT_REST;

				// on to the next one
				pData++;

			}

		}

		// mesh data is no longer in motion
		m_meshInMotion = FALSE;

	}

	if (m_waterTrackSystem)
		m_waterTrackSystem->reset();
}

void WaterRenderObjClass::enableWaterGrid(Bool state)
{
	m_doWaterGrid = state;

	m_drawingRiver = false;
	m_disableRiver = false;

	if (state && m_meshData == nullptr)
	{	//water type has changed, must allocate necessary assets for new water.
		//contains the current deformed water surface z(height) values.  With 1 vertex invisible border
		//around surface to speed up normal calculations.
		m_meshDataSize = (m_gridCellsX+1+2)*(m_gridCellsY+1+2);
		m_meshData=NEW WaterMeshData[ m_meshDataSize ];
		memset(m_meshData,0,sizeof(WaterMeshData)*(m_gridCellsX+1+2)*(m_gridCellsY+1+2));
		reset();

		//Create new grid data
		if (!generateIndexBuffer(m_gridCellsX+1,m_gridCellsY+1))
			return;
	}
}

// ------------------------------------------------------------------------------------------------
/** Update phase for water if we need it. */
// ------------------------------------------------------------------------------------------------
void WaterRenderObjClass::update()
{
	// TheSuperHackers @tweak The water movement time step is now decoupled from the render update.
	const Real timeScale = TheFramePacer->getActualLogicTimeScaleOverFpsRatio();

	{
		constexpr const Real MagicOffset = 0.0125f * 33 / 5000; ///< the work of top Munkees; do not question it

		m_riverVOrigin += 0.002f * timeScale;
		m_riverXOffset += (Real)(MagicOffset * timeScale);
		m_riverYOffset += (Real)(2 * MagicOffset * timeScale);

		// This moves offsets towards zero when smaller -1.0 or larger 1.0
		m_riverXOffset -= (Int)m_riverXOffset;
		m_riverYOffset -= (Int)m_riverYOffset;

		m_fBumpFrame += timeScale;
		if (m_fBumpFrame >= NUM_BUMP_FRAMES)
			m_fBumpFrame = 0.0f;

		// for vertex animated water we need to update the vector field
		if( m_doWaterGrid && m_meshInMotion == TRUE )
		{
			const Real PREFERRED_HEIGHT_FUDGE = 1.0f;		///< this is close enough to at rest
			const Real AT_REST_VELOCITY_FUDGE = 1.0f;		///< when we're close enough to at rest height and velocity we will stop
			const Real WATER_DAMPENING = 0.93f;					///< use with up force of 15.0
			Int i, j;
			Int	mx = m_gridCellsX+1;
			Int my = m_gridCellsY+1;
			WaterMeshData *pData;

			//
			// we will mark the mesh as clean now ... if any of the fields are still in motion
			// they will continue to mark the mesh as dirty so processing continues next frame
			//
			m_meshInMotion = FALSE;

			// go through each mesh point and adjust the height according to the velocity
			for( j = 0, pData = m_meshData; j < (my + 2); j++ )
			{

				for( i = 0; i < (mx + 2); i++ )
				{

					// only pay attention to mesh points that are in motion
					if( BitIsSet( pData->status, WaterRenderObjClass::IN_MOTION ) )
					{

						// DAMPENING to slow the changes down
						pData->velocity *= WATER_DAMPENING;

						// if the height here is below our preferred height, we want to add upward force to counteract it
						if( pData->height < pData->preferredHeight )
							pData->velocity -= TheGlobalData->m_gravity * 3.0f;
						else
							pData->velocity += TheGlobalData->m_gravity * 3.0f;

						// adjust the height at this grid location according to the current velocity
						pData->height = pData->height + pData->velocity;

						//
						// if we are close enough to our preferred height and our velocity is small enough
						// this will be our resting location
						//
						if( fabs( pData->height - pData->preferredHeight ) < PREFERRED_HEIGHT_FUDGE &&
								fabs( pData->velocity ) < AT_REST_VELOCITY_FUDGE )
						{

							BitClear( pData->status, WaterRenderObjClass::IN_MOTION );
							pData->height = pData->preferredHeight;
							pData->velocity = 0.0f;

						}
						else
						{

							// there is still motion in the mesh, we need to process next frame
							m_meshInMotion = TRUE;

						}

					}

					// on to the next one
					pData++;

				}

			}

		}

	}

}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::replaceSkyboxTexture(const AsciiString& oldTexName, const AsciiString& newTextName)
{
	W3DAssetManager* assetManager = ((W3DAssetManager*)W3DAssetManager::Get_Instance());

	assetManager->replacePrototypeTexture(m_skyBox, oldTexName.str(), newTextName.str());

	//Enable clamping on all textures used by the skybox (to reduce corner seams).
	if (m_skyBox && m_skyBox->Class_ID() == RenderObjClass::CLASSID_MESH)
	{
		MeshClass *mesh=(MeshClass*) m_skyBox;
		MaterialInfoClass	*material = mesh->Get_Material_Info();

		for (Int i=0; i<material->Texture_Count(); i++)
		{
			if (material->Peek_Texture(i))
			{
				material->Peek_Texture(i)->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
				material->Peek_Texture(i)->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
			}
		}
		REF_PTR_RELEASE(material);
	}

}

//-------------------------------------------------------------------------------------------------
/** Adjusts various water/sky rendering settings that depend on time of day. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::setTimeOfDay(TimeOfDay tod)
{
	m_tod=tod;
}

//-------------------------------------------------------------------------------------------------
/**Copies GDF settings dealing with a particular time of day into our own
	* structures.  Also allocates any required W3D assets (textures). */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::loadSetting( Setting *setting, TimeOfDay timeOfDay )
{
	SurfaceClass::SurfaceDescription surfaceDesc;

	// sanity
	DEBUG_ASSERTCRASH( setting, ("WaterRenderObjClass::loadSetting, null setting") );

	// textures
	setting->skyTexture = WW3DAssetManager::Get_Instance()->Get_Texture( WaterSettings[ timeOfDay ].m_skyTextureFile.str() );
	setting->waterTexture = WW3DAssetManager::Get_Instance()->Get_Texture( WaterSettings[ timeOfDay ].m_waterTextureFile.str() );

	// texelss per unit
	setting->skyTexelsPerUnit = WaterSettings[ timeOfDay ].m_skyTexelsPerUnit;
	setting->waterTexture->Get_Level_Description( surfaceDesc, 0 );
	setting->skyTexelsPerUnit /= (Real)surfaceDesc.Width;

	// water repeat
	setting->waterRepeatCount = WaterSettings[ timeOfDay ].m_waterRepeatCount;

	// U and V scroll per ms
	setting->uScrollPerMs = WaterSettings[ timeOfDay ].m_uScrollPerMs;
	setting->vScrollPerMs = WaterSettings[ timeOfDay ].m_vScrollPerMs;

	//
	// vertex colors
	//
	// bottom left
	setting->vertex00Diffuse = (WaterSettings[ timeOfDay ].m_vertex00Diffuse.red << 16) |
														 (WaterSettings[ timeOfDay ].m_vertex00Diffuse.green << 8) |
														  WaterSettings[ timeOfDay ].m_vertex00Diffuse.blue;
	// top left
	setting->vertex01Diffuse = (WaterSettings[ timeOfDay ].m_vertex01Diffuse.red << 16) |
														 (WaterSettings[ timeOfDay ].m_vertex01Diffuse.green << 8) |
														  WaterSettings[ timeOfDay ].m_vertex01Diffuse.blue;
	// bottom right
	setting->vertex10Diffuse = (WaterSettings[ timeOfDay ].m_vertex10Diffuse.red << 16) |
														 (WaterSettings[ timeOfDay ].m_vertex10Diffuse.green << 8) |
														  WaterSettings[ timeOfDay ].m_vertex10Diffuse.blue;
	// top right
	setting->vertex11Diffuse = (WaterSettings[ timeOfDay ].m_vertex11Diffuse.red << 16) |
														 (WaterSettings[ timeOfDay ].m_vertex11Diffuse.green << 8) |
														  WaterSettings[ timeOfDay ].m_vertex11Diffuse.blue;

	// diffuse water color
	setting->waterDiffuse = (WaterSettings[ timeOfDay ].m_waterDiffuseColor.alpha << 24) |
												  (WaterSettings[ timeOfDay ].m_waterDiffuseColor.red		<< 16) |
													(WaterSettings[ timeOfDay ].m_waterDiffuseColor.green << 8) |
												   WaterSettings[ timeOfDay ].m_waterDiffuseColor.blue;

	// transparent water color
	setting->transparentWaterDiffuse = (WaterSettings[ timeOfDay ].m_transparentWaterDiffuse.alpha << 24) |
																		 (WaterSettings[ timeOfDay ].m_transparentWaterDiffuse.red	 << 16) |
																		 (WaterSettings[ timeOfDay ].m_transparentWaterDiffuse.green << 8) |
																		  WaterSettings[ timeOfDay ].m_transparentWaterDiffuse.blue;

}

//-------------------------------------------------------------------------------------------------
/** Our water may use effects that require run-time rendered textures.  These
	*	textures need to be updated before we start rendering to the main screen
	* render target because the legacy renderer did not support multiple render targets. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::updateRenderTargetTextures(CameraClass *cam)
{
#if !defined(GGC_BGFX_STANDALONE)
	if (!W3DWater_UseBackendSeaBatch() &&
		m_waterType == WATER_TYPE_2_PVSHADER && getClippedWaterPlane(cam, nullptr) &&
		TheTerrainRenderObject && TheTerrainRenderObject->getMap())
		renderMirror(cam);	//generate texture containing reflected scene
#endif
}

//-------------------------------------------------------------------------------------------------
/** Renders the reflected scene into an offscreen texture. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::renderMirror(CameraClass *cam)
{
#ifdef EXTENDED_STATS
	if (g_renderDebugStats.m_disableWater) {
		return;
	}
#endif
	Matrix3D	OldCameraMatrix=cam->Get_Transform();
	Matrix4x4	FullMatrix4(cam->Get_Transform());	//copy 3x4 matrix into a 4x4
	Vector3		WaterNormal(0,0,1);	//normal of plane used for reflection
	Vector4		WaterPlane(WaterNormal.X,WaterNormal.Y,WaterNormal.Z,m_level);
	Vector3		rRight,rUp,rN,rPos;	//orientation and translation vectors of camera

	Matrix4x4	FullMatrix(FullMatrix4.Transpose());	//swap rows/columns

	//reflect camera right vector
	Real axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[0],WaterNormal);
	rRight = (Vector3&)FullMatrix[0] - (2.0f*axis_distance*WaterNormal);

	//reflect camera up vector
	axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[1],WaterNormal);
	rUp = (Vector3&)FullMatrix[1] - (2.0f*axis_distance*WaterNormal);

	//reflect camera n vector
	axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[2],WaterNormal);
	rN = (Vector3&)FullMatrix[2] - (2.0f*axis_distance*WaterNormal);

	//reflect camera position
	axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[3],WaterNormal);	//distance cam to origin
	axis_distance -= WaterPlane.W;	// subtract mirror plane distance to get distance camera to plane
	rPos = (Vector3&)FullMatrix[3] - (2.0f*axis_distance*WaterNormal);

	//generate a new camera matrix from reflected vectors
	Matrix3D reflectedTransform(rRight,rUp,rN,rPos);


	g_renderBackend->Set_Render_Target_With_Z((TextureClass*)m_pReflectionTexture);

	// Clear the backbuffer
	WW3D::Begin_Render(false,true,Vector3(0.0f,0.0f,0.0f));	//clearing only z-buffer since background always filled with clouds

	cam->Set_Transform( reflectedTransform );

	//Force reflected image to be drawn into full texture size - not a viewport inside texture.
	Vector2 vMin,vMax,vOldMax,vOldMin;
 	cam->Get_Viewport(vOldMin,vOldMax);
 	vMax.X=vMax.Y=1.0f;
	vMin.X=vMin.Y=0.0f;
 	cam->Set_Viewport(vMin,vMax);

	cam->Apply();	//force an update of all the camera dependent parameters like frustum clip planes

	//flip the winding order of polygons to draw the reflected back sides.
	ShaderClass::Invert_Backface_Culling(true);

	// Render the scene
	renderSky();
	if (m_tod == TIME_OF_DAY_NIGHT)
		renderSkyBody(&reflectedTransform);

	WW3D::Render(m_parentScene,cam);

	cam->Set_Transform(OldCameraMatrix);	//restore original non-reflected matrix
 	cam->Set_Viewport(vOldMin,vOldMax);

	cam->Apply();	//force an update of all the camera dependent parameters like frustum clip planes

	ShaderClass::Invert_Backface_Culling(false);

	WW3D::End_Render(false);

	// TheSuperHackers @fix bobtista 21/04/2026 Route through g_renderBackend
	// so the bgfx backend's renderToTexture flag gets reset. Same pattern as
	// TexProjectClass::Compute_Texture. The old direct-device bypass left
	// renderToTexture stuck at true after the reflection pass.
	g_renderBackend->Set_Render_Target_With_Z(nullptr, nullptr);
}

//-------------------------------------------------------------------------------------------------
/** Renders (draws) the water.
	*	Algorithm:
	*	Draw reflected scene.
	*	Draw reflected sky layer(s) and bodies.
	*	Clear Zbuffer
	*	Fill Zbuffer by drawing water surface (allows proper sorting into regular scene).
	*	Draw non-reflected scene (done in regular app render loop).
	*
	*	This algorithm doesn't apply to translucent water, which is rendered into a
	*   texture and rendered at end of scene. */
//-------------------------------------------------------------------------------------------------
//DECLARE_PERF_TIMER(Water)
void WaterRenderObjClass::Render(RenderInfoClass & rinfo)
{
	//USE_PERF_TIMER(Water)
	if (TheTerrainRenderObject && !TheTerrainRenderObject->getMap())
		return;	//no map has been loaded yet.

	if (((RTS3DScene *)rinfo.Camera.Get_User_Data())->getCustomPassMode() == SCENE_PASS_ALPHA_MASK ||
		((SceneClass *)rinfo.Camera.Get_User_Data())->Get_Extra_Pass_Polygon_Mode() == SceneClass::EXTRA_PASS_CLEAR_LINE)
		return;	//water is not drawn in wireframe or custom scene passes

#ifdef EXTENDED_STATS
	if (g_renderDebugStats.m_disableWater) {
		return;
	}
#endif
	if (ShaderClass::Is_Backface_Culling_Inverted())
		return;	//the water object will not reflect in itself, so don't do anything if rendering a mirror.

	//this water type needs to rendered after the rest of scene, so buffer it up for later

	// If static sort lists are enabled and this mesh has a sort level, put it on the list instead
	// of rendering it.
	unsigned int sort_level = (unsigned int)Get_Sort_Level();

	if (WW3D::Are_Static_Sort_Lists_Enabled() && sort_level != SORT_LEVEL_NONE)
	{
		WW3D::Add_To_Static_Sort_List(this, sort_level);
		return;
	}

	switch(m_waterType)
	{
		case WATER_TYPE_0_TRANSLUCENT:
		case WATER_TYPE_3_GRIDMESH:
			//Draw the water surface as a bunch of alpha blended tiles covering areas where water is visible
			renderWater();
			if (!m_drawingRiver || m_disableRiver) {
				renderWaterMesh();	//Draw water surface as 3D deforming mesh if it's enabled on this map.
			}
			break;

		case WATER_TYPE_2_PVSHADER:
			//Pixel/Vertex Shader based water which uses an off-screen rendered reflection texture
			drawSeaBatch(rinfo);
			break;

		case WATER_TYPE_1_FB_REFLECTION:
			{
				//Normal frame buffer reflection water type. Non translucent.  Legacy code we're not using anymore.
				Matrix3D	OldCameraMatrix=rinfo.Camera.Get_Transform();
				Matrix4x4	FullMatrix4(rinfo.Camera.Get_Transform());	//copy 3x4 matrix into a 4x4
				Vector3		WaterNormal(0,0,1);	//normal of plane used for reflection
				Vector4		WaterPlane(WaterNormal.X,WaterNormal.Y,WaterNormal.Z,m_level);	//assume distance to origin 0
				Vector3		rRight,rUp,rN,rPos;	//orientation and translation vectors of camera

				Matrix4x4	FullMatrix(FullMatrix4.Transpose());	//swap rows/columns

				//reflect camera right vector
				Real axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[0],WaterNormal);
				rRight = (Vector3&)FullMatrix[0] - (2.0f*axis_distance*WaterNormal);

				//reflect camera up vector
				axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[1],WaterNormal);
				rUp = (Vector3&)FullMatrix[1] - (2.0f*axis_distance*WaterNormal);

				//reflect camera n vector
				axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[2],WaterNormal);
				rN = (Vector3&)FullMatrix[2] - (2.0f*axis_distance*WaterNormal);

				//reflect camera position
				axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[3],WaterNormal);	//distance cam to origin
				axis_distance -= WaterPlane.W;	// subtract mirror plane distance to get distance camera to plane
				rPos = (Vector3&)FullMatrix[3] - (2.0f*axis_distance*WaterNormal);

				//generate a new camera matrix from reflected vectors
				Matrix3D reflectedTransform(rRight,rUp,rN,rPos);

				//flip the winding order of polygons to draw the reflected back sides.
				ShaderClass::Invert_Backface_Culling(true);

			#if 0	// No longer do simple rendering.
				if (TheGlobalData->m_useWaterPlane)
				{
					//@todo : Would it be better to create a new camera or change the transform of the
					//existing one?
					rinfo.Camera.Set_Transform( reflectedTransform );
					rinfo.Camera.Apply();	//force an update of all the camera dependent parameters like frustum clip planes

					if(m_useCloudLayer)
					{
						if (TheGlobalData && TheGlobalData->m_drawEntireTerrain)
							m_skyBox->Render(rinfo);
						else
						{
							renderSky();
							if (m_tod == TIME_OF_DAY_NIGHT)
								renderSkyBody(&reflectedTransform);
						}
					}

					WW3D::Render(m_parentScene,&rinfo.Camera);

					rinfo.Camera.Set_Transform(OldCameraMatrix);	//restore original non-reflected matrix
					rinfo.Camera.Apply();	//force an update of all the camera dependent parameters like frustum clip planes

					//clear the z-buffer to remove changes made by objects inside mirror
					g_renderBackend->Clear(false,true,Vector3(0.1f,0.1f,0.1f));
				}
			#endif

				ShaderClass::Invert_Backface_Culling(false);	//return culling back to normal

				ShaderClass::Invalidate();	//reset shading system so it forces full state set.

				renderWater();
			}
			break;

		default:
			break;
	}

	if (TheGlobalData && TheGlobalData->m_drawSkyBox)
	{	//center skybox around camera
		Vector3 pos=rinfo.Camera.Get_Position();
		pos.Z = TheGlobalData->m_skyBoxPositionZ;
		m_skyBox->Set_Position(pos);
		m_skyBox->Render(rinfo);
	}

	//Clean up after any pixel shaders.
	// Force render state apply so that the null texture releases the shroud reference count.
	g_renderBackend->Apply_Render_State_Changes();
	g_renderBackend->Invalidate_Cached_Render_States();

	if (m_waterTrackSystem)
		m_waterTrackSystem->flush(rinfo);

//	renderWaterMesh();
//	renderWaterWave();
}

//-------------------------------------------------------------------------------------------------
/** Clips the water plane to the current camera frustum and returns a bounding
	* box enclosing the clipped plane.  Returns false if water plane is not visible. */
//-------------------------------------------------------------------------------------------------
Bool WaterRenderObjClass::getClippedWaterPlane(CameraClass *cam, AABoxClass *box)
{
	const FrustumClass & frustum = cam->Get_Frustum();

	ClipPolyClass	ClippedPoly0;
	ClipPolyClass	ClippedPoly1;

	///@todo: generate proper sized polygon
	ClippedPoly0.Reset();
	ClippedPoly0.Add_Vertex(Vector3(0,0,m_level));
	ClippedPoly0.Add_Vertex(Vector3(0,m_dy,m_level));
	ClippedPoly0.Add_Vertex(Vector3(m_dx,m_dy,m_level));
	ClippedPoly0.Add_Vertex(Vector3(m_dx,0,m_level));

	//clip against all 6 frustum planes
	ClippedPoly0.Clip(frustum.Planes[0],ClippedPoly1);
	ClippedPoly1.Clip(frustum.Planes[1],ClippedPoly0);
	ClippedPoly0.Clip(frustum.Planes[2],ClippedPoly1);
	ClippedPoly1.Clip(frustum.Planes[3],ClippedPoly0);
	ClippedPoly0.Clip(frustum.Planes[4],ClippedPoly1);
	ClippedPoly1.Clip(frustum.Planes[5],ClippedPoly0);

	Int final_vcount = ClippedPoly0.Verts.Count();

	//make sure the polygon is visible
	if (final_vcount >= 3)
	{
		//find axis aligned bounding box around visible polygon
		if (box)
  			box->Init(&(ClippedPoly0.Verts[0]),final_vcount);
		return TRUE;
	}

	return FALSE;	//water plane is not visible
}

void WaterRenderObjClass::drawSeaBatch(RenderInfoClass & rinfo)
{
	AABoxClass	seaBox;

	if (!getClippedWaterPlane(&rinfo.Camera,&seaBox))
	{
		return;	//the sea is not visible
	}

	std::vector<SeaPatchBatchEntry> patches;

	Int patchX;
	Int patchY;
	Real patchWorldWidth = PATCH_WIDTH * PATCH_SCALE;

	for (patchY=(Int)((seaBox.Center.Y-seaBox.Extent.Y)/patchWorldWidth);
		(patchY*patchWorldWidth)<(seaBox.Center.Y+seaBox.Extent.Y); patchY++)
	{
		for (patchX=(Int)((seaBox.Center.X-seaBox.Extent.X)/patchWorldWidth);
			(patchX*patchWorldWidth)<(seaBox.Center.X+seaBox.Extent.X); patchX++)
		{
			SeaPatchBatchEntry entry;
			entry.patchX = patchX;
			entry.patchY = patchY;
			patches.push_back(entry);
		}
	}

	if (patches.empty())
	{
		return;
	}

	const size_t maxBatchElements = 60000;
	const size_t patchVertexCount = PATCH_SIZE * PATCH_SIZE;
	const size_t patchRectangleCount = PATCH_WIDTH * PATCH_WIDTH;
	const size_t patchIndexCount = patchRectangleCount * 6;
	const Real inverseBumpSize = 1.0f / BUMP_SIZE;
	size_t batchStart = 0;
	while (batchStart < patches.size())
	{
		size_t batchEnd = batchStart;
		size_t totalVertices = 0;
		size_t totalIndices = 0;
		while (batchEnd < patches.size())
		{
			if (batchEnd > batchStart
				&& (totalVertices + patchVertexCount > maxBatchElements
					|| totalIndices + patchIndexCount > maxBatchElements))
			{
				break;
			}
			totalVertices += patchVertexCount;
			totalIndices += patchIndexCount;
			batchEnd++;
		}

		UnsignedShort batchIndexCount = static_cast<UnsignedShort>(totalIndices);
		UnsignedShort batchVertexCount = static_cast<UnsignedShort>(totalVertices);
		UnsignedShort batchTriangleCount = static_cast<UnsignedShort>(totalIndices / 3);

		DynamicIBAccessClass ib_access(BUFFER_TYPE_DYNAMIC,batchIndexCount);
		{
			DynamicIBAccessClass::WriteLockClass lockib(&ib_access);
			UnsignedShort *curIb = lockib.Get_Index_Array();
			UnsignedShort vertexBase = 0;
			for (size_t patchIndex = batchStart; patchIndex < batchEnd; ++patchIndex)
			{
				for (Int j=0; j<PATCH_WIDTH; j++)
				{
					for (Int i=0; i<PATCH_WIDTH; i++)
					{
						curIb[0] = vertexBase + (j)*PATCH_SIZE + i;
						curIb[1] = vertexBase + (j+1)*PATCH_SIZE + i+1;
						curIb[2] = vertexBase + (j+1)*PATCH_SIZE + i;

						curIb[3] = vertexBase + (j)*PATCH_SIZE + i;
						curIb[4] = vertexBase + (j)*PATCH_SIZE + i+1;
						curIb[5] = vertexBase + (j+1)*PATCH_SIZE + i+1;

						curIb += 6;
					}
				}
				vertexBase += static_cast<UnsignedShort>(patchVertexCount);
			}
		}

		DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC,dynamic_fvf_type,batchVertexCount);
		{
			DynamicVBAccessClass::WriteLockClass lock(&vb_access);
			VertexFormatXYZNDUV2* vb=lock.Get_Formatted_Vertex_Array();
			for (size_t patchIndex = batchStart; patchIndex < batchEnd; ++patchIndex)
			{
				Real originX = patches[patchIndex].patchX * patchWorldWidth;
				Real originY = patches[patchIndex].patchY * patchWorldWidth;

				for (Int j=0; j<PATCH_SIZE; j++)
				{
					Real y = originY + (j * PATCH_SCALE);
					for (Int i=0; i<PATCH_SIZE; i++)
					{
						Real x = originX + (i * PATCH_SCALE);

						vb->x=x;
						vb->y=y;
						vb->z=m_level;
						vb->diffuse=m_settings[m_tod].transparentWaterDiffuse;
						vb->u1=(Real)i*PATCH_UV_SCALE + m_uOffset;
						vb->v1=(Real)j*PATCH_UV_SCALE + m_vOffset;
						vb->u2=x*inverseBumpSize;
						vb->v2=(y+0.3f*x)*inverseBumpSize;
						vb->nx=0.0f;
						vb->ny=0.0f;
						vb->nz=1.0f;
						vb++;
					}
				}
			}
		}

		Matrix3D tm(1);
		g_renderBackend->Set_Transform(RB_TRANSFORM_WORLD,tm);
		g_renderBackend->Set_Index_Buffer(ib_access,0);
		g_renderBackend->Set_Vertex_Buffer(vb_access);
		g_renderBackend->Set_Texture(0,m_settings[m_tod].waterTexture);
		g_renderBackend->Set_Texture(1,nullptr);
		g_renderBackend->Set_Texture(2,nullptr);
		g_renderBackend->Set_Texture(3,nullptr);
		g_renderBackend->Set_Material(m_vertexMaterialClass);
		{
			ShaderClass waterShader = ShaderClass::_PresetAlphaShader;
			waterShader.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
			waterShader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
			g_renderBackend->Set_Shader(waterShader);
		}
		g_renderBackend->Override_Alpha_Blend_Enable(true);
		g_renderBackend->Set_Cull_Mode(RB_CULL_NONE);
		g_renderBackend->Draw_Triangles(0,batchTriangleCount,0,batchVertexCount);

		if (TheTerrainRenderObject->getShroud())
		{
			W3DShaderManager::setTexture(0,TheTerrainRenderObject->getShroud()->getShroudTexture());
			W3DShaderManager::setShader(W3DShaderManager::ST_SHROUD_TEXTURE, 0);
			g_renderBackend->Set_Cull_Mode(RB_CULL_NONE);
			g_renderBackend->Set_Depth_Func(RB_CMP_LESS_EQUAL);
			g_renderBackend->Draw_Triangles(0,batchTriangleCount,0,batchVertexCount);
			g_renderBackend->Set_Depth_Func(RB_CMP_EQUAL);
			W3DShaderManager::resetShader(W3DShaderManager::ST_SHROUD_TEXTURE);
		}

		g_renderBackend->Clear_State_Overrides();
		batchStart = batchEnd;
	}
}

#define FEATHER_LAYER_COUNT (5.0f)
#define FEATHER_THICKNESS   (4.0f)

//-------------------------------------------------------------------------------------------------
/** Renders (draws) the water surface.*/
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::renderWater()
{
	std::vector<WaterTrapezoidBatchEntry> trapezoids;

	for (PolygonTrigger *pTrig=PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
		if (pTrig->isWaterArea()) {
			if (pTrig->getNumPoints()>2) {
				if (pTrig->isRiver()) {
					drawRiverWater(pTrig);
					continue;
				}
				Int k;
				for (k=1; k<pTrig->getNumPoints()-1; k=k+2) {
					ICoord3D pt3 = *pTrig->getPoint(0);
					ICoord3D pt2 = *pTrig->getPoint(k);
					ICoord3D pt1 = *pTrig->getPoint(k+1);
					ICoord3D pt0 = *pTrig->getPoint(k+1);
					if (k+2<pTrig->getNumPoints()) {
						pt0 = *pTrig->getPoint(k+2);
					}
					Vector3 points[4];
					points[0].Set(pt0.x, pt0.y, pt0.z);
					points[1].Set(pt1.x, pt1.y, pt1.z);
					points[2].Set(pt2.x, pt2.y, pt2.z);
					points[3].Set(pt3.x, pt3.y, pt3.z);

					if ( TheGlobalData->m_featherWater )
					{
						for (int r = 0; r < TheGlobalData->m_featherWater; ++r)
						{
							WaterTrapezoidBatchEntry entry;
							entry.points[0] = points[0];
							entry.points[1] = points[1];
							entry.points[2] = points[2];
							entry.points[3] = points[3];
							trapezoids.push_back(entry);
							points[0].Z += (FEATHER_THICKNESS/TheGlobalData->m_featherWater);
						}
					}

					else
					{
						WaterTrapezoidBatchEntry entry;
						entry.points[0] = points[0];
						entry.points[1] = points[1];
						entry.points[2] = points[2];
						entry.points[3] = points[3];
						trapezoids.push_back(entry);
					}
				}
			}
		}
	}

	drawTrapezoidWaterBatch(trapezoids);
}

//-------------------------------------------------------------------------------------------------
/** Renders (draws) the sky plane.  Will apply current time-of-day settings including
	* some simple UV scrolling animation. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::renderSky()
{
	Int timeNow,timeDiff;
	Real fu,fv;

	Setting *setting=&m_settings[m_tod];

	timeNow=timeGetTime();

	timeDiff=timeNow-m_LastUpdateTime;
	m_LastUpdateTime=timeNow;

	m_uOffset += timeDiff * setting->uScrollPerMs * setting->skyTexelsPerUnit;
	m_vOffset += timeDiff * setting->vScrollPerMs * setting->skyTexelsPerUnit;

	//clamp uv coordinate into 0,1 range
	m_uOffset = m_uOffset - (Real)((Int) m_uOffset);
	m_vOffset = m_vOffset - (Real)((Int) m_vOffset);

	fu= m_uOffset + (SKYPLANE_SIZE * 2) * setting->skyTexelsPerUnit;
	fv= m_vOffset + (SKYPLANE_SIZE * 2) * setting->skyTexelsPerUnit;


	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	g_renderBackend->Set_Material(vmat);
	REF_PTR_RELEASE(vmat);

	ShaderClass m_shader2=ShaderClass::_PresetOpaqueShader;
	m_shader2.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	m_shader2.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);	//no need to check against z-buffer, sky always rendered first.
	m_shader2.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);	//sky is always behind everything so no need to update z-buffer

	g_renderBackend->Set_Shader(m_shader2);

	g_renderBackend->Set_Texture(0,setting->skyTexture);

	//draw an infinite sky plane
	DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC,dynamic_fvf_type,4);
	{
		DynamicVBAccessClass::WriteLockClass lock(&vb_access);
		VertexFormatXYZNDUV2* verts=lock.Get_Formatted_Vertex_Array();
		if(verts)
		{
			verts[0].x=-SKYPLANE_SIZE;
			verts[0].y=SKYPLANE_SIZE;
			verts[0].z=SKYPLANE_HEIGHT;
			verts[0].u1=m_uOffset;
			verts[0].v1=fv;
			verts[0].diffuse=setting->vertex01Diffuse;

			verts[1].x=SKYPLANE_SIZE;
			verts[1].y=SKYPLANE_SIZE;
			verts[1].z=SKYPLANE_HEIGHT;
			verts[1].u1=fu;
			verts[1].v1=fv;
			verts[1].diffuse=setting->vertex11Diffuse;

			verts[2].x=SKYPLANE_SIZE;
			verts[2].y=-SKYPLANE_SIZE;
			verts[2].z=SKYPLANE_HEIGHT;
			verts[2].u1=fu;
			verts[2].v1=m_vOffset;
			verts[2].diffuse=setting->vertex10Diffuse;

			verts[3].x=-SKYPLANE_SIZE;
			verts[3].y=-SKYPLANE_SIZE;
			verts[3].z=SKYPLANE_HEIGHT;
			verts[3].u1=m_uOffset;
			verts[3].v1=m_vOffset;
			verts[3].diffuse=setting->vertex00Diffuse;
		}
	}

	g_renderBackend->Set_Index_Buffer(m_indexBuffer,0);
	g_renderBackend->Set_Vertex_Buffer(vb_access);

	Matrix3D tm(1);
	tm.Set_Translation(Vector3(0,0,0));
	g_renderBackend->Set_Transform(RB_TRANSFORM_WORLD,tm);

	g_renderBackend->Draw_Triangles(	0,2, 0,	4);	//draw a quad, 2 triangles, 4 verts
}

//-------------------------------------------------------------------------------------------------
/** Renders (draws) the sky body.  Used for moon and sun.  We rotate the image
	* so that it always faces the camera.  This removes perspective and helps hide that
	* it's a flat image. */
//-------------------------------------------------------------------------------------------------
///	@todo: Add code to render properly sorted sun sky body.
void WaterRenderObjClass::renderSkyBody(Matrix3D *mat)
{
	Vector3 cPos;

	Vector3 pView,pRight,pUp,pPos(SKYBODY_X,SKYBODY_Y,SKYBODY_HEIGHT);

	mat->Get_Translation(&cPos);

	pView=cPos-pPos;	//billboard to camera
	pView.Normalize();	//particle view direction

	Vector3 WorldUp(0,0,-1);	///@todo: hacked so only works for reflections across xy plane

#ifdef ALLOW_TEMPORARIES
	Vector3 rotAxis=Vector3::Cross_Product(WorldUp,pView);	//get axis of rotation.
	rotAxis.Normalize();
#else
	Vector3 rotAxis;
	Vector3::Normalized_Cross_Product(WorldUp, pView, &rotAxis);
#endif

	Real angle=Vector3::Dot_Product(WorldUp,pView);

	angle = acos(angle);


	Matrix3D tm(1);
	tm.Set(rotAxis,angle);
	tm.Adjust_Translation(Vector3(SKYBODY_X,SKYBODY_Y,SKYBODY_HEIGHT));


	g_renderBackend->Set_Transform(RB_TRANSFORM_WORLD,tm);


	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	g_renderBackend->Set_Material(vmat);
	REF_PTR_RELEASE(vmat);

	ShaderClass m_shader2=ShaderClass::_PresetAlphaShader;
	m_shader2.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	m_shader2.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);	//no need to check against z-buffer, sky always rendered first.
	m_shader2.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);	//sky is always behind everything so no need to update z-buffer

	g_renderBackend->Set_Shader(m_shader2);


//	g_renderBackend->Set_Shader(ShaderClass::/*_PresetAdditiveShader*//*_PresetOpaqueShader*/_PresetAlphaShader);
//	g_renderBackend->Set_Texture(0,setting->skyBodyTexture);

	g_renderBackend->Set_Texture(0,m_alphaClippingTexture);

	//draw an infinite sky plane
	DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC,dynamic_fvf_type,4);
	{
		DynamicVBAccessClass::WriteLockClass lock(&vb_access);
		VertexFormatXYZNDUV2* verts=lock.Get_Formatted_Vertex_Array();
		if(verts)
		{
			verts[0].x=-SKYBODY_SIZE;
			verts[0].y=SKYBODY_SIZE;
			verts[0].z=0;
			verts[0].u2=0;
			verts[0].v2=1;
			verts[0].diffuse=0xffffffff;

			verts[1].x=SKYBODY_SIZE;
			verts[1].y=SKYBODY_SIZE;
			verts[1].z=0;
			verts[1].u2=1;
			verts[1].v2=1;
			verts[1].diffuse=0xffffffff;

			verts[2].x=SKYBODY_SIZE;
			verts[2].y=-SKYBODY_SIZE;
			verts[2].z=0;
			verts[2].u2=1;
			verts[2].v2=0;
			verts[2].diffuse=0xffffffff;

			verts[3].x=-SKYBODY_SIZE;
			verts[3].y=-SKYBODY_SIZE;
			verts[3].z=0;
			verts[3].u2=0;
			verts[3].v2=0;
			verts[3].diffuse=0xffffffff;
		}
	}

	g_renderBackend->Set_Index_Buffer(m_indexBuffer,0);
	g_renderBackend->Set_Vertex_Buffer(vb_access);

	g_renderBackend->Draw_Triangles(	0,2, 0,	4);	//draw a quad, 2 triangles, 4 verts
}

//Defines for procedural water animation.
#define WATER_FREQ	(2.0*3.2831/4.0)	//2pi (full cycle) cover 4 units
#define WATER_AMP	(1.0f)
#define	WATER_OFFSET (0.1f)

//-------------------------------------------------------------------------------------------------
/** Renders (draws) the water surface mesh geometry.
	*	This is a work-in-progress!  Do not use this code! */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::renderWaterMesh()
{

	if (!m_doWaterGrid)
		return;	//the water grid is disabled.

	Setting *setting=&m_settings[m_tod];

	WaterMeshData *pData;
	Int	mx=m_gridCellsX+1;
	Int my=m_gridCellsY+1;
	Int i,j;

	Real cellSizeX=m_gridCellSize;
	Real cellSizeY=m_gridCellSize;
//	Real	uScale2=5.0f*setting->waterRepeatCount/(128.0f)*cellSizeX/10.0f;
//	Real	vScale2=5.0f*setting->waterRepeatCount/(128.0f)*cellSizeY/10.0f;

	//Old waterRepeatCount settings in INI were based on 128x128 water grid of cellsize=10
	//Scale values to correct size.
	Real	uScale=setting->waterRepeatCount/(128.0f)*cellSizeX/10.0f*0.2f;
	Real	vScale=setting->waterRepeatCount/(128.0f)*cellSizeY/10.0f*0.2f;

	Vector3	nx(cellSizeX*2.0f,0,0);
	Vector3 ny(0,cellSizeY*2.0f,0);
	Vector3 C;

#ifdef DO_WATER_SIMULATION		//Debug code used to create a dummy water animation
	//
	// Mark: If you re-enable this water simulation, you might want to consider moving
	// this code to the update() method of the water render object (Colin)
	//

	static Real PhasePerFrameX=0.1f;
	static Real PhasePerFrameY=0.1f;

	//update the mesh heights for this frame (update buffer is 2 samples wider/taller due to border)
	for (j=0,pData=m_meshData; j<(my+2); j++)
	{
		for (i=0; i<(mx+2); i++)
		{
			//*pData = WATER_AMP * sin(WATER_FREQ*(0.7f*i + 0.7f*j) - PhasePerFrame);

			pData->height=WATER_OFFSET+WATER_AMP*(sin((float)i*WATER_FREQ*0.4+PhasePerFrameX*0.5)+sin((float)i*WATER_FREQ*0.6+PhasePerFrameX*0.2)+sin((float)j*WATER_FREQ+PhasePerFrameX)+sin((float)j*WATER_FREQ*0.7+PhasePerFrameX*0.3));
//			*pData=WATER_OFFSET+WATER_AMP*(sin((float)i*WATER_FREQ*0.4+PhasePerFrameX*0.5)+sin((float)i*WATER_FREQ*0.6+PhasePerFrameX*0.2)+sin((float)j*WATER_FREQ+PhasePerFrameX)+sin((float)j*WATER_FREQ*0.7+PhasePerFrameX*0.3));
			pData++;
		}
	}

	PhasePerFrameX -= 0.08f;
	PhasePerFrameY -= 0.1f;
#endif

	DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC,dynamic_fvf_type,(unsigned short)(mx*my));
	{
		DynamicVBAccessClass::WriteLockClass lock(&vb_access);
		VertexFormatXYZNDUV2 *vb=lock.Get_Formatted_Vertex_Array();
		if (vb == nullptr)
		{
			return;
		}
	Int diffuse;
	diffuse = setting->waterDiffuse&0x00ffffff;
	Int alpha = (setting->waterDiffuse & 0xff000000)>>24;
	// Reduce alpha for wave mesh
	alpha -= 0x20;
	diffuse |= alpha<<24;

	//I pulled some of these constants out of the loops for speed:
	Real uvCosScale=0.02*cos(3*m_riverVOrigin);
	Real sinOffset=25*m_riverVOrigin;
	Real originScale=m_riverVOrigin/vScale;
	Real bumpSizeDiv=cellSizeY/BUMP_SIZE;
	Real bumpSizeDiv2=0.3f*cellSizeY/BUMP_SIZE;

	//Data has a 1 vertex padding all around it so we don't need to special-case edges.  Improves performance
	for (j=0,pData=m_meshData+mx+2+1; j<my; j++,pData+=2)	//skip 2 horizontal border samples after each row
	{
		Real y=(float)j*cellSizeY;
		Real v1Offset=m_riverVOrigin+(float)j*vScale + uvCosScale*WWMath::Fast_Sin(sinOffset+y*PI/(8*MAP_XY_FACTOR));
		Real v2Offset=((float)j+originScale)*bumpSizeDiv + (float)j*bumpSizeDiv2;

		for (i=0; i<mx; i++)
		{
			//compute normal by looking at 4 vertex neightbors
#ifdef USE_MESH_NORMALS
			nx.Z=(pData+1)->height - (pData-1)->height;
			ny.Z=(pData+mx+2)->height - (pData-mx-2)->height;
//			nx.Z=*(pData+1)-*(pData-1);
//			ny.Z=*(pData+mx+2)-*(pData-mx-2);
			Vector3::Cross_Product(nx,ny,&C);
			C.Normalize();
			vb->nx = C.X;
			vb->ny = C.Y;
			vb->nz = C.Z;
#elif defined(GGC_BGFX_STANDALONE)
			vb->nx = 0.0f;
			vb->ny = 0.0f;
			vb->nz = 1.0f;
#endif
			Real x = (float)i*cellSizeX;
			vb->x=	x;
			vb->y=	y;
			vb->z=  pData->height;//WATER_OFFSET+WATER_AMP*(sin((float)i*WATER_FREQ+PhasePerFrame)+cos((float)j*WATER_FREQ+PhasePerFrame));

			vb->diffuse = diffuse;
#ifdef SCROLL_UV
//			vb->diffuse=0x80ffffff;
			vb->u1=(float)i*uScale;
			vb->v1=v1Offset;

			//old slow version
			//vb->v1=m_riverVOrigin+(float)j*vScale + 0.02*cos(3*m_riverVOrigin)*sin(25*m_riverVOrigin+y*PI/(8*MAP_XY_FACTOR));

//			vb->u2=m_initialGridU2+(float)i*uScale2;
//			vb->v2=m_initialGridV2+(float)j*vScale2;
#else
			vb->u1=(float)i*uScale;
			vb->v1=(float)j*vScale;
#endif
			vb->u2=(float)(i)*cellSizeX/BUMP_SIZE;
			vb->v2=v2Offset;
			//old slow code
			//vb->v2=(float)(j+m_riverVOrigin/vScale )*cellSizeY/BUMP_SIZE+ 0.3f*(float)j*cellSizeY/BUMP_SIZE;
			vb++;
			pData++;
		}
	}

	}

	g_renderBackend->Set_Transform(RB_TRANSFORM_WORLD,Transform);	//position the water surface
	g_renderBackend->Set_Material(m_meshVertexMaterialClass);

	ShaderClass::CullModeType oldCullMode=m_shaderClass.Get_Cull_Mode();

	ShaderClass::DepthMaskType oldDepthMask=m_shaderClass.Get_Depth_Mask();
	m_shaderClass.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);	//disable writing to z-buffer to prevent particle clipping.

	m_shaderClass.Set_Cull_Mode(ShaderClass::CULL_MODE_ENABLE);	//water should be visible from both sides

	g_renderBackend->Set_Shader(m_shaderClass);
	setupFlatWaterShader();

	if (m_waterMeshIndexBuffer == nullptr)
	{
		return;
	}
	g_renderBackend->Set_Index_Buffer(m_waterMeshIndexBuffer,0);
	g_renderBackend->Set_Vertex_Buffer(vb_access);


	if (TheTerrainRenderObject->getShroud() && !m_trapezoidWaterPixelShader)
	{	//we have a shroud to apply and can't do it inside the pixel shader.
		//so do it in stage1
		W3DShaderManager::setTexture(0,TheTerrainRenderObject->getShroud()->getShroudTexture());
		W3DShaderManager::setShader(W3DShaderManager::ST_SHROUD_TEXTURE, 1);

		//modulate with shroud texture
		g_renderBackend->Set_Texture_Color_Argument(1, 1, RB_TEXARG_TEXTURE);	//stage 1 texture
		g_renderBackend->Set_Texture_Color_Argument(1, 2, RB_TEXARG_CURRENT);	//previous stage texture
		g_renderBackend->Set_Texture_Color_Operation(1, RB_TEXOP_MODULATE);
		g_renderBackend->Set_Texture_Alpha_Operation(1, RB_TEXOP_MODULATE);

		//Shroud shader uses z-compare of EQUAL which wouldn't work on water because it doesn't
		//write to the zbuffer.  Change to LESSEQUAL.
		g_renderBackend->Set_Depth_Func(RB_CMP_LESS_EQUAL);
		g_renderBackend->Draw_Strip(0,m_numIndices-2,0,mx*my);
		g_renderBackend->Set_Depth_Func(RB_CMP_EQUAL);
		W3DShaderManager::resetShader(W3DShaderManager::ST_SHROUD_TEXTURE);
	}
	else
	{
		g_renderBackend->Draw_Strip(0,m_numIndices-2,0,mx*my);
	}

	Debug_Statistics::Record_DX8_Polys_And_Vertices(m_numIndices-2,mx*my,ShaderClass::_PresetOpaqueShader);
	if (m_trapezoidWaterPixelShader) g_renderBackend->Set_Pixel_Shader(0);

	g_renderBackend->Set_Texture(0,nullptr);
	g_renderBackend->Set_Texture(1,nullptr);
	ShaderClass::Invalidate();
	m_shaderClass.Set_Cull_Mode(oldCullMode);	//water should be visible from both sides

	// restore shader to old mask
	m_shaderClass.Set_Depth_Mask(oldDepthMask);

	//W3DShaderManager::resetShader(W3DShaderManager::ST_SHROUD_TEXTURE);

}

inline void WaterRenderObjClass::setGridVertexHeight(Int x, Int y, Real value)
{
	DEBUG_ASSERTCRASH( x < (m_gridCellsX+1) && y < (m_gridCellsY+1), ("Invalid Water Mesh Coordinates") );

	if (m_meshData)
	{
		m_meshData[(y+1)*(m_gridCellsX+1+2)+x+1].height = value;
	}
}

void WaterRenderObjClass::setGridHeightClamps(Real minz, Real maxz)
{
	m_minGridHeight = minz;
	m_maxGridHeight = maxz;
}

void WaterRenderObjClass::addVelocity( Real worldX, Real worldY,
																			 Real zVelocity, Real preferredHeight )
{

	if( m_doWaterGrid)
	{
		Real gx,gy;
		Real minX,maxX,minY,maxY;
		Int x,y;
		WaterMeshData *meshPoint;
		m_disableRiver = true;

		//check if center falls within grid bounds
		if (worldToGridSpace(worldX, worldY, gx, gy))
		{

			//find extents of influence
			minX = floorf(gx - m_gridChangeMaxRange);
			if (minX < 0 )
				minX = 0;	//clamp extent to fall within box
			maxX = ceilf(gx + m_gridChangeMaxRange);
			if (maxX > m_gridCellsX)
				maxX = m_gridCellsX;	//clamp extent to fall within box

			minY = floorf(gy - m_gridChangeMaxRange);
			if (minY < 0 )
				minY = 0;	//clamp extent to fall within box
			maxY = ceilf(gy + m_gridChangeMaxRange);
			if (maxY > m_gridCellsY)
				maxY = m_gridCellsY;	//clamp extent to fall within box

			for (y=minY; y<=maxY; y++)
			{
				for (x=minX; x<=maxX; x++)
				{

					// get the mesh point that we're concerned with
					meshPoint = &m_meshData[ (y + 1) * (m_gridCellsX + 1 + 2) + x + 1 ];

					// we now have a new preferred height
					meshPoint->preferredHeight = preferredHeight;

					//
					// set the velocity of this point based on the distance from the center of the
					// "core" point for this call
					//
					meshPoint->velocity = meshPoint->velocity + zVelocity;

					// this point is now "in motion"
					BitSet( meshPoint->status, WaterRenderObjClass::IN_MOTION );

				}
			}

			//
			// the mesh data is now dirty, we need to pass through the velocity field
			// during an update phase to update the positions
			//
			m_meshInMotion = TRUE;

		}

	}

}

void WaterRenderObjClass::changeGridHeight(Real wx, Real wy, Real delta)
{
	Real gx,gy;
	Real *oldData;
	Real newData;
	Real distance;
	Real minX,maxX,minY,maxY;
	Int x,y;

	//check if center falls within grid bounds
	if (worldToGridSpace(wx, wy, gx, gy))
	{	//find extents of influence
		minX = floorf(gx - m_gridChangeMaxRange);
		if (minX < 0 )
			minX = 0;	//clamp extent to fall within box
		maxX = ceilf(gx + m_gridChangeMaxRange);
		if (maxX > m_gridCellsX)
			maxX = m_gridCellsX;	//clamp extent to fall within box

		minY = floorf(gy - m_gridChangeMaxRange);
		if (minY < 0 )
			minY = 0;	//clamp extent to fall within box
		maxY = ceilf(gy + m_gridChangeMaxRange);
		if (maxY > m_gridCellsY)
			maxY = m_gridCellsY;	//clamp extent to fall within box

		for (y=minY; y<=maxY; y++)
		{
			for (x=minX; x<=maxX; x++)
			{	oldData = &m_meshData[(y+1)*(m_gridCellsX+1+2)+x+1].height;
				distance = (gx - (Real)x)*(gx - (Real)x) + (gy - (Real)y)*(gy - (Real)y);
				distance = sqrt(distance);
				newData = *oldData + 1.0f/(m_gridChangeAtt0+m_gridChangeAtt1*distance+distance*distance*m_gridChangeAtt2)*delta;
				//Clamp to min/max values
				if (newData < m_minGridHeight)
					newData = m_minGridHeight;
				if (newData > m_maxGridHeight)
					newData = m_maxGridHeight;
				*oldData = newData;
			}
		}
	}
}

void WaterRenderObjClass::setGridChangeAttenuationFactors(Real a, Real b, Real c, Real range)
{
	m_gridChangeAtt0 = a;
	m_gridChangeAtt1 = b;
	m_gridChangeAtt2 = c;
	m_gridChangeMaxRange = range/m_gridCellSize;	//convert range to grid space
}

void WaterRenderObjClass::setGridTransform(Real angle, Real x, Real y, Real z)
{
	m_gridDirectionX = Vector2(1.0f,0.0f);

	m_gridOrigin.X = x;
	m_gridOrigin.Y = y;

	Matrix3D xform(1);
	xform.Rotate_Z(angle);

	m_gridDirectionX.X = xform.Get_X_Vector().X;
	m_gridDirectionX.Y = xform.Get_X_Vector().Y;

	m_gridDirectionY.X = xform.Get_Y_Vector().X;
	m_gridDirectionY.Y = xform.Get_Y_Vector().Y;

	xform.Set_Translation(Vector3(x,y,z));
	Set_Transform(xform);
}

void WaterRenderObjClass::setGridTransform(const Matrix3D *transform )
{

	if( transform )
		Set_Transform( *transform );

}

void WaterRenderObjClass::getGridTransform(Matrix3D *transform )
{

	if( transform )
		*transform = Get_Transform();

}

void WaterRenderObjClass::setGridResolution(Real gridCellsX, Real gridCellsY, Real cellSize)
{
	m_gridCellSize=cellSize;

	if (m_gridCellsX != gridCellsX || m_gridCellsY != gridCellsY)
	{	//resolution has changed
		m_gridCellsX=gridCellsX;
		m_gridCellsY=gridCellsY;

		if (m_meshData)
		{

			delete [] m_meshData;//free previously allocated grid and allocate new size
			m_meshData = nullptr;	 // must set to null so that we properly re-allocate
			m_meshDataSize = 0;

			Bool enable = m_doWaterGrid;
			enableWaterGrid(true);	// allocates buffers.
			m_doWaterGrid = enable;

		}
	}
}

void WaterRenderObjClass::getGridResolution( Real *gridCellsX, Real *gridCellsY, Real *cellSize )
{

	if( gridCellsX )
		*gridCellsX = m_gridCellsX;
	if( gridCellsY )
		*gridCellsY = m_gridCellsY;
	if( cellSize )
		*cellSize = m_gridCellSize;

}

static Real wobble(Real baseV, Real offset, Bool wobble)
{
	if (!wobble) return 0;
	offset = sin(2*PI*baseV - 3*offset);
	return offset/22;
}

/**Utility function used to query water heights in a manner that works in both RTS and WB.*/
Real WaterRenderObjClass::getWaterHeight(Real x, Real y)
{
	const WaterHandle *waterHandle = nullptr;
	Real waterZ = 0.0f;
	ICoord3D iLoc;

	iLoc.x = REAL_TO_INT_FLOOR( x + 0.5f );
	iLoc.y = REAL_TO_INT_FLOOR( y + 0.5f );
	iLoc.z = 0;

	for( PolygonTrigger *pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext() )
	{

		if( !pTrig->isWaterArea() )
			continue;

		// See if point is in a water area
		if( pTrig->pointInTrigger( iLoc ) )
		{

			if( pTrig->getPoint( 0 )->z >= waterZ )
			{

				waterZ = pTrig->getPoint( 0 )->z;
				waterHandle = pTrig->getWaterHandle();

			}

		}

	}

	if (waterHandle)
		return waterHandle->m_polygon->getPoint( 0 )->z;
	return INVALID_WATER_HEIGHT;	//point not underwater
}

//-------------------------------------------------------------------------------------------------
//Draw a many sided river polygon.
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::drawRiverWater(PolygonTrigger *pTrig)
{
	g_renderBackend->Invalidate_Cached_Render_States();	///@todo: Figure out why rivers don't draw without reset of all states.

	Int rectangleCount = pTrig->getNumPoints()/2;
	rectangleCount--;

	Real bumpFactor = 5;
	static Bool doWobble = true;

	if (m_disableRiver) return;
	m_drawingRiver = true;

	//allocate 2 triangles per side with 3 indices per triangle
	DynamicIBAccessClass ib_access(BUFFER_TYPE_DYNAMIC,(rectangleCount+1)*2*3);
	{
		DynamicIBAccessClass::WriteLockClass lockib(&ib_access);
 		UnsignedShort *curIb = lockib.Get_Index_Array();
		for (Int i=0; i<rectangleCount; i++)
		{
			//triangle 1
			curIb[0] = i*2;
			curIb[1] = i*2+1;
			curIb[2] = i*2+3;

			//triangle 2
			curIb[3] = i*2;
			curIb[4] = i*2+3;
			curIb[5] = i*2+2;

			curIb += 6;	//skip the 6 indices we just added.
		}
	}


	Real shadeR=TheWaterTransparency->m_standingWaterColor.red;
	Real shadeG=TheWaterTransparency->m_standingWaterColor.green;
	Real shadeB=TheWaterTransparency->m_standingWaterColor.blue;

	//If the water color is not overridden, use legacy lighting code.
	if ( shadeR==1.0f && shadeG==1.0f && shadeB==1.0f)
	{
		shadeR = TheGlobalData->m_terrainAmbient[0].red;
		shadeG = TheGlobalData->m_terrainAmbient[0].green;
		shadeB = TheGlobalData->m_terrainAmbient[0].blue;

		//Add in diffuse lighting from each terrain light
		for (Int lightIndex=0; lightIndex < TheGlobalData->m_numGlobalLights; lightIndex++)
		{
			if (-TheGlobalData->m_terrainLightPos[lightIndex].z > 0)
			{	shadeR += -TheGlobalData->m_terrainLightPos[lightIndex].z * TheGlobalData->m_terrainDiffuse[lightIndex].red;
				shadeG += -TheGlobalData->m_terrainLightPos[lightIndex].z * TheGlobalData->m_terrainDiffuse[lightIndex].green;
				shadeB += -TheGlobalData->m_terrainLightPos[lightIndex].z * TheGlobalData->m_terrainDiffuse[lightIndex].blue;
			}
		}

		//Get water material colors
		Real waterShadeR = (m_settings[m_tod].waterDiffuse & 0xff) / 255.0f;
		Real waterShadeG = ((m_settings[m_tod].waterDiffuse >> 8) & 0xff) / 255.0f;
		Real waterShadeB = ((m_settings[m_tod].waterDiffuse >> 16) & 0xff) / 255.0f;

		shadeR=shadeR*waterShadeR*255.0f;
		shadeG=shadeG*waterShadeG*255.0f;
		shadeB=shadeB*waterShadeB*255.0f;
	}
	else
	{
		shadeR=shadeR*255.0f;
		shadeG=shadeG*255.0f;
		shadeB=shadeB*255.0f;

		if (shadeR == 0 && shadeG == 0 && shadeB == 0)
		{	//special case where we disable lighting
			shadeR=255;
			shadeG=255;
			shadeB=255;
		}
	}

	Int diffuse=REAL_TO_INT(shadeB) | (REAL_TO_INT(shadeG) << 8) | (REAL_TO_INT(shadeR) << 16);

	//Keep diffuse from lighting calculations but substitute custom alpha
	diffuse |= m_settings[m_tod].waterDiffuse & 0xff000000;	//copy alpha/opacity from ini setting

	Int innerNdx = pTrig->getRiverStart();
	Int outerNdx = innerNdx+1;

	Real endLen=0;
	Real totalLen=0;
	Int i;
	for (i=0; i<pTrig->getNumPoints()-1; i++) {
		ICoord3D innerPt = *pTrig->getPoint(i);
		ICoord3D outerPt = *pTrig->getPoint(i+1);
		Real dx = innerPt.x-outerPt.x;
		Real dy = innerPt.y-outerPt.y;
		Real curLen = sqrt(dx*dx+dy*dy);
		totalLen += curLen;
		if ( i==innerNdx) {
			endLen = curLen;
		}
	}
	bumpFactor = endLen/BUMP_SIZE;

	Real lengthOfRiver = (totalLen/2)-endLen;
	Real repeatCount = lengthOfRiver / (endLen);

	Real vScale=(Real)repeatCount/(Real)rectangleCount;

#define HEIGHT_TO_USE (0.5f)
	if (innerNdx >= pTrig->getNumPoints()-1) return;
	//allocate 2 vertices per side
	DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC,dynamic_fvf_type,(rectangleCount+1)*2);
	{
		DynamicVBAccessClass::WriteLockClass lock(&vb_access);
		VertexFormatXYZNDUV2* vb=lock.Get_Formatted_Vertex_Array();

		Real constA=3*m_riverVOrigin;

		// TheSuperHackers @bugfix afc-afc0 14/04/2026 Apply shroud per-vertex to avoid double-darkening
		// at river borders.
		W3DShroud *shroud = TheTerrainRenderObject ? TheTerrainRenderObject->getShroud() : nullptr;

		for (i=0; i<(pTrig->getNumPoints()/2); i++)
		{
			Real x,y;
			ICoord3D innerPt = *pTrig->getPoint(outerNdx);
			ICoord3D outerPt = *pTrig->getPoint(innerNdx);
			outerNdx++;
			innerNdx--;
			if (innerNdx<0) {
				innerNdx = pTrig->getNumPoints()-1;
			}
			if (outerNdx >= pTrig->getNumPoints()) {
				outerNdx = 0;
			}
			x=innerPt.x;
			y=innerPt.y;

			vb->x=x;
			vb->y=y;

			vb->z=innerPt.z;

			vb->diffuse = getRiverVertexDiffuse(shroud, x, y, shadeR, shadeG, shadeB, diffuse);

			Real wobbleConst=-m_riverVOrigin+vScale*(Real)i + WWMath::Fast_Sin(2*PI*(vScale*(Real)i) - constA)/22.0f;
 			//old slower version
			//vb->v1=-m_riverVOrigin+vScale*(Real)i + wobble(vScale*i, m_riverVOrigin, doWobble);
			vb->v1=wobbleConst;
			vb->u1=HEIGHT_TO_USE ;
			//old slower version
			//vb->v2 = -m_riverVOrigin+vScale*(Real)i + wobble(vScale*i, m_riverVOrigin, doWobble);
			vb->v2=wobbleConst;
			vb->u2 = 1.0f;
			vb->nx = 0;
			vb->ny = 0;
			vb->nz = 1.0f;
			vb++;

			x=outerPt.x;
			y=outerPt.y;

			vb->x=x;
			vb->y=y;
			vb->z=outerPt.z;

			vb->diffuse = getRiverVertexDiffuse(shroud, x, y, shadeR, shadeG, shadeB, diffuse);
 			//old slower version
			//vb->v1=-m_riverVOrigin+vScale*(Real)i + wobble(vScale*i, m_riverVOrigin, doWobble);
			vb->v1=wobbleConst;
			vb->u1=0;
			//old slower version
 			//vb->v2 = -m_riverVOrigin+vScale*(Real)i + wobble(vScale*i, m_riverVOrigin, doWobble);
			vb->v2 =wobbleConst;
			vb->u2 = 0;
			vb->nx = 0;
			vb->ny = 0;
			vb->nz = 1.0f;
			vb++;

		}
	}

	Matrix3D tm(1);

	g_renderBackend->Set_Transform(RB_TRANSFORM_WORLD,tm);	//position the water surface
	g_renderBackend->Set_Index_Buffer(ib_access,0);
	g_renderBackend->Set_Vertex_Buffer(vb_access);
	g_renderBackend->Set_Texture(0,m_riverTexture);	//set to blue

	setupJbaWaterShader();
	{
		ShaderClass waterShader = ShaderClass::_PresetAlphaShader;
		waterShader.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
		waterShader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
		g_renderBackend->Set_Shader(waterShader);
	}
	g_renderBackend->Override_Alpha_Blend_Enable(true);
	g_renderBackend->Override_Material_Opacity(WATER_MESH_OPACITY);

	//In additive blending we need to use the alpha at the edges of river to darken
	//rgb instead.
	if (TheWaterTransparency->m_additiveBlend)
		g_renderBackend->Set_Blend_Factors(RB_BLEND_SRC_ALPHA, RB_BLEND_ONE);

	if (m_riverWaterPixelShader) g_renderBackend->Set_Pixel_Shader(m_riverWaterPixelShader);
	CullMode cull = g_renderBackend->Get_Cull_Mode();
	g_renderBackend->Set_Cull_Mode(RB_CULL_NONE);



	if (wireframeForDebug) {
		g_renderBackend->Set_Fill_Mode(RB_FILL_WIREFRAME);
	}
	g_renderBackend->Draw_Triangles(	0,rectangleCount*2, 0,	(rectangleCount+1)*2);
	if (wireframeForDebug) {
		g_renderBackend->Set_Fill_Mode(RB_FILL_SOLID);
	}

	if (m_riverWaterPixelShader) g_renderBackend->Set_Pixel_Shader(0);

	//restore blend mode to what W3D expects.
	// TheSuperHackers @fix bobtista 20/04/2026 The flat water path below
	// resets blend factors for both additive and non-additive modes, but
	// this JBA path only reset for additive. On bgfx the DESTALPHA blend
	// that Override_Material_Opacity() sets then leaked into subsequent
	// draws (e.g. the small faction-emblem quad on the command-center
	// bib), producing a black rectangle there. Match the flat water path
	// so non-additive JBA water restores SRC_ALPHA/INV_SRC_ALPHA.
	if (TheWaterTransparency->m_additiveBlend)
		g_renderBackend->Set_Blend_Factors(RB_BLEND_ONE, RB_BLEND_ONE);
	else
		g_renderBackend->Set_Blend_Factors(RB_BLEND_SRC_ALPHA, RB_BLEND_INV_SRC_ALPHA);

	g_renderBackend->Set_Cull_Mode(cull);


}

void WaterRenderObjClass::setupFlatWaterShader()
{
	g_renderBackend->Set_Texture(0,m_riverTexture);
	if (!TheWaterTransparency->m_additiveBlend)
		g_renderBackend->Set_Shader(ShaderClass::_PresetAlphaShader);
	else
		g_renderBackend->Set_Shader(ShaderClass::_PresetAdditiveShader);

	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	g_renderBackend->Set_Material(vmat);
	REF_PTR_RELEASE(vmat);
	m_riverTexture->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_BEST);
	m_riverTexture->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_BEST);
	m_riverTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_BEST);

	g_renderBackend->Apply_Render_State_Changes();	//force update of view and projection matrices

	//Setup shroud to render in same pass as water
	if (m_trapezoidWaterPixelShader)
	{	if (TheTerrainRenderObject->getShroud() && TheTerrainRenderObject->getShroud()->getShroudTexture())
		{
			TextureClass *shroudTexture = TheTerrainRenderObject->getShroud()->getShroudTexture();
			W3DShaderManager::setTexture(0, shroudTexture);
			//Use stage 3 to apply the shroud
			W3DShaderManager::setShader(W3DShaderManager::ST_SHROUD_TEXTURE, 3);
			W3DWater_BindTexture(3, shroudTexture);
			//Shroud shader uses z-compare of EQUAL which wouldn't work on water because it doesn't
			//write to the zbuffer.  Change to LESSEQUAL.
			g_renderBackend->Set_Depth_Func(RB_CMP_LESS_EQUAL);
		}
		else
		{	//Assume no shroud, so stage 3 will be null texture but using actual white because
			//pixel shader on GF4 generates random colors with SetTexture(3,nullptr).
			if (!m_whiteTexture->Is_Initialized())
			{	m_whiteTexture->Init();
				W3DWater_FillWhiteTexture(m_whiteTexture);
			}
			W3DWater_BindTexture(3, m_whiteTexture);
		}
	}

	g_renderBackend->Set_Texture_Alpha_Operation(0, RB_TEXOP_ADD);
	g_renderBackend->Set_Texture_Coord_Source(0, RB_TEXCOORD_MESH_UV, 0);
	W3DWater_SetStageAddress2D(0, RB_TEXTURE_ADDRESS_WRAP);
	g_renderBackend->Set_Texture_Coord_Source(1, RB_TEXCOORD_MESH_UV, 0);

	Bool doSparkles = true;

	if (m_trapezoidWaterPixelShader && doSparkles) {

		if (!m_waterSparklesTexture->Is_Initialized())
			m_waterSparklesTexture->Init();

		W3DWater_BindTexture(1, m_waterSparklesTexture);

		if (!m_waterNoiseTexture->Is_Initialized())
			m_waterNoiseTexture->Init();

		W3DWater_BindTexture(2, m_waterNoiseTexture);

		W3DWater_SetStageAddress2D(1, RB_TEXTURE_ADDRESS_WRAP);

		// Two output coordinates are used.
		W3DWater_SetCameraSpaceTexcoord2(2, 0);
		W3DWater_SetStageAddress2D(2, RB_TEXTURE_ADDRESS_WRAP);

		W3DWater_SetNoiseTextureTransform(2, NOISE_REPEAT_FACTOR, m_riverVOrigin);

	}
	W3DWater_SetStageMinMagFilter(0, RB_TEXTURE_SAMPLE_LINEAR, RB_TEXTURE_SAMPLE_LINEAR);
	W3DWater_SetStageMinMagFilter(1, RB_TEXTURE_SAMPLE_LINEAR, RB_TEXTURE_SAMPLE_LINEAR);
	W3DWater_SetStageMinMagFilter(2, RB_TEXTURE_SAMPLE_LINEAR, RB_TEXTURE_SAMPLE_LINEAR);
	if (m_trapezoidWaterPixelShader){
		const float reflectionFactor[4] = { REFLECTION_FACTOR, REFLECTION_FACTOR, REFLECTION_FACTOR, 1.0f };
		g_renderBackend->Set_Pixel_Shader_Constant(0, &reflectionFactor, 1);
		g_renderBackend->Set_Pixel_Shader(m_trapezoidWaterPixelShader);
	}
}

//-------------------------------------------------------------------------------------------------
//Draw a 4 sided flat water area.
//-------------------------------------------------------------------------------------------------
static void GetWaterTrapezoidCounts(Vector3 points[4], Int &uCount, Int &vCount, Int &rectangleCount)
{
	Vector3 origin(points[0]);
	Vector3 uVec1(points[1]);
	Vector3 vVec1(points[3]);
	Vector3 uVec2(points[2]);
	Vector3 vVec2(points[2]);
	uVec2 -= vVec1;
	vVec2	-= uVec1;
	uVec1 -= origin;
	vVec1 -= origin;
	uCount = (uVec1.Length()+uVec2.Length()) / (8*MAP_XY_FACTOR);
	if (uCount<1)
	{
		uCount = 1;
	}
	vCount = (vVec1.Length()+vVec2.Length()) / (8*MAP_XY_FACTOR);
	if (vCount<1)
	{
		vCount = 1;
	}

	if (uCount>50)
	{
		uCount = 50;
	}
	if (vCount>50)
	{
		vCount = 50;
	}

	rectangleCount = uCount*vCount;
	uCount++;
	vCount++;
}

void WaterRenderObjClass::drawTrapezoidWaterBatch(const std::vector<WaterTrapezoidBatchEntry> &trapezoids)
{
	if (trapezoids.empty())
	{
		return;
	}

	Real	waterFactor=150;
	Real shadeR=TheWaterTransparency->m_standingWaterColor.red;
	Real shadeG=TheWaterTransparency->m_standingWaterColor.green;
	Real shadeB=TheWaterTransparency->m_standingWaterColor.blue;

	//If the water color is not overridden, use legacy lighting code.
	if ( shadeR==1.0f && shadeG==1.0f && shadeB==1.0f)
	{
		shadeR = TheGlobalData->m_terrainAmbient[0].red;
		shadeG = TheGlobalData->m_terrainAmbient[0].green;
		shadeB = TheGlobalData->m_terrainAmbient[0].blue;

		//Add in diffuse lighting from each terrain light
		for (Int lightIndex=0; lightIndex < TheGlobalData->m_numGlobalLights; lightIndex++)
		{
			if (-TheGlobalData->m_terrainLightPos[lightIndex].z > 0)
			{
				shadeR += -TheGlobalData->m_terrainLightPos[lightIndex].z * TheGlobalData->m_terrainDiffuse[lightIndex].red;
				shadeG += -TheGlobalData->m_terrainLightPos[lightIndex].z * TheGlobalData->m_terrainDiffuse[lightIndex].green;
				shadeB += -TheGlobalData->m_terrainLightPos[lightIndex].z * TheGlobalData->m_terrainDiffuse[lightIndex].blue;
			}
		}

		//Get water material colors
		Real waterShadeR = (m_settings[m_tod].waterDiffuse & 0xff) / 255.0f;
		Real waterShadeG = ((m_settings[m_tod].waterDiffuse >> 8) & 0xff) / 255.0f;
		Real waterShadeB = ((m_settings[m_tod].waterDiffuse >> 16) & 0xff) / 255.0f;

		shadeR=shadeR*waterShadeR*255.0f;
		shadeG=shadeG*waterShadeG*255.0f;
		shadeB=shadeB*waterShadeB*255.0f;
	}
	else
	{
		shadeR=shadeR*255.0f;
		shadeG=shadeG*255.0f;
		shadeB=shadeB*255.0f;

		if (shadeR == 0 && shadeG == 0 && shadeB == 0)
		{
			//special case where we disable lighting
			shadeR=255;
			shadeG=255;
			shadeB=255;
		}
	}

	Int diffuse=REAL_TO_INT(shadeB) | (REAL_TO_INT(shadeG) << 8) | (REAL_TO_INT(shadeR) << 16);

	//Keep diffuse from lighting calculations but substitute custom alpha
	diffuse |= m_settings[m_tod].waterDiffuse & 0xff000000;	//copy alpha/opacity from ini setting

	const size_t maxBatchElements = 60000;
	size_t batchStart = 0;
	while (batchStart < trapezoids.size())
	{
		size_t batchEnd = batchStart;
		size_t totalVertices = 0;
		size_t totalIndices = 0;
		Int totalRectangleCount = 0;
		while (batchEnd < trapezoids.size())
		{
			Vector3 points[4];
			points[0] = trapezoids[batchEnd].points[0];
			points[1] = trapezoids[batchEnd].points[1];
			points[2] = trapezoids[batchEnd].points[2];
			points[3] = trapezoids[batchEnd].points[3];
			Int uCount;
			Int vCount;
			Int rectangleCount;
			GetWaterTrapezoidCounts(points, uCount, vCount, rectangleCount);
			const size_t patchVertices = uCount * vCount;
			const size_t patchIndices = rectangleCount * 6;
			if (batchEnd > batchStart
				&& (totalVertices + patchVertices > maxBatchElements
					|| totalIndices + patchIndices > maxBatchElements))
			{
				break;
			}
			totalVertices += patchVertices;
			totalIndices += patchIndices;
			totalRectangleCount += rectangleCount;
			batchEnd++;
		}

		UnsignedShort batchIndexCount = static_cast<UnsignedShort>(totalIndices);
		UnsignedShort batchVertexCount = static_cast<UnsignedShort>(totalVertices);

		DynamicIBAccessClass ib_access(BUFFER_TYPE_DYNAMIC,batchIndexCount);
		{
			DynamicIBAccessClass::WriteLockClass lockib(&ib_access);
			UnsignedShort *curIb = lockib.Get_Index_Array();
			UnsignedShort vertexBase = 0;
			for (size_t patchIndex = batchStart; patchIndex < batchEnd; ++patchIndex)
			{
				Vector3 points[4];
				points[0] = trapezoids[patchIndex].points[0];
				points[1] = trapezoids[patchIndex].points[1];
				points[2] = trapezoids[patchIndex].points[2];
				points[3] = trapezoids[patchIndex].points[3];
				Int uCount;
				Int vCount;
				Int rectangleCount;
				GetWaterTrapezoidCounts(points, uCount, vCount, rectangleCount);
				for (Int j=0; j<vCount-1; j++)
				{
					for (Int i=0; i<uCount-1; i++)
					{
						//triangle 1
						curIb[0] = vertexBase + (j)*uCount + i;
						curIb[1] = vertexBase + (j+1)*uCount + i+1;
						curIb[2] = vertexBase + (j+1)*uCount + i;

						//triangle 2
						curIb[3] = vertexBase + (j)*uCount + i;
						curIb[4] = vertexBase + (j)*uCount + i+1;
						curIb[5] = vertexBase + (j+1)*uCount + i+1;

						curIb += 6;	//skip the 6 indices we just added.
					}
				}
				vertexBase += static_cast<UnsignedShort>(uCount * vCount);
			}
		}

		DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC,dynamic_fvf_type,batchVertexCount);
		{
			DynamicVBAccessClass::WriteLockClass lock(&vb_access);
			VertexFormatXYZNDUV2* vb=lock.Get_Formatted_Vertex_Array();
			for (size_t patchIndex = batchStart; patchIndex < batchEnd; ++patchIndex)
			{
				Vector3 points[4];
				points[0] = trapezoids[patchIndex].points[0];
				points[1] = trapezoids[patchIndex].points[1];
				points[2] = trapezoids[patchIndex].points[2];
				points[3] = trapezoids[patchIndex].points[3];
				Int uCount;
				Int vCount;
				Int rectangleCount;
				GetWaterTrapezoidCounts(points, uCount, vCount, rectangleCount);
				Vector3 origin(points[0]);
				Vector3 uVec1(points[1]);
				Vector3 vVec1(points[3]);
				Vector3 uVec2(points[2]);
				Vector3 vVec2(points[2]);
				uVec2 -= vVec1;
				vVec2	-= uVec1;
				uVec1 -= origin;
				vVec1 -= origin;

				if ( TheGlobalData->m_featherWater )
				{
					Real phase = 0;
					Real mapCoeff = PI/(4*MAP_XY_FACTOR);
					Real wave = 0;
					Real amplitude = 0.5f;

					Int Alpha = 0;
					if ( TheGlobalData->m_featherWater == 5)
					{
						Alpha = 80;
					}
					if ( TheGlobalData->m_featherWater == 4)
					{
						Alpha = 110;
					}
					if ( TheGlobalData->m_featherWater == 3)
					{
						Alpha = 140;
					}
					if ( TheGlobalData->m_featherWater == 2)
					{
						Alpha = 200;
					}
					if ( TheGlobalData->m_featherWater == 1)
					{
						Alpha = 255;
					}

					//Keep diffuse from lighting calculations but substitute custom alpha
					Int customDiffuse = (diffuse & 0x00ffffff) | (Alpha<< 24);//(0x80 << 16)|(0x90 << 8)|0xa0;

					for (Int j=0; j<vCount; j++)
					{
						Real dv = j;
						dv /= (vCount-1);
						for (Int i=0; i<uCount; i++)
						{
							Real du = i;
							du /= (uCount-1);
							Vector3 vertex = origin;
							vertex += uVec1*du;
							vertex += vVec1*dv;
							vertex += (dv)*(du)*(vVec2-vVec1);

							vb->x=vertex.X;
							vb->y=vertex.Y;

							// common to all the waving effects
							phase = 25 * m_riverVOrigin + vertex.X * mapCoeff;
							wave = (sin(phase) - 1.0f) * amplitude;

							vb->z = (vertex.Z + wave);
							vb->diffuse = customDiffuse;
							vb->u1 = (vertex.X/waterFactor) + 0.02*cos(11*m_riverVOrigin)*wave;
							vb->v1 = (vertex.Y/waterFactor) + 0.02*cos(5*m_riverVOrigin)*wave;
							vb->u2 = vertex.X/BUMP_SIZE;
							vb->v2 = vertex.Y/BUMP_SIZE + 0.3f*vertex.X/BUMP_SIZE;
							vb->nx = 0;
							vb->ny = 0;
							vb->nz = 1.0f;
							vb++;
						}
					}
				}
				else
				{
					//Pulling some constants out of the inner loops to improve performance -MW
					Real constA=0.02*cos(11*m_riverVOrigin);
					Real constB=0.02*cos(5*m_riverVOrigin);
					Real constC=25*m_riverVOrigin;
					Real ooWaterFactor = 1.0f/waterFactor;
					const Real constD=PI/(4*MAP_XY_FACTOR);
					Real constE=1.0f/(Real)(vCount-1);
					Real constF=1.0f/(Real)(uCount-1);

					for (Int j=0; j<vCount; j++)
					{
						Real dv = (Real)j * constE;

						for (Int i=0; i<uCount; i++)
						{
							Real du = (Real)i * constF;
							Vector3 vertex = origin;
							vertex += uVec1*du;
							vertex += vVec1*dv;
							vertex += (dv)*(du)*(vVec2-vVec1);

							vb->x=vertex.X;
							vb->y=vertex.Y;
							vb->z=vertex.Z;

							vb->diffuse= diffuse;
							vb->u1=vertex.X*ooWaterFactor + constA*WWMath::Fast_Sin(constC+vertex.X*constD);
							vb->v1=vertex.Y*ooWaterFactor + constB*WWMath::Fast_Sin(constC+vertex.Y*constD);
							vb->u2 = vertex.X/BUMP_SIZE;
							vb->v2 = (vertex.Y+0.3f*vertex.X)/BUMP_SIZE;
							vb->nx = 0;
							vb->ny = 0;
							vb->nz = 1.0f;
							vb++;
						}
					}
				}
			}
		}

		Matrix3D tm(1);

		g_renderBackend->Set_Transform(RB_TRANSFORM_WORLD,tm);	//position the water surface
		g_renderBackend->Set_Index_Buffer(ib_access,0);
		g_renderBackend->Set_Vertex_Buffer(vb_access);

		setupFlatWaterShader();
		{
			ShaderClass waterShader = ShaderClass::_PresetAlphaShader;
			waterShader.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
			waterShader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
			g_renderBackend->Set_Shader(waterShader);
		}
		if (m_trapezoidWaterPixelShader)
		{
			g_renderBackend->Set_Pixel_Shader(m_trapezoidWaterPixelShader);
		}
		g_renderBackend->Override_Alpha_Blend_Enable(true);
		g_renderBackend->Override_Material_Opacity(WATER_MESH_OPACITY);

		if (g_renderBackend->Get_Back_Buffer_Format() == WW3D_FORMAT_A8R8G8B8 && TheGlobalData->m_showSoftWaterEdge && TheWaterTransparency->m_transparentWaterDepth !=0)
		{
			if (TheWaterTransparency->m_additiveBlend)
			{
				g_renderBackend->Set_Blend_Factors(RB_BLEND_DEST_ALPHA, RB_BLEND_ONE);
			}
			else
			{
				g_renderBackend->Set_Blend_Factors(RB_BLEND_DEST_ALPHA, RB_BLEND_INV_DEST_ALPHA);
			}
		}

		CullMode cull = g_renderBackend->Get_Cull_Mode();
		g_renderBackend->Set_Cull_Mode(RB_CULL_NONE);

		g_renderBackend->Draw_Triangles(	0,totalRectangleCount*2, 0,	batchVertexCount);

		if (m_trapezoidWaterPixelShader)
		{
			g_renderBackend->Set_Pixel_Shader(0);
		}
		//Restore alpha blend to default values since we may have changed them to feather edges.
		if (!TheWaterTransparency->m_additiveBlend)
		{
			g_renderBackend->Set_Blend_Factors(RB_BLEND_SRC_ALPHA, RB_BLEND_INV_SRC_ALPHA);
		}
		else
		{
			g_renderBackend->Set_Blend_Factors(RB_BLEND_ONE, RB_BLEND_ONE);
		}

		if (TheTerrainRenderObject->getShroud())
		{
			if (m_trapezoidWaterPixelShader)
			{
				W3DShaderManager::resetShader(W3DShaderManager::ST_SHROUD_TEXTURE);
				W3DWater_BindTexture(3, nullptr);
				g_renderBackend->Set_Depth_Func(RB_CMP_EQUAL);
			}
			else
			{
				W3DShaderManager::setTexture(0,TheTerrainRenderObject->getShroud()->getShroudTexture());
				W3DShaderManager::setShader(W3DShaderManager::ST_SHROUD_TEXTURE, 0);
				g_renderBackend->Set_Cull_Mode(RB_CULL_NONE);
				g_renderBackend->Set_Depth_Func(RB_CMP_LESS_EQUAL);
				g_renderBackend->Draw_Triangles(	0,totalRectangleCount*2, 0,	batchVertexCount);
				g_renderBackend->Set_Depth_Func(RB_CMP_EQUAL);
				W3DShaderManager::resetShader(W3DShaderManager::ST_SHROUD_TEXTURE);
			}
		}
		g_renderBackend->Set_Cull_Mode(cull);

		batchStart = batchEnd;
	}
}




//-------------------------------------------------------------------------------------------------
//debug version where moon rotates with the camera	(always upright on screen)
//-------------------------------------------------------------------------------------------------
#if 0
void WaterRenderObjClass::renderSkyBody(Matrix3D *mat)
{
	Vector3 vRight,vUp,V0,V1,V2,V3;

	mat->Get_X_Vector(&vRight);
	mat->Get_Y_Vector(&vUp);

	//calculate offsets from quad center to each of the 4 corners
	//	0-----1
	//  |    /|
	//  |  /  |
	//	|/    |
	//  3-----2
	V0=-vRight+vUp;
	V2=vRight+vUp;
	V2=vRight-vUp;
	V3=-vRight-vUp;

	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	g_renderBackend->Set_Material(vmat);
	REF_PTR_RELEASE(vmat);
	g_renderBackend->Set_Shader(ShaderClass::/*_PresetAdditiveShader*//*_PresetOpaqueShader*/_PresetAlphaShader);
//	g_renderBackend->Set_Texture(0,setting->skyBodyTexture);

	g_renderBackend->Set_Texture(0,m_alphaClippingTexture);

	//draw an infinite sky plane
	DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC,4);
	{
		DynamicVBAccessClass::WriteLockClass lock(&vb_access);
		VertexFormatXYZNDUV2* verts=lock.Get_Formatted_Vertex_Array();
		if(verts)
		{
			verts[0].x=SKYBODY_SIZE*V0.X;
			verts[0].y=SKYBODY_SIZE*V0.Y;
			verts[0].z=SKYBODY_SIZE*V0.Z;
			verts[0].u2=0;
			verts[0].v2=1;
			verts[0].diffuse=0xffffffff;

			verts[1].x=SKYBODY_SIZE*V1.X;
			verts[1].y=SKYBODY_SIZE*V1.Y;
			verts[1].z=SKYBODY_SIZE*V1.Z;
			verts[1].u2=1;
			verts[1].v2=1;
			verts[1].diffuse=0xffffffff;

			verts[2].x=SKYBODY_SIZE*V2.X;
			verts[2].y=SKYBODY_SIZE*V2.Y;
			verts[2].z=SKYBODY_SIZE*V2.Z;
			verts[2].u2=1;
			verts[2].v2=0;
			verts[2].diffuse=0xffffffff;

			verts[3].x=SKYBODY_SIZE*V3.X;
			verts[3].y=SKYBODY_SIZE*V3.Y;
			verts[3].z=SKYBODY_SIZE*V3.Z;
			verts[3].u2=0;
			verts[3].v2=0;
			verts[3].diffuse=0xffffffff;
		}
	}

	g_renderBackend->Set_Index_Buffer(m_indexBuffer,0);
	g_renderBackend->Set_Vertex_Buffer(vb_access);

	Matrix3D tm(1);
	//set position of skybody in world
//	tm.Set_Translation(Vector3(40,0,0));
	g_renderBackend->Set_Transform(RB_TRANSFORM_WORLD,tm);

	g_renderBackend->Draw_Triangles(	0,2, 0,	4);	//draw a quad, 2 triangles, 4 verts
}
#endif

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void WaterRenderObjClass::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void WaterRenderObjClass::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// grid cells x
	Int cellsX = m_gridCellsX;
	xfer->xferInt( &cellsX );
	if( cellsX != m_gridCellsX )
	{

		DEBUG_CRASH(( "WaterRenderObjClass::xfer - cells X mismatch" ));
		throw SC_INVALID_DATA;

	}

	// grid cells Y
	Int cellsY = m_gridCellsY;
	xfer->xferInt( &cellsY );
	if( cellsY != m_gridCellsY )
	{

		DEBUG_CRASH(( "WaterRenderObjClass::xfer - cells Y mismatch" ));
		throw SC_INVALID_DATA;

	}

	// xfer each of the mesh data points
	for( UnsignedInt i = 0; i < m_meshDataSize; ++i )
	{

		// height
		xfer->xferReal( &m_meshData[ i ].height );

		// velocity
		xfer->xferReal( &m_meshData[ i ].velocity );

		// status
		xfer->xferUnsignedByte( &m_meshData[ i ].status );

		// preferred height
		xfer->xferUnsignedByte( &m_meshData[ i ].preferredHeight );

	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void WaterRenderObjClass::loadPostProcess()
{

}
