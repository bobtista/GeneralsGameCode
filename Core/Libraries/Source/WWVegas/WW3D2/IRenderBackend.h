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

// TheSuperHackers @refactor bobtista 10/04/2026 Introduce IRenderBackend
// abstract interface so WW3D2 rendering can be re-targeted to modern backends
// (bgfx, Diligent, etc.) while the existing DX8 path stays functional as the
// reference implementation. See Core/Libraries/Source/WWVegas/WW3D2/RENDER_BACKEND.md.

#pragma once

#include "ww3dformat.h"

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
//
// Kept lightweight deliberately. IRenderBackend.h must be includable without
// dragging in the full WW3D2 header graph, so callers only pay for the types
// they actually use.
//
// All W3D classes passed through this interface are referenced by pointer or
// reference; none of them need a full definition in this header.

class ShaderClass;
class VertexMaterialClass;
class TextureBaseClass;
class TextureClass;
class ZTextureClass;
class SurfaceClass;
class VertexBufferClass;
class IndexBufferClass;
class DynamicVBAccessClass;
class DynamicIBAccessClass;
class LightClass;
class LightEnvironmentClass;
class Matrix4x4;
class Matrix3D;
class Vector3;

// -----------------------------------------------------------------------------
// POD types owned by the interface
// -----------------------------------------------------------------------------

enum TransformKind
{
    // Values chosen so they can be mapped directly to D3DTS_* inside the
    // DX8Backend without a branch. A modern backend ignores these indices
    // and uses whichever matrix storage is convenient for it.
    RB_TRANSFORM_VIEW       = 2,  // D3DTS_VIEW
    RB_TRANSFORM_PROJECTION = 3,  // D3DTS_PROJECTION
    RB_TRANSFORM_WORLD      = 256 // D3DTS_WORLD
};

struct RenderBackendViewport
{
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
    float min_z;
    float max_z;
};

// TheSuperHackers @refactor bobtista 10/04/2026 Phase 3B interface extension
// to unblock W3DStatusCircle fade effects and FlatHeightMap shroud trickery
// without exposing raw D3DRENDERSTATETYPE in the interface. See PHASE3B.md.
//
// Values chosen to match D3DBLENDOP_* / D3DBLEND_* directly so the DX8Backend
// can cast without a branch. Modern backends translate these to their native
// blend-state representation.

enum BlendOp
{
    RB_BLEND_OP_ADD          = 1, // D3DBLENDOP_ADD
    RB_BLEND_OP_SUBTRACT     = 2, // D3DBLENDOP_SUBTRACT
    RB_BLEND_OP_REV_SUBTRACT = 3, // D3DBLENDOP_REVSUBTRACT
    RB_BLEND_OP_MIN          = 4, // D3DBLENDOP_MIN
    RB_BLEND_OP_MAX          = 5  // D3DBLENDOP_MAX
};

enum BlendFactor
{
    RB_BLEND_ZERO            = 1,  // D3DBLEND_ZERO
    RB_BLEND_ONE             = 2,  // D3DBLEND_ONE
    RB_BLEND_SRC_COLOR       = 3,  // D3DBLEND_SRCCOLOR
    RB_BLEND_INV_SRC_COLOR   = 4,  // D3DBLEND_INVSRCCOLOR
    RB_BLEND_SRC_ALPHA       = 5,  // D3DBLEND_SRCALPHA
    RB_BLEND_INV_SRC_ALPHA   = 6,  // D3DBLEND_INVSRCALPHA
    RB_BLEND_DEST_ALPHA      = 7,  // D3DBLEND_DESTALPHA
    RB_BLEND_INV_DEST_ALPHA  = 8,  // D3DBLEND_INVDESTALPHA
    RB_BLEND_DEST_COLOR      = 9,  // D3DBLEND_DESTCOLOR
    RB_BLEND_INV_DEST_COLOR  = 10, // D3DBLEND_INVDESTCOLOR
    RB_BLEND_SRC_ALPHA_SAT   = 11  // D3DBLEND_SRCALPHASAT
};

// TheSuperHackers @refactor bobtista 10/04/2026 Phase 3F stencil state
// extension. Generic CompareFunc enum is also reusable for depth-test
// comparison in a future phase. See PHASE3F.md.
enum CompareFunc
{
    // Values match D3DCMP_* 1..8 directly so DX8Backend can cast.
    RB_CMP_NEVER         = 1,
    RB_CMP_LESS          = 2,
    RB_CMP_EQUAL         = 3,
    RB_CMP_LESS_EQUAL    = 4,
    RB_CMP_GREATER       = 5,
    RB_CMP_NOT_EQUAL     = 6,
    RB_CMP_GREATER_EQUAL = 7,
    RB_CMP_ALWAYS        = 8
};

