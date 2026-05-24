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

// W3DSmudge.cpp ////////////////////////////////////////////////////////////////////////////////
// Smudge System implementation
// Author: Mark Wilczynski, June 2003
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "always.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "Common/GlobalData.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "Common/GameMemory.h"
#include "GameClient/View.h"
#include "GameClient/Display.h"
#include "WW3D2/texture.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/RenderBufferTypes.h"
#include "WW3D2/RenderBackend.h"
#include "WW3D2/renderbufferclasses.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/sortingrenderer.h"
#include "WW3D2/surfaceclass.h"
#include "WW3D2/vertexbuffer.h"
#include "WW3D2/vertmaterial.h"
#include "WWMath/vector2i.h"


SmudgeManager *TheSmudgeManager=nullptr;

W3DSmudgeManager::W3DSmudgeManager()
{
}

W3DSmudgeManager::~W3DSmudgeManager()
{
	ReleaseResources();
}

void W3DSmudgeManager::init()
{
	SmudgeManager::init();
	ReAcquireResources();
}

void W3DSmudgeManager::reset ()
{
	SmudgeManager::reset();	//base
}

void W3DSmudgeManager::ReleaseResources()
{
	REF_PTR_RELEASE(m_backgroundTexture);
	REF_PTR_RELEASE(m_indexBuffer);
}


#define SMUDGE_DRAW_SIZE	500	//draw at most 50 smudges per call. Tweak value to improve CPU/GPU parallelism.

static_assert(SMUDGE_DRAW_SIZE * 5 < 0x10000, "Vertex index exceeds 16-bit limit");


void W3DSmudgeManager::ReAcquireResources()
{
	ReleaseResources();

	RenderBackendSurfaceDescription surface_desc;

#if defined(GGC_BGFX_STANDALONE)
	surface_desc.Format = WW3D_FORMAT_UNKNOWN;
	surface_desc.Width = TheDisplay ? TheDisplay->getWidth() : 0;
	surface_desc.Height = TheDisplay ? TheDisplay->getHeight() : 0;
#else
	if (!g_renderBackend || !g_renderBackend->Get_Back_Buffer_Description(0, surface_desc))
		return;

	m_backgroundTexture = MSGNEW("TextureClass") TextureClass(surface_desc.Width,surface_desc.Height,surface_desc.Format,MIP_LEVELS_1,TextureClass::POOL_DEFAULT, true);
#endif

	m_backBufferWidth = surface_desc.Width;
	m_backBufferHeight = surface_desc.Height;

	m_indexBuffer=NEW_REF(RenderIndexBufferClass,(SMUDGE_DRAW_SIZE*4*3));	//allocate 4 triangles per smudge, each with 3 indices.

	// Fill up the IB with static vertex indices that will be used for all smudges.
	{
		RenderIndexBufferClass::WriteLockClass lockIdxBuffer(m_indexBuffer);
		UnsignedShort *ib=lockIdxBuffer.Get_Index_Array();
		//quad of 4 triangles:
		//	0-----3
		//  |\   /|
		//  |  4  |
		//	|/   \|
		//  1-----2
		Int vbCount=0;
		for (Int i=0; i<SMUDGE_DRAW_SIZE; i++)
		{
			//Top
			ib[0]=vbCount;
			ib[1]=vbCount+4;
			ib[2]=vbCount+3;
			//Right
			ib[3]=vbCount+3;
			ib[4]=vbCount+4;
			ib[5]=vbCount+2;
			//Bottom
			ib[6]=vbCount+2;
			ib[7]=vbCount+4;
			ib[8]=vbCount+1;
			//Left
			ib[9]=vbCount+1;
			ib[10]=vbCount+4;
			ib[11]=vbCount+0;

			vbCount += 5;
			ib+=12;
		}
	}
}

