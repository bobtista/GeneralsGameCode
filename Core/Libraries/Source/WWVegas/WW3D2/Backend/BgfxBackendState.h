/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// TheSuperHackers @refactor bobtista 21/04/2026 Shared render-state structs for the bgfx backend. Included by BgfxBackend.cpp (which defines the instances) and BgfxBackendTextures.cpp (which references them).
#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <bgfx/bgfx.h>

#include "ww3dformat.h"

// Forward declarations — full headers are included by the .cpp files that need the method bodies.
class TextureBaseClass;
class TextureClass;
class VertexMaterialClass;
class VertexBufferClass;
class IndexBufferClass;
class DynamicVBAccessClass;
class DynamicIBAccessClass;

// --- Helper types -----------------------------------------------------------

struct BgfxFramebufferEntry
{
    bgfx::FrameBufferHandle fb;
    bgfx::TextureHandle     colorTex;
    uint16_t                width;
    uint16_t                height;
};

struct BgfxVbCacheEntry {
    bgfx::DynamicVertexBufferHandle handle;
    uint32_t num_verts;
    uint32_t stride;
};

struct BgfxIbCacheEntry {
    bgfx::DynamicIndexBufferHandle handle;
    uint32_t num_indices;
};

struct BgfxPendingRangeUpload
{
    uint32_t startByte = 0;
    uint32_t endByte = 0;
    bool valid = false;
};

struct TextureCacheInfo
{
    unsigned revision;
    uint16_t w;
    uint16_t h;
    int sourceFormat = 0;
    int createFormat = 0;
    uint16_t mipCount = 0;
    uint16_t uploadVariant = 0;
};

struct PendingTransientVB
{
    bool                        valid;
    const DynamicVBAccessClass * owner;
    bgfx::TransientVertexBuffer  tvb;
    bool                        coplanarNormalBias;
};

struct PendingTransientIB
{
    bool                        valid;
    const DynamicIBAccessClass * owner;
    bgfx::TransientIndexBuffer   tib;
};

// --- The 8 render-state structs --------------------------------------------

// Device: created in Initialize(), released in Shutdown(). Never reset during frames.
struct BgfxDevice
{
    bool initialized = false;
    HWND window      = nullptr;
    bool mainWindowShown = false;
    // Set to 0 so any read before Initialize() trips obvious downstream
    // sentinels (clip rects = 0x0, no allocation). The previous 800x600
    // placeholder masked uninitialized-use bugs.
    int  width       = 0;
    int  height      = 0;
    // TheSuperHackers @feature bobtista 08/06/2026 Letterbox present. width/height are the CONTENT
    // (render) size; when letterboxing, the swapchain is larger (swapWidth/swapHeight == window) and
    // the content is drawn at presentOffsetX/Y with black bars filling the rest. When not
    // letterboxing, swap == content and offset == 0 (identical to a direct present).
    bool letterboxRequested = false;
    float letterboxAspectW = 16.0f;
    float letterboxAspectH = 9.0f;
    bool letterboxActive = false;
    int  swapWidth   = 0;
    int  swapHeight  = 0;
    int  presentOffsetX = 0;
    int  presentOffsetY = 0;
    uint32_t msaaResetFlags = 0;
    bool srgbEnabled = false;
    bool vsyncEnabled = false;
    // TheSuperHackers @refactor bobtista 08/06/2026 Device windowed flag and color bit-depth are
    // owned here on bgfx builds; DX8Wrapper mirrors its IsWindowed/BitDepth into these from its
    // Set_Render_Device/Init. Defaults match DX8Wrapper (IsWindowed=false, BitDepth=DEFAULT_BIT_DEPTH=32).
    bool windowed    = false;
    int  bits        = 32;
    // bgfx debug-log callback is a file-local global in BgfxBackend.cpp (g_bgfxCallback); it needs the full BgfxLoggingCallback class definition and only BgfxBackend.cpp uses it.

