/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// TheSuperHackers @refactor bobtista 21/04/2026 Shared render-state structs for the bgfx backend. Included by BgfxBackend.cpp (which defines the instances) and BgfxBackendTextures.cpp (which references them). See BgfxBackend.cpp for the design rationale of the 7-struct split.
#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

// d3d8 types leak into D3DPtrInfo (we shadow the D3D8 base-texture pointer).
#include <d3d8.h>

#include <bgfx/bgfx.h>

#include "ww3dformat.h"

// Forward declarations — full headers are included by the .cpp files that need the method bodies.
class TextureBaseClass;
class TextureClass;
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

struct D3DPtrInfo { IDirect3DBaseTexture8 * ptr; uint16_t w; uint16_t h; };

struct PendingTransientVB
{
    bool                        valid;
    const DynamicVBAccessClass * owner;
    bgfx::TransientVertexBuffer  tvb;
};

struct PendingTransientIB
{
    bool                        valid;
    const DynamicIBAccessClass * owner;
    bgfx::TransientIndexBuffer   tib;
};

// --- The 7 render-state structs --------------------------------------------

// Device: created in Initialize(), released in Shutdown(). Never reset during frames.
struct BgfxDevice
{
    bool initialized = false;
    HWND window      = nullptr;
    int  width       = 800;
    int  height      = 600;
    // bgfx debug-log callback is a file-local global in BgfxBackend.cpp (g_bgfxCallback); it needs the full BgfxLoggingCallback class definition and only BgfxBackend.cpp uses it.

    // Programs
    bgfx::ProgramHandle uberProgram         = BGFX_INVALID_HANDLE; // single uber program; all TSS combos via uniforms.
    bgfx::ProgramHandle passthroughProgram  = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle treeProgram         = BGFX_INVALID_HANDLE; // vs_trees + fs_uber; enabled via Set_Tree_Vertex_Shader_Active for swaying grass, else reverts to uberProgram.
    bgfx::ProgramHandle shadowCasterProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle shadowVolumeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle shadowApplyProgram  = BGFX_INVALID_HANDLE;

    // Shadow map RT
    bgfx::FrameBufferHandle shadowMapFB    = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     shadowMapDepth = BGFX_INVALID_HANDLE;

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
    bgfx::UniformHandle sShadowMap = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sCloudMap  = BGFX_INVALID_HANDLE;

    // Material / TSS
    bgfx::UniformHandle uMatDiffuse  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uMatEmissive = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uAtestParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTssOps0     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uTssOps1     = BGFX_INVALID_HANDLE;

    // Lighting
    bgfx::UniformHandle uLightDirs       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uLightColors     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uSceneAmbient    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uLightingEnabled = BGFX_INVALID_HANDLE;

    // Misc per-draw flags / params
    bgfx::UniformHandle uTexcoordSelect      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uGrayscaleEnable     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uSwayTable           = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uShroudOffset        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uShroudScale         = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uShroudParams        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uCloudParams         = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uShadowLightViewProj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uShadowColor         = BGFX_INVALID_HANDLE;
    // Polygon-offset equivalent for stencil-shadow-volume passes. bgfx has no state bit for polygon offset, so the post-projection Z bias is applied in vs_shadow_volume.sc. .x is the offset; negative = toward the camera.
    bgfx::UniformHandle uShadowBias          = BGFX_INVALID_HANDLE;
};

// Draw: per-draw pipeline state + uniform values consumed by SubmitEngineDraw.
struct BgfxDraw
{
    // Pipeline state
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    uint64_t            state   = 0;

    // Textures + per-stage sampler flags
    bgfx::TextureHandle tex[4] = {
        BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE,
        BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE
    };
    uint32_t samplerFlags[4] = { 0, 0, 0, 0 };