/*Copies a portion of the current render target into a specified buffer*/
Int copyRect(unsigned char *buf, Int bufSize, int oX, int oY, int width, int height)
{
	if (buf == nullptr || bufSize <= 0 || width <= 0 || height <= 0 || g_renderBackend == nullptr) {
		return 0;
	}

	RenderBackendImage image;
	if (!g_renderBackend->Capture_Back_Buffer_Image(0, image)) {
		return 0;
	}

	if (oX < 0 || oY < 0 ||
		oX + width > static_cast<int>(image.Width) ||
		oY + height > static_cast<int>(image.Height)) {
		return 0;
	}

	const int bytesPerPixel = Get_Bytes_Per_Pixel(image.Format);
	const int rowBytes = width * bytesPerPixel;
	const int totalBytes = rowBytes * height;
	const int copyBytes = (bufSize < totalBytes) ? bufSize : totalBytes;
	if (bytesPerPixel <= 0 || copyBytes <= 0) {
		return 0;
	}

	int copied = 0;
	for (int row = 0; row < height && copied < copyBytes; ++row) {
		const int rowCopy = (copyBytes - copied < rowBytes) ? copyBytes - copied : rowBytes;
		const size_t sourceOffset =
			(static_cast<size_t>(oY + row) * image.Pitch) + (static_cast<size_t>(oX) * bytesPerPixel);
		memcpy(buf + copied, image.Bytes.data() + sourceOffset, rowCopy);
		copied += rowCopy;
	}

	return copied;
}

#define UNIQUE_COLOR	(0x12345678)
#define BLOCK_SIZE	(8)

Bool W3DSmudgeManager::testHardwareSupport()
{
#if defined(GGC_BGFX_STANDALONE)
	// TheSuperHackers @feature bobtista 27/04/2026 Standalone bgfx samples
	// the scene-color framebuffer directly for smudge/heat-haze distortion,
	// so it no longer needs the old DX8 CopyRects support test.
	m_hardwareSupportStatus = SMUDGE_SUPPORT_YES;
	return TRUE;
#else
	if (m_hardwareSupportStatus == SMUDGE_SUPPORT_UNKNOWN)
	{	//we have not done the test yet.

		if (!W3DShaderManager::hasRenderTexture() || !W3DShaderManager::isRenderingToTexture())
		{
			// TheSuperHackers @bugfix When Render-To-Texture is disabled globally, we fallback
			// to copying the backbuffer to a texture.
			if (m_backgroundTexture)
			{
				m_hardwareSupportStatus = SMUDGE_SUPPORT_YES;
				return TRUE;
			}

			m_hardwareSupportStatus = SMUDGE_SUPPORT_NO;
			return FALSE;
		}

		VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
		g_renderBackend->Set_Material(vmat);
		REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.

		ShaderClass shader=ShaderClass::_PresetOpaqueShader;
		shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
		shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
		g_renderBackend->Set_Shader(shader);
		g_renderBackend->Set_Texture(0,nullptr);
		g_renderBackend->Apply_Render_State_Changes();	//force update of view and projection matrices

		RenderBackendScreenVertex v[4];

		//bottom right
		v[0].x = BLOCK_SIZE-0.5f;
		v[0].y = BLOCK_SIZE-0.5f;
		v[0].z = 0.0f;
		v[0].w = 1.0f;
		v[0].u0 = BLOCK_SIZE/(Real)TheDisplay->getWidth();
		v[0].v0 = BLOCK_SIZE/(Real)TheDisplay->getHeight();
		//top right
		v[1].x = BLOCK_SIZE-0.5f;
		v[1].y = 0-0.5f;
		v[1].z = 0.0f;
		v[1].w = 1.0f;
		v[1].u0 = BLOCK_SIZE/(Real)TheDisplay->getWidth();
		v[1].v0 = 0;
		//bottom left
		v[2].x = 0-0.5f;
		v[2].y = BLOCK_SIZE-0.5f;
		v[2].z = 0.0f;
		v[2].w = 1.0f;
		v[2].u0 = 0;
		v[2].v0 = BLOCK_SIZE/(Real)TheDisplay->getHeight();
		//top left
		v[3].x = 0-0.5f;
		v[3].y = 0-0.5f;
		v[3].z = 0.0f;
		v[3].w = 1.0f;
		v[3].u0 = 0;
		v[3].v0 = 0;

		for (Int i = 0; i < 4; ++i)
		{
			v[i].diffuse = UNIQUE_COLOR;
			v[i].u1 = 0.0f;
			v[i].v1 = 0.0f;
		}

		if (g_renderBackend == nullptr || !g_renderBackend->Draw_Screen_Quad(v, 4, false))
		{
			m_hardwareSupportStatus = SMUDGE_SUPPORT_NO;
			return FALSE;
		}

		DWORD refData[BLOCK_SIZE*BLOCK_SIZE];
		memset(refData,0,sizeof(refData));
		Int bufSize=copyRect((unsigned char *)refData,sizeof(refData),0,0,BLOCK_SIZE,BLOCK_SIZE);	//copy area we just rendered using solid color
		if (!bufSize)
		{
			m_hardwareSupportStatus = SMUDGE_SUPPORT_NO;
			return FALSE;
		}

		if (g_renderBackend == nullptr ||
			!g_renderBackend->Bind_View_Capture_Texture(RB_VIEW_CAPTURE_TACTICAL, 0))
		{
			m_hardwareSupportStatus = SMUDGE_SUPPORT_NO;
			return FALSE;
		}

		DWORD testData[BLOCK_SIZE*BLOCK_SIZE];
		memset(testData,0xff,sizeof(testData));

		v[0].diffuse = 0xffffffff;
		v[1].diffuse = 0xffffffff;
		v[2].diffuse = 0xffffffff;
		v[3].diffuse = 0xffffffff;

		if (!g_renderBackend->Draw_Screen_Quad(v, 4, false))
		{
			m_hardwareSupportStatus = SMUDGE_SUPPORT_NO;
			return FALSE;
		}
		bufSize=copyRect((unsigned char *)testData,sizeof(testData),0,0,BLOCK_SIZE,BLOCK_SIZE);

		if (!bufSize)
		{
			m_hardwareSupportStatus = SMUDGE_SUPPORT_NO;
			return FALSE;
		}

		//compare the 2 buffers to see if they match.
		if (memcmp(testData,refData,bufSize) == 0)
		{
			m_hardwareSupportStatus = SMUDGE_SUPPORT_YES;
			return TRUE;
		}
		m_hardwareSupportStatus = SMUDGE_SUPPORT_NO;
	}

	return (SMUDGE_SUPPORT_YES == m_hardwareSupportStatus);
#endif // GGC_BGFX_STANDALONE
}