// TheSuperHackers @refactor bobtista 14/04/2026 Phase 4F. D3DFILLMODE
// values match D3DFILL_* so DX8Backend can cast directly.
enum FillMode
{
    RB_FILL_POINT     = 1,   // D3DFILL_POINT
    RB_FILL_WIREFRAME = 2,   // D3DFILL_WIREFRAME
    RB_FILL_SOLID     = 3    // D3DFILL_SOLID
};

enum StencilOp
{
    // Values match D3DSTENCILOP_* 1..8 directly so DX8Backend can cast.
    RB_STENCIL_OP_KEEP     = 1,
    RB_STENCIL_OP_ZERO     = 2,
    RB_STENCIL_OP_REPLACE  = 3,
    RB_STENCIL_OP_INCR_SAT = 4,
    RB_STENCIL_OP_DECR_SAT = 5,
    RB_STENCIL_OP_INVERT   = 6,
    RB_STENCIL_OP_INCR     = 7,
    RB_STENCIL_OP_DECR     = 8
};

// -----------------------------------------------------------------------------
// IRenderBackend — abstract W3D-facing rendering interface
// -----------------------------------------------------------------------------
//
// This interface exposes the *high-level* subset of DX8Wrapper's public API:
// the calls that take and return W3D types (ShaderClass, TextureBaseClass,
// Matrix4x4, etc.) and are backend-neutral by construction. The low-level
// D3D8-specific entry points on DX8Wrapper (Set_DX8_Render_State,
// _Create_DX8_Texture, _Get_D3D_Device8, etc.) are NOT exposed here and
// remain reachable only through DX8Wrapper's static methods. Code that
// needs them is DX8-only and must be migrated during later phases.
//
// **Method names intentionally match the existing DX8Wrapper names** so
// that migrating callers is a mechanical `DX8Wrapper::X(...)` →
// `g_renderBackend->X(...)` rewrite with minimal diff noise.
//
// **This header is included from VC6-compiled translation units** (the
// DX8 reference path and the tools). Keep it C++98-compatible:
//   - No <memory>, no <string>, no <vector> or other STL in signatures
//   - No `override`, `= default`, `= delete`, `auto`, `constexpr`
//   - `nullptr` is OK (the project has a VC6 shim)
//   - POD structs for parameter bundles
//   - Forward-declare W3D types rather than including their headers
//
// Implementations (DX8Backend.cpp, future BgfxBackend.cpp, etc.) can use
// whatever C++ features the project's main build allows.

class IRenderBackend
{
public:
    virtual ~IRenderBackend() {}

    // -------------------------------------------------------------------------
    // Backend lifecycle
    // -------------------------------------------------------------------------
    //
    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4 session 1.
    // Initialize is called once after DX8Wrapper has finished its own device
    // setup, with the game's main HWND and current back-buffer dimensions.
    // DX8Backend treats it as a no-op (DX8Wrapper still owns the real device).
    // BgfxBackend uses it to call bgfx::init. Shutdown is the symmetric
    // teardown, called before DX8Wrapper releases its device.
    //
    // These are in addition to the existing per-scene Begin_Scene / End_Scene
    // pair, which are called every frame.

    virtual void Initialize(void * hwnd, int width, int height) = 0;
    virtual void Shutdown() = 0;

    // -------------------------------------------------------------------------
    // Device state queries
    // -------------------------------------------------------------------------