    // Buffers (static + transient variants)
    bgfx::DynamicVertexBufferHandle vb       = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle  ib       = BGFX_INVALID_HANDLE;
    unsigned short                  ibOffset = 0;
    bool                        useTransientVB = false;
    bgfx::TransientVertexBuffer transientVB    = {};
    bool                        useTransientIB = false;
    bgfx::TransientIndexBuffer  transientIB    = {};
    PendingTransientVB pendingVB = { false, nullptr, {} };
    PendingTransientIB pendingIB = { false, nullptr, {} };

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
    float matDiffuse[4]       = { 1.0f, 1.0f, 1.0f, 1.0f };
    float matEmissive[4]      = { 0.0f, 0.0f, 0.0f, 0.0f };
    float grayscaleEnable[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float tssOps0[4]          = { 3.0f, 3.0f, 0.0f, 0.0f };
    float tssOps1[4]          = { 0.0f, 0.0f, 0.0f, 0.0f };
    float atestRef            = 0.0f;
    float texcoordSelect[4]   = { 0.0f, 0.0f, 0.0f, 0.0f };
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
    float sceneAmbient[4]     = { 0.45f, 0.45f, 0.45f, 1.0f };
    float lightingEnabled[4]  = { 1.0f, 0.0f, 0.0f, 0.0f };
    float swayTable[11][4]    = {{0}};
    float shroudOffset[4]     = { 0.0f, 0.0f, 0.0f, 0.0f };
    float shroudScale[4]      = { 0.0f, 0.0f, 1.0f, 1.0f };
};

// Overrides: transient per-shader overrides. Reset by Clear_State_Overrides (called from Set_Shader).
struct BgfxOverrides
{
    bool     blendActive        = false;
    uint64_t blendBits          = 0;
    bool     atestActive        = false;
    float    atestRef           = 0.0f;
    bool     suppressDraw       = false;
    int      colorWriteOverride = -1;

    void Reset()
    {
        blendActive        = false;
        blendBits          = 0;
        atestActive        = false;
        atestRef           = 0.0f;
        suppressDraw       = false;
        colorWriteOverride = -1;
    }

    void SetBlend(uint64_t bits)
    {
        blendActive = true;
        blendBits   = bits;
    }
};

// Views: flags that control which bgfx view a submit routes to + ephemeral per-pass state.
struct BgfxViewFlags
{
    bool overlay2DActive           = false;
    bool renderToTexture           = false;
    bool waterOverrideActive       = false;
    bool effectOverlayActive       = false;
    bool inSortFlush               = false;
    bool treeShaderActive          = false;
    bool shadowVolumeActive        = false;
    bool skipNextSubmitEngineDraw  = false;
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

    float sortWorld[16]      = {};
    float sortWorldRaw[16]   = {};
    float sortProj[16]       = {};
    bool  sortProjCaptured   = false;

    float shadowLightView[16] = {};
    float shadowLightProj[16] = {};
    bool  shadowLightCaptured = false;

    float shadowSunPosX      = 0.0f;
    float shadowSunPosY      = 0.0f;
    float shadowSunPosZ      = 1500.0f;
    bool  shadowSunPosSet    = false;
};

// Caches: long-lived resource maps.
struct BgfxCaches
{
    std::unordered_map<const VertexBufferClass *, BgfxVbCacheEntry>      vb;
    std::unordered_map<const IndexBufferClass  *, BgfxIbCacheEntry>      ib;
    std::unordered_map<const TextureBaseClass  *, bgfx::TextureHandle>   texture;
    std::unordered_map<const TextureBaseClass  *, D3DPtrInfo>            d3dPtr;
    std::unordered_map<const TextureBaseClass  *, BgfxFramebufferEntry>  framebuffer;
    std::unordered_map<const TextureBaseClass  *, bool>                  renderTarget;
    std::vector<bgfx::TextureHandle> deferredDestroys;     // current frame
    std::vector<bgfx::TextureHandle> deferredDestroysPrev; // previous frame, safe to destroy
};

// --- Shared globals ---------------------------------------------------------
// Defined in BgfxBackend.cpp.
extern BgfxDevice     g_device;
extern BgfxUniforms   g_uniforms;
extern BgfxDraw       g_draw;
extern BgfxOverrides  g_overrides;
extern BgfxViewFlags  g_views;
extern BgfxFrame      g_frame;
extern BgfxCaches     g_caches;

// --- Helpers shared across BgfxBackend*.cpp ---------------------------------
// Defined in BgfxBackendTextures.cpp.
bgfx::TextureHandle EnsureBgfxTexture(TextureBaseClass * tex);