void W3DSmudgeManager::render(RenderInfoClass &rinfo)
{
	//Verify that the card supports the effect.
	if (!testHardwareSupport())
		return;

	RenderBackendSurfaceDescription surface_desc;
	Bool bgfxSmudgeActive = FALSE;

#if defined(GGC_BGFX_STANDALONE)
	surface_desc.Format = WW3D_FORMAT_UNKNOWN;
	surface_desc.Width = TheDisplay->getWidth();
	surface_desc.Height = TheDisplay->getHeight();
#else
	if (!g_renderBackend || !g_renderBackend->Get_Back_Buffer_Description(0, surface_desc))
		return;
#endif

	CameraClass &camera=rinfo.Camera;
	Vector3 vsVert;
	Vector4 ssVert;
	Real uvSpanX,uvSpanY;
	Vector3 vertex_offsets[4] = {
		Vector3(-0.5f, 0.5f, 0.0f),
		Vector3(-0.5f, -0.5f, 0.0f),
		Vector3(0.5f, -0.5f, 0.0f),
		Vector3(0.5f, 0.5f, 0.0f)
	};

#if defined(GGC_BGFX_STANDALONE)
#define THE_COLOR (0x00ffffff)
#else
#define THE_COLOR (0x00ffeedd)
#endif

	UnsignedInt vertexDiffuse[5]={THE_COLOR,THE_COLOR,THE_COLOR,THE_COLOR,THE_COLOR};

	Matrix4x4 proj;
	Matrix3D view;

	camera.Get_View_Matrix(&view);
	camera.Get_Projection_Matrix(&proj);

	Real texClampX = (Real)TheTacticalView->getWidth()/(Real)surface_desc.Width;
	Real texClampY = (Real)TheTacticalView->getHeight()/(Real)surface_desc.Height;

	Real texScaleX = texClampX*0.5f;
	Real texScaleY = texClampY*0.5f;

	//Do a first pass over the smudges to determine how many are visible
	//and to fill in their world-space positions and screen uv coordinates.
	//TODO: Optimize out this extra pass!
	//TODO: Find size of screen rectangle that actually needs copying.

	SmudgeSet *set=m_usedSmudgeSetList.Head();	//first set that didn't fit into render batch.
	Int count = 0;

	if (set)
	{
		//there are possibly some smudges to render, so make sure background particles have finished drawing.
		SortingRendererClass::Flush();	//draw sorted translucent polys like particles.
	}

	while (set)
	{
		Smudge *smudge=set->getUsedSmudgeList().Head();

		for (; smudge; smudge = smudge->Succ())
		{
			if (!smudge->m_draw)
				continue;

			//Get view-space center
			Matrix3D::Transform_Vector(view,smudge->m_pos,&vsVert);

			//Get 5 view-space vertices
			Smudge::smudgeVertex *verts=smudge->m_verts;

			//Do center vertex outside 'for' loop since it's different.
			verts[4].pos = vsVert;

			Vector2 offset = smudge->m_offset;

			for (Int i=0; i<4; i++)
			{
				verts[i].pos = vsVert + vertex_offsets[i] * smudge->m_size;
				//Ge uv coordinates for each vertex
				ssVert = proj * verts[i].pos;
				Real oow = 1.0f/ssVert.W;
				ssVert *= oow;	//returned in camera space which is -1,-1 (bottom-left) to 1,1 (top-right)
				//convert camera space to uv space: 0,0 (top-left), 1,1 (bottom-right)
				verts[i].uv.Set((ssVert.X+1.0f)*texScaleX,(1.0f-ssVert.Y)*texScaleY);

				Vector2 &thisUV=verts[i].uv;

				// Zero coordinates that fall outside valid texel bounds
				if (thisUV.X < 0 || thisUV.X > texClampX)
					offset.X = 0;

				if (thisUV.Y < 0 || thisUV.Y > texClampY)
					offset.Y = 0;
			}

			//Finish center vertex
			//Ge uv coordinates by interpolating corner uv coordinates and applying desired offset.
			uvSpanX=verts[3].uv.X - verts[0].uv.X;
			uvSpanY=verts[1].uv.Y - verts[0].uv.Y;
			verts[4].uv.X=verts[0].uv.X+uvSpanX*(0.5f+offset.X);
			verts[4].uv.Y=verts[0].uv.Y+uvSpanY*(0.5f+offset.Y);

			count++;	//increment visible smudge count.
		}

		set=set->Succ();	//advance to next node.
	}

	m_smudgeCountLastFrame = count;

	if (!count)
	{
		return;	//nothing to render.
	}

#if !defined(GGC_BGFX_STANDALONE)
	//Copy the area of backbuffer occupied by smudges into an alternate buffer.
	RenderBackendImage back_buffer_image;
	if (m_backgroundTexture == nullptr ||
		g_renderBackend == nullptr ||
		!g_renderBackend->Capture_Back_Buffer_Image(0, back_buffer_image))
	{
		return;
	}

	SurfaceClass::SurfaceImageData image;
	image.Format = back_buffer_image.Format;
	image.Width = back_buffer_image.Width;
	image.Height = back_buffer_image.Height;
	image.Pitch = back_buffer_image.Pitch;
	image.Data = back_buffer_image.Bytes;
	m_backgroundTexture->Update_Surface_Level_From_Surface(0, image);
#else
	if (!g_renderBackend || !g_renderBackend->Begin_Smudge_Distortion())
		return;

	bgfxSmudgeActive = TRUE;
#endif

	Matrix4x4 identity(true);
	g_renderBackend->Set_Transform(RB_TRANSFORM_WORLD,identity);
	g_renderBackend->Set_Transform(RB_TRANSFORM_VIEW,identity);
	g_renderBackend->Set_Transform(RB_TRANSFORM_PROJECTION,proj);

	g_renderBackend->Set_Index_Buffer(m_indexBuffer,0);
	//g_renderBackend->Set_Shader(ShaderClass::_PresetOpaqueSpriteShader);

	g_renderBackend->Set_Shader(ShaderClass::_PresetAlphaShader);

	g_renderBackend->Set_Texture(0,bgfxSmudgeActive ? nullptr : m_backgroundTexture);
	//Need these states in case texture is non-power-of-2
	g_renderBackend->Set_Texture_Address_Mode(0, RB_TEXTURE_ADDRESS_CLAMP, RB_TEXTURE_ADDRESS_CLAMP, RB_TEXTURE_ADDRESS_CLAMP);
	g_renderBackend->Set_Texture_Sample_Filter(0, RB_TEXTURE_SAMPLE_LINEAR, RB_TEXTURE_SAMPLE_LINEAR, RB_TEXTURE_SAMPLE_NONE);
	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	g_renderBackend->Set_Material(vmat);
	REF_PTR_RELEASE(vmat);
	g_renderBackend->Apply_Render_State_Changes();

		// Disable reading texture alpha since it's undefined.
	g_renderBackend->Set_Texture_Alpha_Operation(0, RB_TEXOP_SELECTARG2);

	Int smudgesRemaining=count;
	set=m_usedSmudgeSetList.Head();	//first smudge set that needs rendering.
	Smudge	*remainingSmudgeStart=set->getUsedSmudgeList().Head();	//first smudge that needs rendering.

	while (smudgesRemaining)	//keep drawing smudges until we run out.
	{
		//Now that we know how many smudges need rendering, allocate vertex buffer space and copy verts.
		count=smudgesRemaining;

		if (count > SMUDGE_DRAW_SIZE)
			count = SMUDGE_DRAW_SIZE;

		Int smudgesInRenderBatch=0;

		DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC,dynamic_fvf_type,count*5);	//allocate 5 verts per smudge.
		{
			DynamicVBAccessClass::WriteLockClass lock(&vb_access);
			VertexFormatXYZNDUV2* verts=lock.Get_Formatted_Vertex_Array();

			while (set)
			{
				Smudge *smudge=remainingSmudgeStart;

				for (; smudge; smudge=smudge->Succ())
				{
					if (!smudge->m_draw)
						continue;

					Smudge::smudgeVertex *smVerts = smudge->m_verts;

					//Check if we exceeded maximum number of smudges allowed per draw call.
					if (smudgesInRenderBatch >= count)
					{
						remainingSmudgeStart = smudge;
						goto flushSmudges;
					}

					//Set center vertex opacity.
					Real opacity = smudge->m_opacity;
#if defined(GGC_BGFX_STANDALONE)
#if defined(RTS_ZEROHOUR)
					if (TheGlobalData)
					{
						opacity *= TheGlobalData->m_bgfxHeatHazeOpacityScale;
						if (opacity < 0.0f)
						{
							opacity = 0.0f;
						}
						else if (opacity > 1.0f)
						{
							opacity = 1.0f;
						}
					}
#endif
#endif
					vertexDiffuse[4] = ((Int)(opacity * 255.0f) << 24) | THE_COLOR;

					for (Int i=0; i<5; i++)
					{
						verts->x=smVerts->pos.X;
						verts->y=smVerts->pos.Y;
						verts->z=smVerts->pos.Z;
						verts->nx=0;	//keep AGP write-combining active
						verts->ny=0;
						verts->nz=0;
						verts->diffuse=vertexDiffuse[i];	//set to transparent
						verts->u1=smVerts->uv.X;
						verts->v1=smVerts->uv.Y;
						verts->u2=0;	//keep AGP write-combining active
						verts->v2=0;
						verts++;
						smVerts++;
					}

					smudgesInRenderBatch++;
				}

				set=set->Succ();	//advance to next node.

				if (set)	//start next batch at beginning of set.
					remainingSmudgeStart = set->getUsedSmudgeList().Head();
			}
		}

flushSmudges:
		g_renderBackend->Set_Vertex_Buffer(vb_access);

		g_renderBackend->Draw_Triangles(0,smudgesInRenderBatch*4, 0, smudgesInRenderBatch*5);

		smudgesRemaining -= smudgesInRenderBatch;
	}

	g_renderBackend->Set_Texture_Color_Operation(0, RB_TEXOP_MODULATE);
	g_renderBackend->Set_Texture_Alpha_Operation(0, RB_TEXOP_MODULATE);

	if (bgfxSmudgeActive)
		g_renderBackend->End_Smudge_Distortion();
}