    virtual bool Is_Device_Lost() const = 0;
    virtual bool Has_Stencil() = 0;
    virtual WW3DFormat Get_Back_Buffer_Format() = 0;
    virtual SurfaceClass * Get_Back_Buffer(unsigned int num) = 0;
    virtual void Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit) = 0;

    // -------------------------------------------------------------------------
    // Frame lifecycle
    // -------------------------------------------------------------------------

    virtual void Begin_Scene() = 0;
    virtual void End_Scene(bool flip_frame) = 0;
    virtual void Flip_To_Primary() = 0;
    // Defaults match DX8Wrapper::Clear so existing call sites that supplied
    // only the first 3-4 arguments compile unchanged after migration.
    virtual void Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha = 0.0f, float z = 1.0f, unsigned int stencil = 0) = 0;
    virtual void Set_Viewport(const RenderBackendViewport & viewport) = 0;

    // -------------------------------------------------------------------------
    // Vertex / index buffers
    // -------------------------------------------------------------------------

    virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream = 0) = 0;
    virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba) = 0;
    virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset) = 0;
    virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset) = 0;
    virtual void Set_Index_Buffer_Index_Offset(unsigned int offset) = 0;

    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4C.4 write-side
    // capture hooks. The W3D engine writes vertex/index data through
    // VertexBufferClass::WriteLockClass / IndexBufferClass::WriteLockClass
    // (and the various Copy() helpers). At unlock time the data is sitting
    // in a CPU-mapped pointer that the engine just wrote into - that is
    // the safe moment for the bgfx backend to grab a copy and create its
    // own GPU buffer. The DX8 backend ignores these calls; only BgfxBackend
    // uses them. Default empty implementations so existing call sites that
    // do not need them are not forced to override.
    virtual void Capture_Vertex_Data(const VertexBufferClass * /*vb*/,
                                     const void * /*data*/,
                                     unsigned int /*size_bytes*/) {}
    virtual void Capture_Index_Data(const IndexBufferClass * /*ib*/,
                                    const void * /*data*/,
                                    unsigned int /*size_bytes*/) {}

    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.2 dynamic
    // capture hooks. Same pattern as above but for DynamicVBAccessClass /
    // DynamicIBAccessClass. The data pointer and size describe just the
    // sub-range the caller locked - not the entire dynamic ring buffer.
    // BgfxBackend copies the sub-range into a per-frame transient buffer
    // keyed by the access class pointer, so the next Set_Vertex_Buffer /
    // Set_Index_Buffer call on the same access class instance picks it up.
    virtual void Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * /*vba*/,
                                             const void * /*data*/,
                                             unsigned int /*size_bytes*/) {}
    virtual void Capture_Dynamic_Index_Data(const DynamicIBAccessClass * /*iba*/,
                                            const void * /*data*/,
                                            unsigned int /*size_bytes*/) {}

    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.6 sub-range
    // capture. Rigid mesh category containers fill their shared VB / IB
    // via AppendLockClass one sub-range at a time. BgfxBackend creates a
    // bgfx dynamic buffer the first time it sees a VB / IB and updates
    // the sub-range in place. start_vertex / start_index is in elements
    // (verts or shorts), size_bytes is in bytes.
    virtual void Capture_Vertex_Sub_Range(const VertexBufferClass * /*vb*/,
                                          const void * /*data*/,
                                          unsigned int /*start_vertex*/,
                                          unsigned int /*size_bytes*/) {}
    virtual void Capture_Index_Sub_Range(const IndexBufferClass * /*ib*/,
                                         const void * /*data*/,
                                         unsigned int /*start_index*/,
                                         unsigned int /*size_bytes*/) {}

    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.12 sorted
    // draw pass routing. SortingRendererClass::Flush_Sorting_Pool wraps
    // its per-batch draw loop in Begin/End_Sorted_Batch_Pass and calls
    // Capture_Sorted_Batch_Transforms once per batch inside the loop.
    // BgfxBackend uses this to route the sorted submits to a dedicated
    // bgfx view id so per-batch matrices cannot stomp the opaque view.
    // Empty defaults = no-op on DX8Backend.
    virtual void Begin_Sorted_Batch_Pass() {}
    virtual void End_Sorted_Batch_Pass() {}
    virtual void Capture_Sorted_Batch_Transforms(const Matrix4x4 & /*world*/,
                                                 const Matrix4x4 & /*view*/) {}
    virtual void Capture_Sorted_Batch_Light(const D3DLIGHT8 & /*light*/, bool /*enabled*/) {}

    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.13 sorted
    // direct-draw path hook. DX8Wrapper::Draw_Sorting_IB_VB handles
    // draws whose currently-bound VB/IB are sorting-type (CPU arrays)
    // by creating an internal dynamic VB/IB, copying a slice out of
    // the sorting buffers at (vba_offset+index_base_offset+min_vertex_index),
    // and issuing the dx8 draw against those inner buffers. The bgfx
    // backend never sees the sorting-VB path otherwise because the
    // outer Set_Vertex_Buffer captured the full sorting VB. This hook
    // lets Draw_Sorting_IB_VB hand the freshly populated inner dynamic
    // VB/IB access classes to the render backend so BgfxBackend can
    // claim their pending transient buffers and submit a correctly
    // remapped draw (start_index=0, min_vertex_index=0). Sets an
    // internal skip flag so the outer Draw_Triangles does not also
    // emit a stale submit. Empty default = no-op on DX8Backend.
    virtual void Submit_Sorted_Draw(const DynamicVBAccessClass & /*dyn_vb*/,
                                    const DynamicIBAccessClass & /*dyn_ib*/,
                                    unsigned short /*polygon_count*/,
                                    unsigned short /*vertex_count*/) {}

    // -------------------------------------------------------------------------
    // State: shaders, materials, textures
    // -------------------------------------------------------------------------

    virtual void Set_Shader(const ShaderClass & shader) = 0;
    virtual void Get_Shader(ShaderClass & shader) = 0;
    virtual void Set_Material(const VertexMaterialClass * material) = 0;
    virtual void Set_Texture(unsigned int stage, TextureBaseClass * texture) = 0;

    virtual void Apply_Render_State_Changes() = 0;
    virtual void Apply_Default_State() = 0;
    virtual void Invalidate_Cached_Render_States() = 0;

    // TheSuperHackers @refactor bobtista 10/04/2026 Phase 3B typed blend +
    // color-write setters (see PHASE3B.md). These exist so subsystems that
    // previously called DX8Wrapper::Set_DX8_Render_State(D3DRS_BLENDOP / ...)
    // can migrate without the interface re-exposing the raw D3DRENDERSTATETYPE.
    virtual void Set_Blend_Op(BlendOp op) = 0;
    virtual void Set_Blend_Factors(BlendFactor src, BlendFactor dest) = 0;
    virtual void Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha) = 0;
    // TheSuperHackers @refactor bobtista 10/04/2026 Phase 3E. Natural complement
    // to the Phase 3B blend extension. See PHASE3E.md.
    virtual void Set_Alpha_Blend_Enable(bool enable) = 0;

    // TheSuperHackers @refactor bobtista 10/04/2026 Phase 3D hardware cursor
    // extension. Lets W3DMouse drive the device's hardware cursor without
    // touching IDirect3DDevice8 directly. See PHASE3D.md.
    virtual void Show_Hardware_Cursor(bool show) = 0;
    virtual void Set_Hardware_Cursor_Image(int hotspot_x, int hotspot_y, SurfaceClass * surface) = 0;
    virtual void Set_Hardware_Cursor_Position(int x, int y) = 0;

    // TheSuperHackers @refactor bobtista 10/04/2026 Phase 3F stencil state
    // group. Each method maps 1:1 onto an existing D3DRS_STENCIL* state. The
    // CompareFunc and StencilOp enums above are reusable for future depth
    // and stencil work. See PHASE3F.md.
    virtual void Set_Stencil_Enable(bool enable) = 0;
    virtual void Set_Stencil_Func(CompareFunc func) = 0;
    virtual void Set_Stencil_Ref(unsigned int ref) = 0;
    virtual void Set_Stencil_Mask(unsigned int mask) = 0;
    virtual void Set_Stencil_Write_Mask(unsigned int mask) = 0;
    virtual void Set_Stencil_Pass_Op(StencilOp op) = 0;
    virtual void Set_Stencil_Fail_Op(StencilOp op) = 0;
    virtual void Set_Stencil_ZFail_Op(StencilOp op) = 0;

    // TheSuperHackers @refactor bobtista 14/04/2026 Phase 4F.1 / 4F.2
    // render-state remainders. These wrap the last D3DRS_* values still
    // being set directly by the terrain / scene / water / snow code
    // (ZBIAS, FILLMODE, ZENABLE/ZFUNC, COLORWRITEENABLE as DWORD mask).
    // The DWORD variant of Set_Color_Write_Mask coexists with the
    // boolean Set_Color_Write_Enable — callers that receive a saved
    // bitmask from GetRenderState use this, callers that know the four
    // channel flags use the boolean form.
    virtual void Set_Z_Bias(int bias) = 0;
    virtual void Set_Fill_Mode(FillMode mode) = 0;
    virtual void Set_Depth_Test_Enable(bool enable) = 0;
    virtual void Set_Depth_Write_Enable(bool enable) = 0;
    virtual void Set_Depth_Func(CompareFunc func) = 0;
    virtual void Set_Color_Write_Mask(unsigned mask) = 0;
    virtual void Set_Lighting_Enable(bool enable) = 0;
    virtual void Set_Texture_Factor(unsigned argb) = 0;

    // TheSuperHackers @refactor bobtista 14/04/2026 Phase 4H tree /
    // grass sway vertex shader hooks. DX8 backends ignore these (they
    // use the real DX8 vertex shader DWORD via Set_Vertex_Shader). bgfx
    // backend uses them to drive its ported vs_trees program. Call
    // order: Set_Tree_Shader_Constants first (per-frame constants),
    // then Set_Tree_Vertex_Shader_Active(true) around the grass draws,
    // then Set_Tree_Vertex_Shader_Active(false) after.
    // swayTable must have 11 float4 entries: [0] = no-sway (0,0,0,0),
    // [1..10] = per-wave offsets.
    virtual void Set_Tree_Shader_Constants(const float swayTable[11][4],
                                           const float shroudOffset[4],
                                           const float shroudScale[4]) {}
    virtual void Set_Tree_Vertex_Shader_Active(bool active) {}

    // -------------------------------------------------------------------------
    // Transforms
    // -------------------------------------------------------------------------

    virtual void Set_Transform(TransformKind transform, const Matrix4x4 & m) = 0;
    virtual void Set_Transform(TransformKind transform, const Matrix3D & m) = 0;
    virtual void Get_Transform(TransformKind transform, Matrix4x4 & m) = 0;
    virtual void Set_World_Identity() = 0;
    virtual void Set_View_Identity() = 0;
    virtual bool Is_World_Identity() = 0;
    virtual bool Is_View_Identity() = 0;
    virtual void Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix,
                                                      float znear, float zfar) = 0;

    // -------------------------------------------------------------------------
    // Lighting and fog
    // -------------------------------------------------------------------------

    virtual void Set_Light(unsigned int index, const LightClass & light) = 0;
    virtual void Set_Ambient(const Vector3 & color) = 0;
    virtual const Vector3 & Get_Ambient() const = 0;
    virtual void Set_Fog(bool enable, const Vector3 & color, float start, float end) = 0;
    virtual bool Get_Fog_Enable() const = 0;
    virtual void Set_Light_Environment(LightEnvironmentClass * light_env) = 0;
    virtual LightEnvironmentClass * Get_Light_Environment() const = 0;

    // Post-ShaderClass render state overrides. The terrain edge blending
    // and other systems set D3D blend/alpha-test state AFTER ShaderClass
    // applies. These methods let the bgfx backend capture the overrides.
    // Empty defaults = forward to DX8Wrapper only (DX8Backend behavior).
    virtual void Override_Blend(unsigned srcBlend, unsigned dstBlend) {}
    virtual void Override_Alpha_Test(bool enable, unsigned ref, unsigned func) {}
    virtual void Override_Alpha_Blend_Enable(bool enable) {}
    virtual void Override_Texcoord_Index(unsigned stage, unsigned uvIndex) {}
    virtual void Override_Terrain_Blend(bool enable) {}
    virtual void Override_Material_Opacity(float opacity) {}
    // Route subsequent draws to the sort view instead of the opaque view.
    // Used by dazzle/lens-flare effects that need to render on top of water.
    virtual void Begin_Effect_Overlay() {}
    virtual void End_Effect_Overlay() {}
    virtual void Clear_State_Overrides() {}

    // -------------------------------------------------------------------------
    // Draw calls
    // -------------------------------------------------------------------------

    virtual void Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) = 0;

    virtual void Draw_Triangles(unsigned int buffer_type,
                                unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) = 0;

    virtual void Draw_Strip(unsigned short start_index,
                            unsigned short index_count,
                            unsigned short min_vertex_index,
                            unsigned short vertex_count) = 0;

    // -------------------------------------------------------------------------
    // Programmable pipeline (GPU vertex / pixel shaders)
    // -------------------------------------------------------------------------
    //
    // These correspond to DX8's programmable shader slots. Modern backends
    // will re-interpret the handles internally; the interface treats the
    // shader id as an opaque unsigned long.

    virtual void Set_Vertex_Shader(unsigned long vertex_shader) = 0;
    virtual void Set_Pixel_Shader(unsigned long pixel_shader) = 0;
    virtual void Set_Vertex_Shader_Constant(int reg, const void * data, int count) = 0;
    virtual void Set_Pixel_Shader_Constant(int reg, const void * data, int count) = 0;

    // -------------------------------------------------------------------------
    // Render targets
    // -------------------------------------------------------------------------

    virtual TextureClass * Create_Render_Target(int width, int height, WW3DFormat format = WW3D_FORMAT_UNKNOWN) = 0;
    virtual void Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture = nullptr) = 0;
    virtual bool Is_Render_To_Texture() = 0;
    virtual void Set_Shadow_Map(int idx, ZTextureClass * ztex) = 0;
    virtual ZTextureClass * Get_Shadow_Map(int idx) = 0;
};
