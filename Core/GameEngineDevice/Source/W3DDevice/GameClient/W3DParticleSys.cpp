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

// W3DParticleSys.cpp
// W3D Particle System implementation
// Author: Michael S. Booth, November 2001

#include "Common/GlobalData.h"
#include "GameClient/Color.h"
#include "GameLogic/TerrainLogic.h"
#include "W3DDevice/GameClient/W3DParticleSys.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "W3DDevice/GameClient/W3DSnow.h"
#include "WW3D2/BgfxRenderProfile.h"
#include "W3DDevice/GameClient/W3DWater.h"
#include "WW3D2/camera.h"
#include "WW3D2/RenderBackend.h"
#include "GgcRuntimeFlags.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// TheSuperHackers @tweak bobtista 05/06/2026 Empirically-tuned boosts that make
// ground-aligned additive foam read at retail brightness on the bgfx shader pipeline.
static const float BGFX_ADDITIVE_FOAM_SIZE_BOOST = 2.0f;
static const float BGFX_ADDITIVE_FOAM_COLOR_BOOST = 1.5f;

static bool containsCaseInsensitive(const char *text, const char *needle)
{
	if (text == nullptr || needle == nullptr || *needle == '\0')
	{
		return false;
	}
	for (const char *cursor = text; *cursor != '\0'; ++cursor)
	{
		const char *a = cursor;
		const char *b = needle;
		while (*a != '\0' && *b != '\0')
		{
			const char ca = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a - 'A' + 'a') : *a;
			const char cb = (*b >= 'A' && *b <= 'Z') ? static_cast<char>(*b - 'A' + 'a') : *b;
			if (ca != cb)
			{
				break;
			}
			++a;
			++b;
		}
		if (*b == '\0')
		{
			return true;
		}
	}
	return false;
}

static bool shouldUseEmpPointGroupDepthOverride(TextureClass *texture)
{
	const char *name = texture != nullptr ? texture->Get_Full_Path().str() : nullptr;
	return containsCaseInsensitive(name, "exempring")
		|| containsCaseInsensitive(name, "exlnzflar");
}

static ShaderClass getParticlePointGroupShader(ParticleSystemInfo::ParticleShaderType shaderType, Bool billboard, TextureClass *texture)
{
	ShaderClass shader = ShaderClass::_PresetAlphaSpriteShader;
	switch( shaderType )
	{
		case ParticleSystemInfo::ADDITIVE:
			shader = ShaderClass::_PresetAdditiveSpriteShader;
			break;
		case ParticleSystemInfo::ALPHA:
			shader = ShaderClass::_PresetAlphaSpriteShader;
			break;
		case ParticleSystemInfo::ALPHA_TEST:
			shader = ShaderClass::_PresetATestSpriteShader;
			break;
		case ParticleSystemInfo::MULTIPLY:
			shader = ShaderClass::_PresetMultiplicativeSpriteShader;
			break;
	}

	if (g_renderBackend != nullptr
		&& g_renderBackend->Has_Shader_Pipeline()
		&& shaderType == ParticleSystemInfo::ADDITIVE
		&& !billboard)
	{
		if (shouldUseEmpPointGroupDepthOverride(texture))
		{
			shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
		}
	}

	return shader;
}

//------------------------------------------------------------------------------ Performance Timers
//#include "Common/PerfMetrics.h"
//#include "Common/PerfTimer.h"

//-------------------------------------------------------------------------------------------------