    // Programs
    bgfx::ProgramHandle uberProgram         = BGFX_INVALID_HANDLE; // single uber program; all TSS combos via uniforms.
    bgfx::ProgramHandle uberInstancedProgram = BGFX_INVALID_HANDLE; // vs_uber_instanced + fs_uber; per-instance world matrix from instance buffer.
    bgfx::ProgramHandle passthroughProgram  = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle treeProgram         = BGFX_INVALID_HANDLE; // vs_trees + fs_uber; enabled via Set_Tree_Vertex_Shader_Active for swaying grass, else reverts to uberProgram.
    bgfx::ProgramHandle shadowVolumeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle shadowApplyProgram  = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle sceneCompositeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle bloomBrightProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle bloomBlurProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle ssaoProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle copyProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle sceneDepthProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle sceneDepthInstancedProgram = BGFX_INVALID_HANDLE; // per-instance world matrix
    bgfx::ProgramHandle shadowCasterProgram = BGFX_INVALID_HANDLE; // alpha-aware shadow caster
    bgfx::ProgramHandle shadowCasterInstancedProgram = BGFX_INVALID_HANDLE; // per-instance world matrix
    bgfx::ProgramHandle smudgeProgram = BGFX_INVALID_HANDLE;

    // Scene color/depth RT. World, water, sorted translucency, and effects
    // render here, then a fullscreen composite pass copies the scene to the
    // backbuffer before UI draws.
    bgfx::FrameBufferHandle sceneFB    = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     sceneColor = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     sceneDepth = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     sceneSmudgeCopy = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle sceneSmudgeCopyFB = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle sceneReadableDepthFB = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     sceneReadableDepth = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     sceneReadableDepthTest = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     bloomBrightTex = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle bloomBrightFB  = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     bloomBlurTex   = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle bloomBlurFB    = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     ssaoTex        = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle ssaoFB         = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     ssaoBlurTex    = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle ssaoBlurFB     = BGFX_INVALID_HANDLE;
    // Sun shadow map: light-POV depth (R32F) rendered with sceneDepthProgram.
    bgfx::TextureHandle     shadowMapTex   = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     shadowMapDepth = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle shadowMapFB    = BGFX_INVALID_HANDLE;
    uint16_t                shadowMapSize  = 0;
    // TheSuperHackers @feature bobtista 23/06/2026 Point-light shadow map for one bright dynamic
    // point light (e.g. the nuke fireball). 1024x1024 perspective depth target.
    bgfx::FrameBufferHandle pointShadowFB  = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     pointShadowTex = BGFX_INVALID_HANDLE;
    uint16_t                pointShadowMapSize = 0;
    uint16_t                sceneWidth = 0;
    uint16_t                sceneHeight = 0;
    // Supersampled scene render size = content size * render scale (1.0-2.0).
    uint16_t                sceneRenderWidth = 0;
    uint16_t                sceneRenderHeight = 0;
    uint16_t                bloomWidth = 0;
    uint16_t                bloomHeight = 0;

    // Default textures + helper VB
    bgfx::TextureHandle       defaultWhiteTexture       = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle       defaultTransparentTexture = BGFX_INVALID_HANDLE;
    // Static VB for a fullscreen black triangle submitted on view 0 every frame. bgfx::setViewClear alone does not emit ClearRenderTargetView on our backbuffer view when only activated via bgfx::touch, so persisted pixels can leak between frames. A real submit makes bgfx process the view fully. See commit ad575e6be.
    bgfx::VertexBufferHandle  fullscreenClearVB         = BGFX_INVALID_HANDLE;

    // Vertex layouts
    bgfx::VertexLayout triangleLayout;
    bgfx::VertexLayout layoutP;
    bgfx::VertexLayout layoutPN;
    bgfx::VertexLayout layoutPNT1;
    bgfx::VertexLayout layoutPNT2;
    bgfx::VertexLayout layoutPT1;
    bgfx::VertexLayout layoutPDT1;
    bgfx::VertexLayout layoutPNDT1;
    bgfx::VertexLayout layoutPNDT2;
};

// Uniforms: all uniform handles. Created once in Initialize, never reset.
struct BgfxUniforms
{
    // Texture samplers
    bgfx::UniformHandle sTex0      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sTex1      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sTex2      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sTex3      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sCloudMap  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sSceneDepth = BGFX_INVALID_HANDLE;