W3DParticleSystemManager::W3DParticleSystemManager()
{
	m_pointGroup = nullptr;
	m_streakLine = nullptr;
	m_posBuffer = nullptr;
	m_RGBABuffer = nullptr;
	m_sizeBuffer = nullptr;
	m_angleBuffer = nullptr;
	m_readyToRender = false;

	m_onScreenParticleCount = 0;

	m_pointGroup = NEW PointGroupClass();
	//m_streakLine = nullptr;
	m_streakLine = NEW StreakLineClass();

	m_posBuffer = NEW_REF( ShareBufferClass<Vector3>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_posBuffer") );
	m_RGBABuffer = NEW_REF( ShareBufferClass<Vector4>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_RGBABuffer") );
	m_sizeBuffer = NEW_REF( ShareBufferClass<float>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_sizeBuffer") );
	m_angleBuffer = NEW_REF( ShareBufferClass<uint8>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_angleBuffer") );
}

W3DParticleSystemManager::~W3DParticleSystemManager()
{
	delete m_pointGroup;

//	W3DDisplay::m_3DScene->Remove_Render_Object( m_streakLine );

	if (m_streakLine)
	{
		REF_PTR_RELEASE(m_streakLine);
	}

	REF_PTR_RELEASE(m_posBuffer);
	REF_PTR_RELEASE(m_RGBABuffer);
	REF_PTR_RELEASE(m_sizeBuffer);
	REF_PTR_RELEASE(m_angleBuffer);
}

/**
 * Hack because DoParticles is called from Flush(), which is called
 * multiple times per frame.  We only want to render once.
 * @todo Clean up the flag/Flush hack.
 */
void W3DParticleSystemManager::queueParticleRender()
{
	m_readyToRender = true;
}

/**
 * Nasty hack to render particles last. Called directly by WW3D::Flush()
 */
void DoParticles( RenderInfoClass &rinfo )
{
	if (TheParticleSystemManager)
		TheParticleSystemManager->doParticles(rinfo);
}

// TheSuperHackers @perf bobtista 03/06/2026 Render the accumulated [0,count) range of the shared
// scratch buffers as a single point-group draw. The buffers already hold world-space vertices from
// one or more emitters that share the bucket key, so no per-emitter transform is needed.
void W3DParticleSystemManager::flushPointGroupBatch(RenderInfoClass &rinfo, TextureClass *texture,
	ParticleSystemInfo::ParticleShaderType shaderType, Bool billboard, UnsignedInt volumeDepth, Int count)
{
	if (count == 0 || texture == nullptr || m_pointGroup == nullptr)
	{
		return;
	}

	m_pointGroup->Set_Texture( texture );
	m_pointGroup->Set_Flag( PointGroupClass::TRANSFORM, true );	// transform to screen space
	m_pointGroup->Set_Shader( getParticlePointGroupShader(shaderType, billboard, texture) );

	m_pointGroup->Set_Point_Mode( PointGroupClass::QUADS );
	m_pointGroup->Set_Arrays( m_posBuffer, m_RGBABuffer, nullptr, m_sizeBuffer, m_angleBuffer, nullptr, count );
	m_pointGroup->Set_Billboard( billboard );
	m_pointGroup->Set_Point_Frame( 0 );

	if( volumeDepth > 1 )
	{
		m_pointGroup->RenderVolumeParticle( rinfo, volumeDepth );
	}
	else
		m_pointGroup->Render( rinfo );
}

void W3DParticleSystemManager::doParticles(RenderInfoClass &rinfo)
{
	const bool particleDiag = GgcFlags::Enabled(GgcFlag_ParticleDiag);

	if (m_readyToRender == false)
	{
		return;
	}

	// external mechanism must tell us when it's OK to render again...
	m_readyToRender = false;

	//reset each frame
	/// @todo lorenzen sez: this should be debug only:
	m_onScreenParticleCount = 0;

 	const FrustumClass & frustum = rinfo.Camera.Get_Frustum();
	AABoxClass bbox;

	//Get a bounding box around our visible universe.  Bounded by terrain and the sky
	//so much tighter fitting volume than what's actually visible.  This will cull
	//particles falling under the ground.

 	TheTerrainRenderObject->getMaximumVisibleBox(frustum, &bbox, TRUE);

	//@todo lorenzen sez: put these in registers for sure
	Real bcX = bbox.Center.X;
	Real bcY = bbox.Center.Y;
	Real bcZ = bbox.Center.Z;
	Real beX = bbox.Extent.X;
	Real beY = bbox.Extent.Y;
	Real beZ = bbox.Extent.Z;

	unsigned int personalities[MAX_POINTS_PER_GROUP];


	m_fieldParticleCount = 0;

	const Bool drawSmudge = TheSmudgeManager && TheSmudgeManager->getHardwareSupport() && TheGlobalData->m_useHeatEffects;

	if (drawSmudge)
	{
		TheSmudgeManager->resetDraw();
	}

	// TheSuperHackers @perf bobtista 03/06/2026 Only the shader-pipeline backend batches emitter draws.
	// The fixed-function DX8 reference path keeps batchBase==0 so its buffer indexing, particle cap and
	// per-emitter submission stay bit-for-bit identical to the original code.
	// TheSuperHackers @perf bobtista 03/06/2026 GGC_NO_PARTICLE_BATCH=1 forces the
	// per-emitter path for A/B visual verification and as a runtime safety escape.
	static const bool s_particleBatchDisabled = GgcFlags::Enabled(GgcFlag_NoParticleBatch);
	const bool batchPointGroups = (!s_particleBatchDisabled && g_renderBackend != nullptr && g_renderBackend->Has_Shader_Pipeline());
	Int batchBase = 0;
	TextureClass *batchTexture = nullptr;
	ParticleSystemInfo::ParticleShaderType batchShader = ParticleSystemInfo::INVALID_SHADER;
	Bool batchBillboard = FALSE;
	UnsignedInt batchVolumeDepth = 0;

	ParticleSystemManager::ParticleSystemList &particleSysList = TheParticleSystemManager->getAllParticleSystems();
	for( ParticleSystemManager::ParticleSystemListIt it = particleSysList.begin(); it != particleSysList.end(); ++it)
	{
		ParticleSystem *sys = (*it);
		if (!sys) {
			continue;
		}

		// only look at particle/point style systems
		if (sys->isUsingDrawables())
		{
			continue;
		}

		//temporary hack that checks if texture name starts with "SMUD" - if so, we can assume it's a smudge type
		if (/*sys->isUsingSmudge()*/ *((DWORD *)sys->getParticleTypeName().str()) == 0x44554D53)
		{
			if (drawSmudge)
			{
				for (Particle *p = sys->getFirstParticle(); p; p = p->m_systemNext)
				{
					const Coord3D *pos = p->getPosition();
					Real psize = p->getSize();

					//Cull particle to edges of screen and terrain.
					if (WWMath::Fabsf_Legacy( pos->x - bcX ) > ( beX + psize ) )
						continue;

					if (WWMath::Fabsf_Legacy( pos->y - bcY ) > ( beY + psize ) )
						continue;

					if (WWMath::Fabsf_Legacy( pos->z - bcZ ) > ( beZ + psize ) )
						continue;

					if (Smudge *smudge = TheSmudgeManager->findSmudge(p))
					{
						// The particle is in view. Draw the smudge!
						smudge->m_draw = true;
					}
				}
			}
			continue;
		}

		/// @todo lorenzen sez: declare these outside the sys loop, and put some in registers
		// initialize them here still, of course
		// build W3D particle buffer
		Int count = 0;
		TextureClass *texture = nullptr;

		// TheSuperHackers @perf bobtista 03/06/2026 In batch mode resolve this emitter's bucket key up
		// front so we can flush the open batch (and reset the accumulation base) before appending into
		// the shared scratch buffers. Streak emitters never batch; flush before handing them off below.
		if (batchPointGroups)
		{
			{
				GGC_RPROFILE(PARTICLE_TEX_FETCH);
				texture = W3DDisplay::m_assetManager->Get_Texture( sys->getParticleTypeName().str() );
			}
			const Bool isStreak = (m_streakLine != nullptr && sys->isUsingStreak());
			// TheSuperHackers @bugfix bobtista 03/06/2026 Volume particles must NOT batch:
			// RenderVolumeParticle derives its per-layer shift from current_size[0] (the
			// first particle's size, pointgr.cpp:1880), so merging multiple volume emitters
			// applies the first emitter's size to every layer of every merged particle,
			// smearing volumetric blobs across the screen (over-bright clouds). Treat them
			// like streaks: flush the open batch and render this emitter per-emitter.
			const Bool isVolume = (sys->getVolumeParticleDepth() > 1);
			const Bool keyMatches = (batchTexture == texture
				&& batchShader == sys->getShaderType()
				&& batchBillboard == sys->shouldBillboard()
				&& batchVolumeDepth == sys->getVolumeParticleDepth());
			if (batchBase > 0 && (isStreak || isVolume || !keyMatches))
			{
				flushPointGroupBatch( rinfo, batchTexture, batchShader, batchBillboard, batchVolumeDepth, batchBase );
				batchTexture->Release_Ref();
				batchTexture = nullptr;
				batchBase = 0;
			}
		}

		Vector3 *posArray = m_posBuffer->Get_Array() + batchBase;
		Real *sizeArray = m_sizeBuffer->Get_Array() + batchBase;
		Vector4 *RGBAArray = m_RGBABuffer->Get_Array() + batchBase;
		uint8 *angleArray = m_angleBuffer->Get_Array() + batchBase;
		const Coord3D *pos;
		const RGBColor *color;
		Real psize;



		//set-up all the per-particle
		for (Particle *p = sys->getFirstParticle(); p; p = p->m_systemNext)
		{
			pos = p->getPosition();
			psize = p->getSize();

			//Cull particle to edges of screen and terrain.
			if (WWMath::Fabsf_Legacy(pos->x - bcX) > (beX + psize))
				continue;

			if (WWMath::Fabsf_Legacy(pos->y - bcY) > (beY + psize))
				continue;

			if (WWMath::Fabsf_Legacy(pos->z - bcZ) > (beZ + psize))
				continue;

			m_fieldParticleCount += ( sys->getPriority() == AREA_EFFECT && sys->m_isGroundAligned != FALSE );

			//@todo lorenzen sez: use pointer arithmetic for these arrays
			personalities[count] = p->getPersonality();

			posArray[count].X = pos->x;
			posArray[count].Y = pos->y;
			posArray[count].Z = pos->z;

			sizeArray[count] = psize;

			color = p->getColor();
			RGBAArray[count].X = color->red;
			RGBAArray[count].Y = color->green;
			RGBAArray[count].Z = color->blue;

			// TheSuperHackers @bugfix bobtista 28/05/2026 Ground-aligned ADDITIVE water-surface foam
			// renders dimmer through the shader pipeline than DX8; boost size 2x and color 1.5x
			// (clamped), gated on batchPointGroups so the DX8 path is unaffected.
			if (batchPointGroups
				&& sys->m_isGroundAligned
				&& sys->getShaderType() == ParticleSystemInfo::ADDITIVE)
			{
				sizeArray[count] *= BGFX_ADDITIVE_FOAM_SIZE_BOOST;
				RGBAArray[count].X = MIN(1.0f, color->red   * BGFX_ADDITIVE_FOAM_COLOR_BOOST);
				RGBAArray[count].Y = MIN(1.0f, color->green * BGFX_ADDITIVE_FOAM_COLOR_BOOST);
				RGBAArray[count].Z = MIN(1.0f, color->blue  * BGFX_ADDITIVE_FOAM_COLOR_BOOST);
			}

			// TheSuperHackers @bugfix bobtista 27/05/2026 ADDITIVE particles keep m_alpha at the
			// initial keyframe (often 0) and fs_uber's alpha-aware paths would discard them; force
			// vertex alpha to 1.0, gated on batchPointGroups so DX8 keeps per-particle alpha.
			if (batchPointGroups && sys->getShaderType() == ParticleSystemInfo::ADDITIVE)
			{
				RGBAArray[count].W = 1.0f;
			}
			else
			{
				RGBAArray[count].W = p->getAlpha();
			}

			angleArray[count] = (uint8)(p->getAngle() * 255.0f / (2.0f * PI));

			if (batchBase + (++count) == MAX_POINTS_PER_GROUP)
			{
				// TheSuperHackers @perf bobtista 03/06/2026 The shared buffer filled while a same-key
				// batch was already accumulated in front of this emitter. Flush that prior batch, then
				// shift this emitter's particles to the front and keep filling from offset 0 so no
				// particle is dropped (the DX8 path keeps batchBase==0 and simply breaks as before).
				if (batchPointGroups && batchBase > 0)
				{
					flushPointGroupBatch( rinfo, batchTexture, batchShader, batchBillboard, batchVolumeDepth, batchBase );
					batchTexture->Release_Ref();
					batchTexture = nullptr;
					std::memmove( m_posBuffer->Get_Array(), posArray, count * sizeof(Vector3) );
					std::memmove( m_sizeBuffer->Get_Array(), sizeArray, count * sizeof(Real) );
					std::memmove( m_RGBABuffer->Get_Array(), RGBAArray, count * sizeof(Vector4) );
					std::memmove( m_angleBuffer->Get_Array(), angleArray, count * sizeof(uint8) );
					batchBase = 0;
					posArray = m_posBuffer->Get_Array();
					sizeArray = m_sizeBuffer->Get_Array();
					RGBAArray = m_RGBABuffer->Get_Array();
					angleArray = m_angleBuffer->Get_Array();
					continue;
				}
				break;
			}
		}

			if ( count == 0 )
			{
				// TheSuperHackers @perf bobtista 03/06/2026 In batch mode the bucket texture was acquired
				// up front; release that reference when this emitter contributes nothing.
				if (batchPointGroups && texture != nullptr)
				{
					texture->Release_Ref();
				}
				continue;	//this system has no particles to render
			}

			if (!batchPointGroups)
			{
				GGC_RPROFILE(PARTICLE_TEX_FETCH);
				texture = W3DDisplay::m_assetManager->Get_Texture( sys->getParticleTypeName().str() );
			}
			if (particleDiag)
			{
				if (FILE *diag = std::fopen("ggc_particle_diag.txt", "a"))
				{
					const Coord3D *firstPos = sys->getFirstParticle() != nullptr
						? sys->getFirstParticle()->getPosition()
						: nullptr;
					float minSz = (count > 0) ? sizeArray[0] : 0.0f;
					float maxSz = minSz;
					float minA = (count > 0) ? RGBAArray[0].W : 0.0f;
					float maxA = minA;
					for (Int idx = 1; idx < count; ++idx) {
						if (sizeArray[idx] < minSz) {
							minSz = sizeArray[idx];
						}
						if (sizeArray[idx] > maxSz) {
							maxSz = sizeArray[idx];
						}
						if (RGBAArray[idx].W < minA) {
							minA = RGBAArray[idx].W;
						}
						if (RGBAArray[idx].W > maxA) {
							maxA = RGBAArray[idx].W;
						}
					}
					Real waterZ = 0.0f;
					Real terrainZ = 0.0f;
					Bool underwater = FALSE;
					if (TheWaterRenderObj != nullptr && firstPos != nullptr) {
						waterZ = TheWaterRenderObj->getWaterHeight(firstPos->x, firstPos->y);
					}
					if (TheTerrainLogic != nullptr && firstPos != nullptr) {
						Real tw = 0.0f, tt = 0.0f;
						underwater = TheTerrainLogic->isUnderwater(firstPos->x, firstPos->y, &tw, &tt);
						terrainZ = tt;
					}
					std::fprintf(diag,
						"particle frame=%u type=%s texture=%s count=%d shader=%d streak=%d volume=%u billboard=%d ground=%d first=(%.2f,%.2f,%.2f) waterZ=%.2f terrainZ=%.2f under=%d sizeRange=[%.2f..%.2f] alphaRange=[%.3f..%.3f] firstRGB=(%.2f,%.2f,%.2f) texMissing=%d\n",
						0u,
						sys->getParticleTypeName().str(),
						texture != nullptr ? texture->Get_Full_Path().str() : "<null>",
						count,
						static_cast<int>(sys->getShaderType()),
						sys->isUsingStreak() ? 1 : 0,
						static_cast<unsigned>(sys->getVolumeParticleDepth()),
						sys->shouldBillboard() ? 1 : 0,
						sys->m_isGroundAligned ? 1 : 0,
						firstPos != nullptr ? firstPos->x : 0.0f,
						firstPos != nullptr ? firstPos->y : 0.0f,
						firstPos != nullptr ? firstPos->z : 0.0f,
						waterZ,
						terrainZ,
						(int)(underwater ? 1 : 0),
						minSz,
						maxSz,
						minA,
						maxA,
						(count > 0) ? RGBAArray[0].X : 0.0f,
						(count > 0) ? RGBAArray[0].Y : 0.0f,
						(count > 0) ? RGBAArray[0].Z : 0.0f,
						texture != nullptr && texture->Is_Missing_Texture() ? 1 : 0);
					std::fclose(diag);
				}
			}

			if ( m_streakLine && sys->isUsingStreak() && (count >= 2) )
		{
			m_streakLine->Reset_Line();

			m_streakLine->Set_Texture( texture );
			texture->Release_Ref();//release reference since it's held by streakline
			switch( sys->getShaderType() )
			{
				case ParticleSystemInfo::ADDITIVE:
					m_streakLine->Set_Shader( ShaderClass::_PresetAdditiveSpriteShader );
					break;
				case ParticleSystemInfo::ALPHA:
					m_streakLine->Set_Shader( ShaderClass::_PresetAlphaSpriteShader );
					break;
				case ParticleSystemInfo::ALPHA_TEST:
					m_streakLine->Set_Shader( ShaderClass::_PresetATestSpriteShader );
					break;
				case ParticleSystemInfo::MULTIPLY:
					m_streakLine->Set_Shader( ShaderClass::_PresetMultiplicativeSpriteShader );
					break;
			}

			//UPDATE THE STREAK'S ARRAYS
			m_streakLine->Set_LocsWidthsColors(
				count,
				m_posBuffer->Get_Array(),
				m_sizeBuffer->Get_Array(),
				m_RGBABuffer->Get_Array(),
				&personalities[0]
				);

			//WWASSERT( m_streakLine->Get_Num_Points() == count );

			// This is the happy place for this!
			RGBAArray[0].X = 0;//eliminates the scissor edge on the trailing edge of the streak
			RGBAArray[0].Y = 0;
			RGBAArray[0].Z = 0;
			RGBAArray[0].W = 0;


			//RENDER STREAK!
			m_streakLine->Render( rinfo );

		}
		else if (batchPointGroups && sys->getVolumeParticleDepth() <= 1)
		{
			// TheSuperHackers @perf bobtista 03/06/2026 Accumulate this emitter into the open bucket
			// instead of issuing a draw. The actual submission happens in flushPointGroupBatch when the
			// next emitter changes the bucket key, the buffer overflows, or the loop ends.
			if (batchBase == 0)
			{
				batchTexture = texture;	// take ownership of the reference acquired for the bucket key
			}
			else
			{
				texture->Release_Ref();	// same bucket: drop the duplicate reference, batchTexture holds one
			}
			batchShader = sys->getShaderType();
			batchBillboard = sys->shouldBillboard();
			batchVolumeDepth = sys->getVolumeParticleDepth();
			batchBase += count;
		}
		else
		{

			WWASSERT( m_pointGroup );

			if ( m_pointGroup ) // this catches the particle and volumeparticle cases
			{
				// render all the systems' particles
				m_pointGroup->Set_Texture( texture );
				texture->Release_Ref();//release reference since it's held by pointGroup
				m_pointGroup->Set_Flag( PointGroupClass::TRANSFORM, true );	// transform to screen space

				m_pointGroup->Set_Shader( getParticlePointGroupShader(sys->getShaderType(), sys->shouldBillboard(), texture) );

				/// @todo Use both QUADS and TRIS for particles
				m_pointGroup->Set_Point_Mode( PointGroupClass::QUADS );
				m_pointGroup->Set_Arrays( m_posBuffer, m_RGBABuffer, nullptr, m_sizeBuffer, m_angleBuffer, nullptr, count );
				m_pointGroup->Set_Billboard(sys->shouldBillboard());

				/// @todo Support animated texture particles
				/// @todo lorenzen sez: unimplemented code wastes cpu cycles
				m_pointGroup->Set_Point_Frame( 0 );

				//RENDER IT!
				if( sys->getVolumeParticleDepth() > 1 )
				{
					m_pointGroup->RenderVolumeParticle( rinfo, sys->getVolumeParticleDepth() );
				}
				else
					m_pointGroup->Render( rinfo );

			}
		}


		/// @todo lorenzen sez: this should be debug only:
		//add particle count to total
		m_onScreenParticleCount += count;

	/*
		// draw the wind vector for this particle system on the screen
		UnsignedInt width = TheDisplay->getWidth();
		UnsignedInt height = TheDisplay->getHeight();
		Coord3D worldStart, worldEnd;
		ICoord2D pixelStart, pixelEnd;
		sys->getPosition( &worldStart );
		worldEnd.x = Cos( sys->getWindAngle() ) * 50.0f + worldStart.x;
		worldEnd.y = Sin( sys->getWindAngle() ) * 50.0f + worldStart.y;
		worldEnd.z = worldStart.z;
		TheTacticalView->worldToScreen( &worldStart, &pixelStart );
		TheTacticalView->worldToScreen( &worldEnd, &pixelEnd );
		Color colorStart = GameMakeColor( 255, 255, 255, 255 );
		Color colorEnd = GameMakeColor( 255, 128, 128, 255 );
		TheDisplay->drawLine( pixelStart.x, pixelStart.y, pixelEnd.x, pixelEnd.y, 1.0f, colorStart, colorEnd );
	*/


	}

	// TheSuperHackers @perf bobtista 03/06/2026 Submit whatever remains in the open point-group bucket.
	if (batchPointGroups && batchBase > 0)
	{
		flushPointGroupBatch( rinfo, batchTexture, batchShader, batchBillboard, batchVolumeDepth, batchBase );
		batchTexture->Release_Ref();
		batchTexture = nullptr;
		batchBase = 0;
	}

		/// @todo lorenzen sez: this should be debug only:
	TheParticleSystemManager->setOnScreenParticleCount(m_onScreenParticleCount);

	//Draw any particles belonging to weather effects
	if (TheSnowManager)
		((W3DSnowManager *)TheSnowManager)->render(rinfo);

	//Now process screen smudges which are particles that distort the background behind them.
	if(TheSmudgeManager)
	{
		((W3DSmudgeManager *)TheSmudgeManager)->render(rinfo);
	}
}