    // Material / TSS
    // TheSuperHackers @performance bobtista 15/06/2026 Packed per-draw material block.
    // The individual uMat*/uTss*/uTex*Transform*/uZBias etc. handles below are uploaded
    // through this single array uniform (one setUniform/draw instead of ~24) to cut
    // submit-thread CPU cost. Slot order is MaterialUniformSlot in BgfxBackend.cpp.
    bgfx::UniformHandle uMaterial    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uMatDiffuse  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uMatAmbient  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uMatEmissive = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uMatSpecular = BGFX_INVALID_HANDLE; // rgb = specular color, w = shininess
    bgfx::UniformHandle uMatFx       = BGFX_INVALID_HANDLE; // x spec strength, y rim strength, z rim power, w emissive boost
    bgfx::UniformHandle uEyePos      = BGFX_INVALID_HANDLE; // xyz = world-space camera position
    bgfx::UniformHandle uAtestParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTssOps0     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTssOps1     = BGFX_INVALID_HANDLE;

    // Lighting
    bgfx::UniformHandle uLightDirs       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uLightColors     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uLightAmbients   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uLightPositions  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uLightParams     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uSceneAmbient    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uLightingEnabled = BGFX_INVALID_HANDLE;

    // Sun shadow map sampling.
    bgfx::UniformHandle uShadowMatrices = BGFX_INVALID_HANDLE; // per-cascade light view-proj (world -> shadow clip)
    bgfx::UniformHandle uShadowParams = BGFX_INVALID_HANDLE; // x texel size, y depth bias, z strength, w enabled
    bgfx::UniformHandle uShadowQuality = BGFX_INVALID_HANDLE; // x: >0.5 = full 36-fetch PCF, else reduced 9-fetch
    bgfx::UniformHandle uSunShadowReceive = BGFX_INVALID_HANDLE; // x>0.5 = this object draw receives the sun cast shadow
    bgfx::UniformHandle sShadowMap    = BGFX_INVALID_HANDLE;
    // TheSuperHackers @feature bobtista 23/06/2026 Point-light shadow map uniforms.
    // u_pointShadowParams: x=active(1)/none(-1), y=bias, z=texel, w=strength.
    // The nuke caster is a dedicated light (not a LightEnvironment slot): u_pointShadowLightPos
    // = world xyz + outer range, u_pointShadowLightColor = diffuse rgb.
    bgfx::UniformHandle uPointShadowMatrix     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uPointShadowParams     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uPointShadowLightPos   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uPointShadowLightColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sPointShadowMap        = BGFX_INVALID_HANDLE;

    // Misc per-draw flags / params
    bgfx::UniformHandle uTexcoordSelect      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTexcoordSelect2     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uProjectedDecalMode  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTexcoordSource      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uVertexColorFlags    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uGrayscaleEnable     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uObjectShroudDim     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uSwayTable           = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uShroudOffset        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uShroudScale         = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uShroudParams        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uCloudParams         = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTexTransform0       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTexTransform1       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTexTransform0Z      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTex1Transform0      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTex1Transform1      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTex1TransformZ      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTex2Transform0      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTex2Transform1      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTexProjected        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uLegacyPixelShaderMode = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uZBias               = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uShadowColor         = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uPostParams          = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uPostTexelSize       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uSmudgeClip          = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uWipeParams          = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uColorGradeParams    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uBloomParams         = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uBloomBlurDir        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sBloom               = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uHdrParams           = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uPostFx2Params       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uSsaoParams          = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uSsaoInvProj         = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uSsaoProj            = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sSsao                = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uSoftParticleParams  = BGFX_INVALID_HANDLE;
    // Polygon-offset equivalent for stencil-shadow-volume passes. bgfx has no state bit for polygon offset, so the post-projection Z bias is applied in vs_shadow_volume.sc. .x is the offset; negative = toward the camera.
    bgfx::UniformHandle uShadowBias          = BGFX_INVALID_HANDLE;
};

// Draw: per-draw pipeline state + uniform values consumed by SubmitEngineDraw.
struct BgfxDraw
{
    // Pipeline state
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    uint64_t            state   = 0;
    uint64_t            blendFuncBits = 0;
    bool                alphaBlendEnabled = false;
    bool                alphaBlendExplicitlySet = false;
    bool                depthTestEnabled = true;
    bool                depthWriteEnabled = true;
    uint64_t            depthFuncBits = BGFX_STATE_DEPTH_TEST_LEQUAL;
    unsigned            depthFunc = 4; // == IRenderBackend RB_CMP_LESS_EQUAL

    // Textures + per-stage sampler flags
    bgfx::TextureHandle tex[4] = {
        BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE,
        BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE
    };
    uint32_t samplerFlags[4] = { 0, 0, 0, 0 };
    bool mipFilterDisabled[4] = { false, false, false, false };
    unsigned texcoordIndex[4] = { 0, 1, 2, 3 };
    unsigned textureTransformFlags[4] = { 0, 0, 0, 0 };
    bool textureIsMissing[4] = { false, false, false, false };
    TextureBaseClass * sourceTextures[4] = { nullptr, nullptr, nullptr, nullptr };
    const VertexMaterialClass * sourceMaterial = nullptr;
    bool explicitMaterialState = false;

    // Buffers (static + transient variants)
    bgfx::DynamicVertexBufferHandle vb       = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle  ib       = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle        staticVB = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle         staticIB = BGFX_INVALID_HANDLE;
    const VertexBufferClass *       vbOwner  = nullptr;
    const IndexBufferClass *        ibOwner  = nullptr;
    unsigned short                  ibOffset = 0;
    bool                        useStaticVB = false;
    bool                        useStaticIB = false;
    bool                        useTransientVB = false;
    bgfx::TransientVertexBuffer transientVB    = {};
    bool                        useTransientIB = false;
    bgfx::TransientIndexBuffer  transientIB    = {};
    PendingTransientVB pendingVB = { false, nullptr, {}, false };
    PendingTransientIB pendingIB = { false, nullptr, {} };
    const DynamicVBAccessClass * activeTransientVBOwner = nullptr;
    const DynamicIBAccessClass * activeTransientIBOwner = nullptr;
    bool activeVertexNormalBias = false;

    // Cull + stencil
    int      cullModeBits       = 0; // 0=NONE, 1=CW, 2=CCW
    bool     stencilEnabled     = false;
    uint32_t stencilRef         = 0;
    uint32_t stencilReadMask    = 0xFF;
    uint32_t stencilFuncBits    = BGFX_STENCIL_TEST_ALWAYS;
    uint32_t stencilPassOpBits  = BGFX_STENCIL_OP_PASS_Z_KEEP;
    uint32_t stencilFailOpBits  = BGFX_STENCIL_OP_FAIL_S_KEEP;
    uint32_t stencilZFailOpBits = BGFX_STENCIL_OP_FAIL_Z_KEEP;
    uint32_t shadowStencilFront = BGFX_STENCIL_NONE;
    uint32_t shadowStencilBack  = BGFX_STENCIL_NONE;

    // Uniform VALUES pushed via bgfx::setUniform per submit
    uint64_t blendEquationBits = 0;
    float matDiffuse[4]       = { 1.0f, 1.0f, 1.0f, 1.0f };
    float matAmbient[4]       = { 1.0f, 1.0f, 1.0f, 1.0f };
    float matEmissive[4]      = { 0.0f, 0.0f, 0.0f, 0.0f };
    float matSpecular[4]      = { 0.0f, 0.0f, 0.0f, 1.0f }; // rgb = specular color, w = shininess
    float grayscaleEnable[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float tssOps0[4]          = { 3.0f, 3.0f, 0.0f, 0.0f };
    float tssOps1[4]          = { 0.0f, 0.0f, 0.0f, 0.0f };
    float shaderTssOps0[4]    = { 3.0f, 3.0f, 0.0f, 0.0f };
    float shaderTssOps1[4]    = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool atestEnabled         = false;
    float atestRef            = 0.0f;
    float atestFunc           = 0.0f;
    bool shaderAlphaBlendEnabled = false;
    uint64_t shaderBlendFuncBits = 0;
    float shaderAtestRef      = 0.0f;
    float shaderAtestFunc     = 0.0f;
    float texcoordSelect[4]   = { 0.0f, 0.0f, 0.0f, 0.0f };
    // .x/.y are used by vs_uber for stage-1 UV routing and transform state.
    // .w tags additive blend draws for black-matte discard in fs_uber.
    float texcoordSelect2[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float projectedDecalMode[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float texcoordSource[4]   = { 0.0f, 0.0f, 0.0f, 0.0f };
    float vertexColorFlags[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    float texTransform0[4]    = { 1.0f, 0.0f, 0.0f, 0.0f };
    float texTransform1[4]    = { 0.0f, 1.0f, 0.0f, 0.0f };
    float texTransform0Z[4]   = { 0.0f, 0.0f, 1.0f, 0.0f };
    float tex1Transform0[4]   = { 1.0f, 0.0f, 0.0f, 0.0f };
    float tex1Transform1[4]   = { 0.0f, 1.0f, 0.0f, 0.0f };
    float tex1TransformZ[4]   = { 0.0f, 0.0f, 1.0f, 0.0f };
    float tex2Transform0[4]   = { 1.0f, 0.0f, 0.0f, 0.0f };
    float tex2Transform1[4]   = { 0.0f, 1.0f, 0.0f, 0.0f };
    // .x > 0.5 = stage 0 uses projected 3-component texture coords - divide UV.xy
    // by the third texcoord output produced from texTransform0Z. .y same for
    // stage 1. Used by TexProjectClass perspective projection of building
    // floor emblems / faction icons.
    float texProjected[4]     = { 0.0f, 0.0f, 0.0f, 0.0f };
    float cloudParams[4]      = { 0.0f, 0.0f, 0.0f, 0.0f };
    bgfx::TextureHandle cloudTex = BGFX_INVALID_HANDLE;
    float lightDirs[4][4] = {
        { 0.35f, 0.55f, 0.75f, 1.0f },
        { 0.0f,  0.0f,  0.0f,  0.0f },
        { 0.0f,  0.0f,  0.0f,  0.0f },
        { 0.0f,  0.0f,  0.0f,  0.0f }
    };
    float lightColors[4][4] = {
        { 0.75f, 0.75f, 0.75f, 1.0f },
        { 0.0f,  0.0f,  0.0f,  0.0f },
        { 0.0f,  0.0f,  0.0f,  0.0f },
        { 0.0f,  0.0f,  0.0f,  0.0f }
    };
    float lightAmbients[4][4] = {
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };
    float lightPositions[4][4] = {
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };
    float lightParams[4][4] = {
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };
    // TheSuperHackers @feature bobtista 23/06/2026 Point-light shadow: world->shadowClip matrix
    // and params (x=lightIndex(-1=none), y=bias, z=texel, w=strength) for the brightest point light.
    float pointShadowMatrix[16] = { 0.0f };
    float pointShadowParams[4]  = { -1.0f, 0.0f, 0.0f, 0.0f };
    // The dedicated nuke caster light published by SetupPointShadowView: world xyz + outer range
    // in pointShadowLightPos, diffuse rgb in pointShadowLightColor. fs_uber applies this light and
    // its shadow directly (dynamic lights never enter the per-object LightEnvironment).
    float pointShadowLightPos[4]   = { 0.0f, 0.0f, 0.0f, 0.0f };
    float pointShadowLightColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool  pointShadowLightValid    = false;
    float sceneAmbient[4]     = { 0.45f, 0.45f, 0.45f, 1.0f };
    float lightingEnabled[4]  = { 1.0f, 0.0f, 0.0f, 0.0f };
    bool  fvfHasNormal        = false;
    // .x = post-projection clip-space Z offset (negative pushes toward camera), applied
    // in vs_uber.sc as gl_Position.z -= u_zBias.x * gl_Position.w; sourced from the
    // cached z-bias at submit time so decals keep their anti-z-fighting bias.
    float zBias[4]            = { 0.0f, 0.0f, 0.0f, 0.0f };
    // x>0.5 marks an opaque object draw (unit/structure/prop) that receives the sun cast shadow.
    float sunShadowReceive[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    unsigned zBiasUnits       = 0;
    float normalBias[4]       = { 0.0f, 0.0f, 0.0f, 0.0f };
    float legacyPixelShaderMode[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float swayTable[11][4]    = {{0}};
    float shroudOffset[4]     = { 0.0f, 0.0f, 0.0f, 0.0f };
    float shroudScale[4]      = { 0.0f, 0.0f, 1.0f, 1.0f };
    float shroudTextureParams[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    float objectShroudDim[4]  = { 1.0f, 0.0f, 0.0f, 0.0f };
    bool shroudTextureParamsValid = false;
    bool delayedObjectShroudPass = false;

    // Instancing batch state
    bgfx::InstanceDataBuffer instanceBatch;
    unsigned instanceCount = 0;
    unsigned instanceMax = 0;
    bool instanceBatchActive = false;
    // True only while the instanced batch submits, so the shadow/depth recast re-binds the instance
    // buffer and uses the instanced caster programs instead of the single-copy path.
    bool drawIsInstanced = false;
};

// Overrides: transient per-shader overrides. Reset by Clear_State_Overrides (called from Set_Shader).
struct BgfxOverrides
{
    bool     blendActive        = false;
    uint64_t blendBits          = 0;
    bool     blendEnableActive  = false;
    bool     blendEnableValue   = false;
    bool     atestActive        = false;
    float    atestRef           = 0.0f;
    float    atestFunc          = 0.0f;
    bool     suppressDraw       = false;
    int      colorWriteOverride = -1;

    void Reset()
    {
        blendActive        = false;
        blendBits          = 0;
        blendEnableActive  = false;
        blendEnableValue   = false;
        atestActive        = false;
        atestRef           = 0.0f;
        atestFunc          = 0.0f;
        suppressDraw       = false;
        colorWriteOverride = -1;
    }

    void SetBlend(uint64_t bits)
    {
        blendActive = true;
        blendBits   = bits;
    }

    void SetBlendEnable(bool enable)
    {
        blendEnableActive = true;
        blendEnableValue = enable;
    }
};

// Views: flags that control which bgfx view a submit routes to + ephemeral per-pass state.
struct BgfxViewFlags
{
    bool overlay2DActive           = false;
    bool renderToTexture           = false;
    TextureClass * renderTargetTexture = nullptr;
    bool waterOverrideActive       = false;
    bool waterOverlayActive        = false;
    bool effectOverlayActive       = false;
    bool smudgeActive              = false;
    float smudgeClip[4]            = { 1.0f, 1.0f, 1.0f, 1.0f };
    uint16_t sceneViewportX        = 0;
    uint16_t sceneViewportY        = 0;
    uint16_t sceneViewportW        = 0;
    uint16_t sceneViewportH        = 0;
    bool inSortFlush               = false;
    bool treeShaderActive          = false;
    bool shadowVolumeActive        = false;
    bool shroudTexturePassActive   = false;
    bool objectShroudTexturePassActive = false;
    unsigned shroudTexturePassStage = 0;
    bool projectedShadowDecalActive = false;
    unsigned projectedDecalMode    = 0;
    bool skipNextSubmitEngineDraw  = false;
    bool pointGroupRenderActive    = false;
    bool streakRenderActive        = false;
    bool meshRenderActive          = false;
    unsigned sortedBatchDrawFlags  = 0;
    bool sortedBatchMaterialCaptured = false;
};

// Frame: per-frame matrices and captured view/proj copies.
struct BgfxFrame
{
    float world[16]          = {};
    float view[16]           = {};
    float proj[16]           = {};
    bool  cameraProjDirty    = true;

    float cameraView[16]     = {};
    float cameraProj[16]     = {};
    bool  cameraCaptured     = false;

    // Sun shadow map: per-cascade light view-proj for the current frame, and whether
    // the shadow pass is active. 3 cascades; matrices are contiguous for setUniform.
    float shadowMatrices[3 * 16] = {};
    bool  shadowActive       = false;

    float sortWorld[16]      = {};
    float sortWorldRaw[16]   = {};
    float sortProj[16]       = {};
    bool  sortProjCaptured   = false;

};

// Stats: per-frame backend counters used by debug builds to profile draw/state churn.
struct BgfxStats
{
    uint32_t frameIndex = 0;
    uint32_t drawCalls = 0;
    uint32_t skippedDraws = 0;

    uint32_t baseSubmits = 0;
    uint32_t sceneDepthSubmits = 0;
    uint32_t shadowVolumeSubmits = 0;
    uint32_t shadowApplySubmits = 0;
    uint32_t smudgeSubmits = 0;
    uint32_t sceneCompositeSubmits = 0;
    uint32_t debugSubmits = 0;

    uint32_t uiDraws = 0;
    uint32_t worldDraws = 0;
    uint32_t waterDraws = 0;
    uint32_t sortedDraws = 0;
    uint32_t effectDraws = 0;
    uint32_t rttDraws = 0;
    uint32_t smudgeDraws = 0;

    uint32_t textureBinds = 0;
    uint32_t textureCreates = 0;
    uint32_t textureUploads = 0;
    uint32_t textureCopies = 0;
    uint32_t materialUniformUploads = 0;
    uint32_t lightUniformUploads = 0;
    uint32_t uniformCommands = 0;
    uint32_t materialUniformCommands = 0;
    uint32_t lightUniformCommands = 0;
    uint32_t shadowUniformCommands = 0;
    uint32_t pointShadowUniformCommands = 0;
    uint32_t textureTransformUpdates = 0;
    uint32_t renderStateCopies = 0;

    uint32_t transientVbAllocations = 0;
    uint32_t transientIbAllocations = 0;
    uint32_t transientVbDraws = 0;
    uint32_t transientIbDraws = 0;
    uint32_t dynamicVbAllocations = 0;
    uint32_t dynamicIbAllocations = 0;
    uint32_t instancedSavedDrawCalls = 0;
    uint32_t sortedReplayCalls = 0;
    long long sortedReplayTotalTicks = 0;
    long long sortedReplayShaderTicks = 0;
    long long sortedReplayMaterialTicks = 0;
    long long sortedReplayTextureTicks = 0;
    long long sortedReplayTransformTicks = 0;
    long long sortedReplayLightTicks = 0;
};

// Caches: long-lived resource maps.
struct BgfxCaches
{
    std::unordered_map<const VertexBufferClass *, BgfxVbCacheEntry>      vb;
    std::unordered_map<const IndexBufferClass  *, BgfxIbCacheEntry>      ib;
    std::unordered_map<const VertexBufferClass *, BgfxPendingRangeUpload> pendingVbRangeUploads;
    std::unordered_map<const IndexBufferClass  *, BgfxPendingRangeUpload> pendingIbRangeUploads;
    std::unordered_map<const TextureBaseClass  *, bgfx::TextureHandle>   texture;
    std::unordered_map<const TextureBaseClass  *, TextureCacheInfo>      textureInfo;
    std::unordered_map<const TextureBaseClass  *, bgfx::TextureHandle>   textureBaseMip;
    std::unordered_map<const TextureBaseClass  *, TextureCacheInfo>      textureBaseMipInfo;
    std::unordered_map<const TextureBaseClass  *, BgfxFramebufferEntry>  framebuffer;
    std::unordered_map<const TextureBaseClass  *, bool>                  renderTarget;
    std::vector<bgfx::TextureHandle> deferredDestroys;     // current frame
    std::vector<bgfx::TextureHandle> deferredDestroysPrev; // previous frame, safe to destroy
    // TheSuperHackers @bugfix bobtista 07/06/2026 Render-target framebuffers (water RTTs, etc.)
    // dropped by a resolution-change device reset must outlive the in-flight frame, same as the
    // texture/buffer queues. Queue the framebuffer handle (which owns its attachment textures)
    // here and destroy it one frame later.
    std::vector<bgfx::FrameBufferHandle> deferredDestroyFB;
    std::vector<bgfx::FrameBufferHandle> deferredDestroyFBPrev;
    // TheSuperHackers @bugfix bobtista 02/06/2026 Dynamic VB/IB handles orphaned by a
    // mid-frame resize must outlive the in-flight frame that may still reference them;
    // destroyed one frame later, like the textures above.
    std::vector<bgfx::DynamicVertexBufferHandle> deferredDestroyVB;
    std::vector<bgfx::DynamicVertexBufferHandle> deferredDestroyVBPrev;
    std::vector<bgfx::DynamicIndexBufferHandle>  deferredDestroyIB;
    std::vector<bgfx::DynamicIndexBufferHandle>  deferredDestroyIBPrev;
    // TheSuperHackers @bugfix bobtista 02/06/2026 Immutable static VB/IB handles dropped
    // when a static-eligible buffer demotes to the dynamic path are still referenced by the
    // draw recorded earlier this frame. Defer their destroy one frame, same as the dynamic
    // queues above, to avoid the single in-gameplay "RefCount is 1 (expected 0)" warning.
    std::vector<bgfx::VertexBufferHandle> deferredDestroyStaticVB;
    std::vector<bgfx::VertexBufferHandle> deferredDestroyStaticVBPrev;
    std::vector<bgfx::IndexBufferHandle>  deferredDestroyStaticIB;
    std::vector<bgfx::IndexBufferHandle>  deferredDestroyStaticIBPrev;
};

// ---asset-ingress resource table -----------------------------------
//
// Resources created via IRenderBackend::Create_Texture / Create_Vertex_Buffer
// etc. are tracked here. RenderResource.id is a monotonically-assigned index
// into BgfxResourceRegistry::table; table[id] holds the bgfx handle(s) plus
// an optional legacy mirror pointer for the ref-popup build.

enum BgfxResourceKind
{
    BGFX_RR_KIND_NONE        = 0,
    BGFX_RR_KIND_TEXTURE     = 1,
    BGFX_RR_KIND_VB          = 2,
    BGFX_RR_KIND_IB          = 3
};

struct BgfxResourceEntry
{
    BgfxResourceKind kind;
    bgfx::TextureHandle              texture;
    bgfx::FrameBufferHandle          fb;
    bgfx::VertexBufferHandle         vb;
    bgfx::IndexBufferHandle          ib;
    bgfx::DynamicVertexBufferHandle  dvb;
    bgfx::DynamicIndexBufferHandle   dib;
    uint16_t width;
    uint16_t height;
    void * d3d_mirror;               // raw legacy mirror pointer, ref-popup only; nullptr in standalone
    void * owner;                    // TextureBaseClass/VertexBufferClass/IndexBufferClass for loaded-resource caches
    // TheSuperHackers @perf bobtista 02/06/2026 Content hash of the data last captured into
    // the static vb/ib; when a re-upload hash-matches the GPU copy the recreate is skipped.
    uint64_t vbContentHash;
    uint64_t ibContentHash;
};

struct BgfxResourceRegistry
{
    // id 0 is reserved for kInvalidRenderResource. Allocate starting at 1.
    std::unordered_map<uint64_t, BgfxResourceEntry> table;
    uint64_t next_id;
};

extern BgfxResourceRegistry g_resourceRegistry;

// --- Shared globals ---------------------------------------------------------
// Defined in BgfxBackend.cpp.
extern BgfxDevice     g_device;
extern BgfxUniforms   g_uniforms;
extern BgfxDraw       g_draw;
extern BgfxOverrides  g_overrides;
extern BgfxViewFlags  g_views;
extern BgfxFrame      g_frame;
extern BgfxStats      g_stats;
extern BgfxCaches     g_caches;

// --- Helpers shared across BgfxBackend*.cpp ---------------------------------
// Defined in BgfxBackendTextures.cpp.
bgfx::TextureHandle EnsureBgfxTexture(TextureBaseClass * tex, bool baseMipOnly = false);
