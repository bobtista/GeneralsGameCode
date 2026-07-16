/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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
// (bgfx, etc.) while the existing DX8 path stays functional as the
// reference implementation.

#pragma once

#include <cstdint>
#include <vector>

#include "ww3dformat.h"

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------

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
class RenderDeviceCleanupHook;
class RenderDeviceDescClass;
struct RenderStateStruct;

// -----------------------------------------------------------------------------
// POD types owned by the interface
// -----------------------------------------------------------------------------

// A single light captured for a sorted batch. Direction follows the legacy
// convention: from the light toward the surface.
struct RenderBackendLight
{
    unsigned int type;
    float position[3];
    float direction[3];
    float diffuse[3];
    float ambient[3];
    float specular[3];
    float range;
    float falloff;
    float attenuation[3];
    float theta;
    float phi;
};

struct RenderBackendImage
{
    unsigned Width = 0;
    unsigned Height = 0;
    WW3DFormat Format = WW3D_FORMAT_UNKNOWN;
    unsigned Pitch = 0;
    std::vector<std::uint8_t> Bytes;

    bool Is_Valid() const
    {
        return Width != 0 && Height != 0 && Pitch != 0 && !Bytes.empty();
    }
};

struct RenderBackendSurfaceDescription
{
    unsigned Width = 0;
    unsigned Height = 0;
    WW3DFormat Format = WW3D_FORMAT_UNKNOWN;

    bool Is_Valid() const
    {
        return Width != 0 && Height != 0 && Format != WW3D_FORMAT_UNKNOWN;
    }
};

static const unsigned RB_MAX_TEXTURE_STAGES = 8;
static const unsigned RB_MAX_LIGHTS = 4;

enum RenderBackendLockFlags
{
    RB_LOCK_NONE = 0,
    RB_LOCK_NOSYSLOCK = 0x00000800,
    RB_LOCK_NOOVERWRITE = 0x00001000,
    RB_LOCK_DISCARD = 0x00002000,
};

enum RenderBackendTextureFilterCapability
{
    RB_TEXTURE_FILTER_MIN_LINEAR,
    RB_TEXTURE_FILTER_MAG_LINEAR,
    RB_TEXTURE_FILTER_MIP_LINEAR,
    RB_TEXTURE_FILTER_MIN_ANISOTROPIC,
    RB_TEXTURE_FILTER_MAG_ANISOTROPIC,
};

enum RenderBackendTextureAddressMode
{
    RB_TEXTURE_ADDRESS_WRAP,
    RB_TEXTURE_ADDRESS_CLAMP,
    RB_TEXTURE_ADDRESS_BORDER,
};

enum RenderBackendTextureSampleFilter
{
    RB_TEXTURE_SAMPLE_NONE,
    RB_TEXTURE_SAMPLE_POINT,
    RB_TEXTURE_SAMPLE_LINEAR,
    RB_TEXTURE_SAMPLE_ANISOTROPIC,
};

enum RenderBackendTextureOpCapability
{
    RB_TEXTURE_OP_SELECTARG1,
    RB_TEXTURE_OP_MODULATE,
    RB_TEXTURE_OP_MODULATE2X,
    RB_TEXTURE_OP_ADD,
    RB_TEXTURE_OP_BUMPENVMAP,
    RB_TEXTURE_OP_BUMPENVMAPLUMINANCE,
    RB_TEXTURE_OP_ADDSMOOTH,
    RB_TEXTURE_OP_SUBTRACT,
    RB_TEXTURE_OP_BLENDTEXTUREALPHA,
    RB_TEXTURE_OP_BLENDCURRENTALPHA,
    RB_TEXTURE_OP_ADDSIGNED,
    RB_TEXTURE_OP_ADDSIGNED2X,
    RB_TEXTURE_OP_MODULATEALPHA_ADDCOLOR,
};

enum RenderBackendTextureOperation
{
    RB_TEXOP_DISABLE     = 1,
    RB_TEXOP_SELECTARG1  = 2,
    RB_TEXOP_SELECTARG2  = 3,
    RB_TEXOP_MODULATE    = 4,
    RB_TEXOP_MODULATE2X  = 5,
    RB_TEXOP_ADD         = 7,
    RB_TEXOP_ADDSIGNED   = 8,
    RB_TEXOP_ADDSIGNED2X = 9,
    RB_TEXOP_SUBTRACT    = 10,
    RB_TEXOP_ADDSMOOTH   = 11,
    RB_TEXOP_BLENDTEXTUREALPHA = 13,
    RB_TEXOP_BLENDCURRENTALPHA = 16,
    RB_TEXOP_MODULATEALPHA_ADDCOLOR = 18,
    RB_TEXOP_BUMPENVMAP  = 22,
    RB_TEXOP_BUMPENVMAPLUMINANCE = 23,
    RB_TEXOP_DOTPRODUCT3 = 24,
    RB_TEXOP_MULTIPLYADD = 25,
};

enum RenderBackendTextureArgument
{
    RB_TEXARG_DIFFUSE         = 0x00000000,
    RB_TEXARG_CURRENT         = 0x00000001,
    RB_TEXARG_TEXTURE         = 0x00000002,
    RB_TEXARG_TFACTOR         = 0x00000003,
    RB_TEXARG_COMPLEMENT      = 0x00000010,
    RB_TEXARG_ALPHAREPLICATE  = 0x00000020,
};

inline RenderBackendTextureArgument operator|(RenderBackendTextureArgument lhs, RenderBackendTextureArgument rhs)
{
    return static_cast<RenderBackendTextureArgument>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));
}

struct RenderBackendLightState
{
    RenderBackendLight lights[RB_MAX_LIGHTS];
    bool enabled[RB_MAX_LIGHTS];
};

struct RenderBackendMaterialState
{
    float diffuse[4];
    float ambient[4];
    float specular[4];
    float emissive[4];
    float power;
};

struct RenderBackendSortedMaterialSnapshot
{
    float diffuse[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float ambient[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float specular[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float emissive[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float vertex_color_flags[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float lighting_enabled[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool valid = false;
};

struct RenderBackendDeviceIdentity
{
    unsigned int vendor_id;
    unsigned int device_id;
    std::uint64_t driver_version;
    int max_simultaneous_textures;
    int pixel_shader_major;
    int pixel_shader_minor;
};

struct RenderBackendTextureLimits
{
    unsigned int max_width;
    unsigned int max_height;
    unsigned int max_volume_extent;
    unsigned int max_aspect_ratio;
};

struct RenderBackendSortedBatchState
{
    const ShaderClass * shader;
    const VertexMaterialClass * material;
    TextureBaseClass * textures[RB_MAX_TEXTURE_STAGES];
    const Matrix4x4 * world;
    const Matrix4x4 * view;
    RenderBackendLightState lights;
    RenderBackendSortedMaterialSnapshot material_snapshot;
    unsigned int draw_flags;
    // TheSuperHackers @performance bobtista 10/07/2026 Backend-opaque pipeline-state
    // word resolved at capture time (see RenderStateStruct::resolved_state_*).
    // Consumed by the packet submit when valid and the resolved-pipeline flag is
    // on; compared against the live derivation under trace.
    std::uint64_t resolved_state;
    bool resolved_state_valid;
};

enum RenderBackendSortedDrawFlags
{
    RB_SORTED_DRAW_NONE        = 0,
    RB_SORTED_DRAW_POINT_GROUP = 1 << 0,
    RB_SORTED_DRAW_STREAK      = 1 << 1,
    RB_SORTED_DRAW_MESH        = 1 << 2
};

enum TransformKind
{
    // Values chosen so they can be mapped directly to legacy transform slots inside the
    // DX8Backend without a branch. A modern backend ignores these indices
    // and uses whichever matrix storage is convenient for it.
    RB_TRANSFORM_VIEW       = 2,
    RB_TRANSFORM_PROJECTION = 3,
    RB_TRANSFORM_WORLD      = 256
};

enum RenderBackendProjectedDecalMode
{
    RB_PROJECTED_DECAL_NONE = 0,
    RB_PROJECTED_DECAL_BLOB_SHADOW = 1,
    RB_PROJECTED_DECAL_ADDITIVE = 2,
    RB_PROJECTED_DECAL_ALPHA = 3,
    RB_PROJECTED_DECAL_MULTIPLY = 4
};

enum RenderBackendShaderKind
{
    RB_SHADER_PIXEL = 0,
    RB_SHADER_VERTEX = 1
};

enum RenderBackendLegacyVertexDeclaration
{
    RB_LEGACY_VERTEX_DECL_XYZNDUV1,
};

enum RenderBackendLegacyPixelShaderMode
{
    RB_LEGACY_PIXEL_SHADER_NONE = 0,
    RB_LEGACY_PIXEL_SHADER_RIVER_WATER = 1,
    RB_LEGACY_PIXEL_SHADER_REFLECTIVE_WATER = 2,
    RB_LEGACY_PIXEL_SHADER_TRAPEZOID_WATER = 3
};

enum RenderBackendTexcoordSource
{
    // Values match the bgfx uber-shader uniform encoding:
    // 0=mesh UV, 1=camera normal, 2=camera reflection, 3=camera position.
    RB_TEXCOORD_MESH_UV = 0,
    RB_TEXCOORD_CAMERA_SPACE_NORMAL = 1,
    RB_TEXCOORD_CAMERA_SPACE_REFLECTION = 2,
    RB_TEXCOORD_CAMERA_SPACE_POSITION = 3
};

enum RenderBackendMaterialColorSource
{
    // Values match legacy material color-source ordinals so DX8Backend can forward them directly.
    RB_MATERIAL_COLOR_SOURCE_MATERIAL = 0,
    RB_MATERIAL_COLOR_SOURCE_COLOR1 = 1,
    RB_MATERIAL_COLOR_SOURCE_COLOR2 = 2
};

enum RenderBackendViewCaptureKind
{
    RB_VIEW_CAPTURE_TACTICAL = 0
};

struct RenderBackendScreenVertex
{
    float x;
    float y;
    float z;
    float w;
    unsigned int diffuse;
    float u0;
    float v0;
    float u1;
    float v1;
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

// TheSuperHackers @refactor bobtista 21/04/2026 Asset ingress types.
// These let W3D asset loaders produce CPU-side pixel / vertex / index data
// and hand it to whichever backend is active, without the loaders caring
// about legacy renderer specifics. Each backend creates its own native GPU resource
// from the bytes and returns an opaque RenderResource handle.

// Opaque handle into any backend's resource table. Backends encode their
// native handle (D3D pointer cast to uint64, bgfx handle index, etc.) into
// the id field. Other code treats it as opaque. id == 0 means invalid.
struct RenderResource
{
    std::uint64_t id;
};

inline bool operator==(const RenderResource & a, const RenderResource & b) { return a.id == b.id; }
inline bool operator!=(const RenderResource & a, const RenderResource & b) { return a.id != b.id; }

// Sentinel for invalid handles, used by the default transitional hooks.
static const RenderResource kInvalidRenderResource = { 0 };

// A single mip level's pixel data for Create_Texture. For compressed
// formats (DXT1/3/5), pitch is set to 0 — backends compute the compressed
// row size from block dimensions and the format enum.
struct MipSlice
{
    const void *   data;
    unsigned int   pitch;       // row pitch in bytes; 0 for compressed
    unsigned int   size_bytes;  // total bytes at this mip level
    unsigned short width;
    unsigned short height;
};

struct TextureDesc
{
    unsigned short   width;
    unsigned short   height;
    WW3DFormat       format;
    unsigned char    mip_count;
    bool             is_render_target;
    const MipSlice * mips;       // array of length mip_count; nullptr when is_render_target
};

struct VertexLayoutDesc
{
    unsigned int fvf;       // legacy FVF bitmap; each backend translates to its native format
    unsigned int stride;    // bytes per vertex
};

struct BufferDesc
{
    unsigned int     size_bytes;
    VertexLayoutDesc layout;    // ignored for index buffers
    bool             dynamic;
};

// TheSuperHackers @refactor bobtista 10/04/2026 Interface extension
// to unblock W3DStatusCircle fade effects and FlatHeightMap shroud trickery
// without exposing raw legacy render-state types in the interface.
//
// Values chosen to match legacy blend ordinals directly so the DX8Backend
// can cast without a branch. Modern backends translate these to their native
// blend-state representation.

enum BlendOp
{
    RB_BLEND_OP_ADD          = 1,
    RB_BLEND_OP_SUBTRACT     = 2,
    RB_BLEND_OP_REV_SUBTRACT = 3,
    RB_BLEND_OP_MIN          = 4,
    RB_BLEND_OP_MAX          = 5
};

enum BlendFactor
{
    RB_BLEND_ZERO            = 1,
    RB_BLEND_ONE             = 2,
    RB_BLEND_SRC_COLOR       = 3,
    RB_BLEND_INV_SRC_COLOR   = 4,
    RB_BLEND_SRC_ALPHA       = 5,
    RB_BLEND_INV_SRC_ALPHA   = 6,
    RB_BLEND_DEST_ALPHA      = 7,
    RB_BLEND_INV_DEST_ALPHA  = 8,
    RB_BLEND_DEST_COLOR      = 9,
    RB_BLEND_INV_DEST_COLOR  = 10,
    RB_BLEND_SRC_ALPHA_SAT   = 11
};

// TheSuperHackers @refactor bobtista 10/04/2026 Stencil state
// extension. Generic CompareFunc enum is also reusable for depth-test
// comparison.
enum CompareFunc
{
    // Values match legacy compare ordinals 1..8 directly so DX8Backend can cast.
    RB_CMP_NEVER         = 1,
    RB_CMP_LESS          = 2,
    RB_CMP_EQUAL         = 3,
    RB_CMP_LESS_EQUAL    = 4,
    RB_CMP_GREATER       = 5,
    RB_CMP_NOT_EQUAL     = 6,
    RB_CMP_GREATER_EQUAL = 7,
    RB_CMP_ALWAYS        = 8
};

// TheSuperHackers @refactor bobtista 28/04/2026 Channel masks for
// Set_Color_Write_Mask. Values match legacy color-write bits so DX8Backend
// can cast directly. Game code that wants to disable color writes
// entirely should pass 0; for ALL channels use RB_COLOR_RGBA.
enum ColorWriteMask
{
    RB_COLOR_RED    = 1,
    RB_COLOR_GREEN  = 2,
    RB_COLOR_BLUE   = 4,
    RB_COLOR_ALPHA  = 8,
    RB_COLOR_RGB    = RB_COLOR_RED | RB_COLOR_GREEN | RB_COLOR_BLUE,
    RB_COLOR_RGBA   = RB_COLOR_RGB | RB_COLOR_ALPHA
};

// TheSuperHackers @refactor bobtista 14/04/2026 Fill-mode
// values match legacy ordinals so DX8Backend can cast directly.
enum FillMode
{
    RB_FILL_POINT     = 1,
    RB_FILL_WIREFRAME = 2,
    RB_FILL_SOLID     = 3
};

// Values match legacy shade ordinals so DX8Backend can cast directly.
enum ShadeMode
{
    RB_SHADE_FLAT    = 1,
    RB_SHADE_GOURAUD = 2,
    RB_SHADE_PHONG   = 3
};

// Values match legacy cull ordinals so DX8Backend can cast directly.
enum CullMode
{
    RB_CULL_NONE = 1,
    RB_CULL_CW   = 2,
    RB_CULL_CCW  = 3
};

enum RenderBackendDeviceStatus
{
    RB_DEVICE_OK = 0,
    RB_DEVICE_LOST,
    RB_DEVICE_NOT_RESET
};

enum RenderBackendMSAAMode
{
    RB_MSAA_NONE = 0,
    RB_MSAA_2X,
    RB_MSAA_4X,
    RB_MSAA_8X
};

enum StencilOp
{
    // Values match legacy stencil operation ordinals 1..8 directly so DX8Backend can cast.
    RB_STENCIL_OP_KEEP     = 1,
    RB_STENCIL_OP_ZERO     = 2,
    RB_STENCIL_OP_REPLACE  = 3,
    RB_STENCIL_OP_INCR_SAT = 4,
    RB_STENCIL_OP_DECR_SAT = 5,
    RB_STENCIL_OP_INVERT   = 6,
    RB_STENCIL_OP_INCR     = 7,
    RB_STENCIL_OP_DECR     = 8
};

// IRenderBackend — abstract W3D-facing rendering interface. Exposes the backend-neutral
// subset of DX8Wrapper's API; method names match DX8Wrapper so callers migrate mechanically.
// Keep this header C++98/VC6-compatible: no STL in signatures, no C++11-only keywords.

class IRenderBackend
{
public:
    virtual ~IRenderBackend() {}

    // TheSuperHackers @feature bobtista 19/04/2026 Runtime check for whether
    // the backend uses its own shader pipeline (bgfx). When true, certain
    // Legacy-specific rendering paths (pixel shaders, shroud passes) should be
    // skipped or replaced with backend-compatible alternatives.
    virtual bool Has_Shader_Pipeline() const { return false; }

    // TheSuperHackers @feature bobtista 19/04/2026 Invalidate a cached
    // texture so the backend re-reads its data on next use. Called after
    // _Copy_DX8_Rects updates a texture's GPU data (font atlas rebuilds).
    virtual void Invalidate_Cached_Texture(TextureBaseClass * /*texture*/) {}

    // Copy a backend render target into a regular cached texture. Legacy
    // projector code renders into a temporary POOL_DEFAULT target, copies it
    // into another POOL_DEFAULT TextureClass, then samples that destination.
    // DX8 owns both GPU resources, but bgfx needs an explicit cache bridge.
    virtual void Copy_Render_Target_To_Texture(TextureClass * /*dst_texture*/,
                                               TextureClass * /*src_render_target*/) {}

    // TheSuperHackers @feature bobtista 20/04/2026 Release a cached
    // texture. Called from TextureBaseClass::~TextureBaseClass before the
    // legacy texture is released, so bgfx's cache never holds a dangling
    // TextureBaseClass* that a later allocation could alias (ABA). The
    // backend must queue the handle for deferred destruction (in-flight
    // draws may still reference it) and erase its cache entries.
    virtual void Release_Cached_Texture(TextureBaseClass * /*texture*/) {}

    // TheSuperHackers @feature bobtista 08/06/2026 Letterbox present. When enabled, the backend
    // renders the game into a centered sub-rect of the window at the requested aspect and fills the
    // remainder with black bars; this gives a consistent viewable area regardless of window/display
    // aspect (used to keep multiplayer fair). When disabled, present is direct (content == window).
    // The default backend ignores it and always presents directly. The accessors report the current
    // content rect so the engine can match its display resolution and offset mouse input.
    virtual void Set_Present_Letterbox(bool /*enabled*/, float /*aspectW*/, float /*aspectH*/) {}
    virtual bool Is_Present_Letterbox_Active() const { return false; }
    virtual int  Get_Present_Content_Width() const { return 0; }
    virtual int  Get_Present_Content_Height() const { return 0; }
    virtual int  Get_Present_Offset_X() const { return 0; }
    virtual int  Get_Present_Offset_Y() const { return 0; }

    // -------------------------------------------------------------------------
    // Backend lifecycle
    // -------------------------------------------------------------------------
    //
    // TheSuperHackers @refactor bobtista 11/04/2026 Initialize is called
    // once after DX8Wrapper has finished its own device
    // setup, with the game's main HWND and current back-buffer dimensions.
    // DX8Backend treats it as a no-op (DX8Wrapper still owns the real device).
    // BgfxBackend uses it to call bgfx::init. Shutdown is the symmetric
    // teardown, called before DX8Wrapper releases its device.
    //
    // These are in addition to the existing per-scene Begin_Scene / End_Scene
    // pair, which are called every frame.

    virtual void Initialize(void * hwnd, int width, int height) {}
    virtual void Shutdown() {}

    // -------------------------------------------------------------------------
    // Render-system bring-up / tear-down (device enumeration layer)
    //
    // Distinct from Initialize/Shutdown above, which create and destroy the
    // per-window rendering context. These mirror the legacy DX8Wrapper
    // Init/Shutdown entry points that enumerate adapters and display modes.
    // -------------------------------------------------------------------------

    virtual bool Init_Render_System(void * hwnd, bool lite) { return false; }
    virtual void Shutdown_Render_System() {}

    // -------------------------------------------------------------------------
    // Device selection, windowing and display-mode control
    // -------------------------------------------------------------------------

    virtual bool Set_Render_Device(const char * dev_name, int width, int height, int bits, int windowed, bool resize_window) { return false; }
    virtual bool Set_Render_Device(int dev, int width, int height, int bits, int windowed, bool resize_window, bool reset_device, bool restore_assets) { return false; }
    virtual bool Set_Any_Render_Device() { return false; }
    virtual bool Set_Next_Render_Device() { return false; }
    virtual bool Toggle_Windowed() { return false; }
    virtual bool Is_Windowed() const { return false; }
    virtual int Get_Render_Device() const { return -1; }
    virtual const RenderDeviceDescClass & Get_Render_Device_Desc(int deviceidx) = 0;
    virtual int Get_Render_Device_Count() const { return 0; }
    virtual const char * Get_Render_Device_Name(int device_index) { return ""; }
    virtual bool Set_Device_Resolution(int width, int height, int bits, int windowed, bool resize_window) { return false; }
    virtual void Get_Render_Target_Resolution(int & set_w, int & set_h, int & set_bits, bool & set_windowed) {}
    virtual void Get_Device_Resolution(int & set_w, int & set_h, int & set_bits, bool & set_windowed) {}
    virtual int Get_Device_Resolution_Width() const { return 0; }
    virtual int Get_Device_Resolution_Height() const { return 0; }
    virtual bool Registry_Save_Render_Device(const char * sub_key) { return false; }
    virtual bool Registry_Save_Render_Device(const char * sub_key, int device, int width, int height, int depth, bool windowed, int texture_depth) { return false; }
    virtual bool Registry_Load_Render_Device(const char * sub_key, bool resize_window) { return false; }
    virtual bool Registry_Load_Render_Device(const char * sub_key, char * device, int device_len, int & width, int & height, int & depth, int & windowed, int & texture_depth) { return false; }

    // Present sync interval. 0 = no vsync, non-zero = sync to display refresh.
    virtual void Set_Swap_Interval(int swap) {}
    virtual int Get_Swap_Interval() const { return 0; }

    // -------------------------------------------------------------------------
    // Device state queries
    // -------------------------------------------------------------------------

    virtual bool Is_Device_Lost() const { return false; }
    virtual RenderBackendDeviceStatus Get_Device_Status() const { return RB_DEVICE_OK; }
    virtual void Reset_Device() {}
    virtual void Set_Device_Cleanup_Hook(RenderDeviceCleanupHook * hook) {}
    virtual bool Has_Stencil() const { return false; }
    virtual WW3DFormat Get_Back_Buffer_Format() const { return WW3D_FORMAT_UNKNOWN; }
    virtual bool Get_Back_Buffer_Description(unsigned int num, RenderBackendSurfaceDescription & desc) const { desc = RenderBackendSurfaceDescription(); return false; }
    virtual bool Capture_Back_Buffer_Image(unsigned int num, RenderBackendImage & image) { return false; }

    // TheSuperHackers @bugfix bobtista 03/06/2026 GPU-direct backbuffer copy
    // to a texture's level-0 surface. Used by W3DSmudge to keep a persistent
    // background snapshot without allocating per-frame system-memory surfaces
    // (the previous Capture_Back_Buffer_Image route on dx8 was leaking ~4 MB
    // per call and exhausting the 2 GB virtual address space). DX8 backend
    // does a CopyRects from back buffer to the texture's POOL_DEFAULT surface;
    // bgfx can blit from its scene RT. Default returns false so callers
    // know to fall back.
    virtual bool Copy_Back_Buffer_To_Texture(unsigned int /*num*/, TextureClass * /*dst_texture*/) { return false; }
    // TheSuperHackers @feature bobtista 09/07/2026 A backend with a native screenshot path encodes and
    // writes the file itself (the path extension selects the format); callers should prefer it over the
    // CPU back-buffer readback. Returns false on backends without one (DX8 reference build).
    virtual bool Supports_Native_Screen_Shot() const { return false; }
    virtual bool Request_Native_Screen_Shot(const char * /*path*/) { return false; }
    virtual void Set_Texture_Bitdepth(int bitdepth) {}
    virtual int Get_Texture_Bitdepth() const { return 16; }
    virtual bool Supports_Texture_Format(WW3DFormat format) const { return false; }
    virtual bool Supports_Compressed_Textures() const { return false; }
    virtual bool Supports_Bump_Envmap() const { return false; }
    virtual bool Supports_Bump_Envmap_Luminance() const { return false; }
    virtual bool Supports_Texture_Filter(RenderBackendTextureFilterCapability /*capability*/) const { return false; }
    virtual bool Supports_Texture_Op(RenderBackendTextureOpCapability /*capability*/) const { return false; }
    virtual bool Supports_Fog() const { return false; }
    virtual bool Is_Legacy_Voodoo3() const { return false; }
    virtual bool Supports_NPatches() const { return false; }
    virtual bool Supports_Hardware_Transform_And_Lighting() const { return false; }
    virtual bool Supports_Point_Sprites() const { return false; }
    virtual RenderBackendTextureLimits Get_Texture_Limits() const
    {
        return { 2048, 2048, 2048, 8 };
    }
    virtual int Get_Max_Texture_Stages() const { return RB_MAX_TEXTURE_STAGES; }
    virtual bool Supports_Z_Bias() const { return false; }
    virtual void Set_MSAA_Mode(RenderBackendMSAAMode mode) {}
    virtual RenderBackendMSAAMode Get_MSAA_Mode() const { return RB_MSAA_NONE; }
    virtual bool Supports_Dot3() const { return false; }
    virtual bool Get_Device_Identity(RenderBackendDeviceIdentity & identity) const { return false; }
    virtual void Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit) {}

    // -------------------------------------------------------------------------
    // Frame lifecycle
    // -------------------------------------------------------------------------

    virtual void Begin_Scene() {}
    virtual void End_Scene(bool flip_frame) {}
    virtual void Flip_To_Primary() {}
    virtual void Begin_Device_Statistics() {}
    virtual void End_Device_Statistics() {}
    // Defaults match DX8Wrapper::Clear so existing call sites that supplied
    // only the first 3-4 arguments compile unchanged after migration.
    virtual void Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha = 0.0f, float z = 1.0f, unsigned int stencil = 0) {}
    virtual void Set_Viewport(const RenderBackendViewport & viewport) {}

    // -------------------------------------------------------------------------
    // View capture / post-effect primitives
    // -------------------------------------------------------------------------
    //
    // High-level replacement for W3DShaderManager's old raw D3D render-target
    // ownership. Callers express that they want to capture and later sample
    // the tactical view; each backend decides whether that is a D3D texture,
    // a bgfx framebuffer, or unsupported.
    virtual bool Initialize_View_Capture(RenderBackendViewCaptureKind /*kind*/) { return false; }
    virtual void Release_View_Capture(RenderBackendViewCaptureKind /*kind*/) {}
    virtual bool Supports_View_Capture(RenderBackendViewCaptureKind /*kind*/) const { return false; }
    virtual bool Begin_View_Capture(RenderBackendViewCaptureKind /*kind*/) { return false; }
    virtual bool End_View_Capture(RenderBackendViewCaptureKind /*kind*/) { return false; }
    virtual bool Is_View_Capture_Active(RenderBackendViewCaptureKind /*kind*/) const { return false; }
    virtual bool Has_View_Capture(RenderBackendViewCaptureKind /*kind*/) const { return false; }
    virtual bool Bind_View_Capture_Texture(RenderBackendViewCaptureKind /*kind*/,
                                           unsigned int /*stage*/) { return false; }
    virtual bool Draw_View_Capture_Quad(RenderBackendViewCaptureKind /*kind*/,
                                        const RenderBackendScreenVertex * /*vertices*/,
                                        unsigned int /*vertex_count*/,
                                        bool /*use_second_uv*/) { return false; }
    virtual bool Draw_Screen_Quad(const RenderBackendScreenVertex * /*vertices*/,
                                  unsigned int /*vertex_count*/,
                                  bool /*use_second_uv*/) { return false; }

    virtual bool Capture_Back_Buffer_RGBA(unsigned int /*display_width*/,
                                          unsigned int /*display_height*/,
                                          unsigned int /*image_size*/,
                                          unsigned char * /*output_pixels*/,
                                          unsigned int /*output_capacity*/,
                                          unsigned int * /*output_width*/,
                                          unsigned int * /*output_height*/) { return false; }

    // -------------------------------------------------------------------------
    // Vertex / index buffers
    // -------------------------------------------------------------------------

    virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream = 0) {}
    virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba) {}
    virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset) {}
    virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset) {}
    virtual void Set_Index_Buffer_Index_Offset(unsigned int offset) {}

    // TheSuperHackers @refactor bobtista 11/04/2026 Write-side
    // upload hooks. The W3D engine writes vertex/index data through
    // VertexBufferClass::WriteLockClass / IndexBufferClass::WriteLockClass
    // (and the various Copy() helpers). At unlock time the data is sitting
    // in a CPU-mapped pointer that the engine just wrote into - that is
    // the safe moment for the bgfx backend to upload a copy and create its
    // own GPU buffer. The DX8 backend ignores these calls; only BgfxBackend
    // uses them. Default empty implementations so existing call sites that
    // do not need them are not forced to override.
    virtual void Upload_Vertex_Buffer_Data(const VertexBufferClass * /*vb*/,
                                     const void * /*data*/,
                                     unsigned int /*size_bytes*/) {}
    virtual void Upload_Index_Buffer_Data(const IndexBufferClass * /*ib*/,
                                    const void * /*data*/,
                                    unsigned int /*size_bytes*/) {}

    // TheSuperHackers @refactor bobtista 11/04/2026 Dynamic
    // upload hooks. Same pattern as above but for DynamicVBAccessClass /
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

    virtual bool Supports_Instancing() const { return false; }
    virtual bool Begin_Instanced_Batch(unsigned max_instances) { return false; }
    virtual void Add_Instance(const float * world_matrix_4x4) {}
    virtual void Submit_Instanced_Batch(unsigned index_offset, unsigned triangle_count,
                                        unsigned min_vertex_index, unsigned vertex_count) {}

    virtual void * Begin_Dynamic_Vertex_Write(const DynamicVBAccessClass * /*vba*/,
                                              unsigned int /*size_bytes*/) { return nullptr; }
    virtual void End_Dynamic_Vertex_Write(const DynamicVBAccessClass * /*vba*/,
                                          const void * /*data*/,
                                          unsigned int /*size_bytes*/) {}
    virtual void * Begin_Dynamic_Index_Write(const DynamicIBAccessClass * /*iba*/,
                                             unsigned int /*size_bytes*/) { return nullptr; }
    virtual void End_Dynamic_Index_Write(const DynamicIBAccessClass * /*iba*/,
                                         const void * /*data*/,
                                         unsigned int /*size_bytes*/) {}

    // TheSuperHackers @refactor bobtista 11/04/2026 Sub-range
    // upload. Rigid mesh category containers fill their shared VB / IB
    // via AppendLockClass one sub-range at a time. BgfxBackend creates a
    // bgfx dynamic buffer the first time it sees a VB / IB and updates
    // the sub-range in place. start_vertex / start_index is in elements
    // (verts or shorts), size_bytes is in bytes.
    virtual void Upload_Vertex_Buffer_Sub_Range(const VertexBufferClass * /*vb*/,
                                          const void * /*data*/,
                                          unsigned int /*start_vertex*/,
                                          unsigned int /*size_bytes*/) {}
    virtual void Upload_Index_Buffer_Sub_Range(const IndexBufferClass * /*ib*/,
                                         const void * /*data*/,
                                         unsigned int /*start_index*/,
                                         unsigned int /*size_bytes*/) {}

    // TheSuperHackers @refactor bobtista 11/04/2026 Sorted
    // draw pass routing. SortingRendererClass::Flush_Sorting_Pool wraps
    // its per-batch draw loop in Begin/End_Sorted_Batch_Pass and applies one
    // RenderBackendSortedBatchState per batch. BgfxBackend uses this to route
    // the sorted submits to a dedicated bgfx view id so per-batch matrices
    // cannot stomp the opaque view. Empty defaults = no-op on DX8Backend.
    virtual void Begin_Sorted_Batch_Pass() {}
    virtual void End_Sorted_Batch_Pass() {}
    virtual void Apply_Sorted_Batch_State(const RenderBackendSortedBatchState & /*state*/) {}
    virtual void Set_Point_Group_Render_Active(bool /*active*/) {}
    virtual void Set_Streak_Render_Active(bool /*active*/) {}
    // TheSuperHackers @feature bobtista 07/07/2026 Marks sorted-pool inserts that originate
    // from a mesh polygon renderer. Mesh geometry is authored in model space and needs its
    // live per-mesh world captured for the sorted replay; point groups/streaks author their
    // vertices in world space and must not carry one. Empty default = no-op on DX8Backend.
    virtual void Set_Mesh_Render_Active(bool /*active*/) {}
    virtual void Capture_Legacy_Render_State_For_Sorted_Draw(RenderStateStruct & /*state*/) {}
    virtual void Restore_Legacy_Render_State_For_Sorted_Draw(const RenderStateStruct & /*state*/) {}
    virtual void Release_Legacy_Render_State_For_Sorted_Draw() {}
    // TheSuperHackers @performance bobtista 08/07/2026 Sorted texture-array merge support.
    // The backend resolves a node's texture2DArray slot during
    // Capture_Legacy_Render_State_For_Sorted_Draw (when the live translated draw state is
    // authoritative) and carries it on the RenderStateStruct sorted_array_* fields.
    // Set_Sorted_Texture_Array_Page(-1 to clear) marks the next sorted submit as a merged
    // run that spans several stage-0 textures via a per-vertex layer index. The empty
    // default keeps DX8Backend on the classic per-texture run splits.
    virtual void Set_Sorted_Texture_Array_Page(int /*page*/) {}

    // TheSuperHackers @refactor bobtista 10/07/2026 Single-call sorted-pool run submit.
    // Applies the captured batch packet and issues the indexed draw in one backend
    // entry, replacing the per-run Apply_Sorted_Batch_State + Draw_Triangles
    // round-trip on backends with a shader pipeline. start_index is in index units
    // (triangle start * 3), matching Draw_Triangles. Returns false when the backend
    // does not implement the packet path; the caller must then run the legacy
    // apply/draw sequence. Default = not implemented (DX8Backend).
    virtual bool Submit_Sorted_Packet(const RenderBackendSortedBatchState & /*packet*/,
                                      unsigned int /*start_index*/,
                                      unsigned int /*polygon_count*/,
                                      unsigned int /*vertex_count*/,
                                      int /*array_page*/) { return false; }

    // TheSuperHackers @refactor bobtista 11/07/2026 Single-call rigid mesh draw:
    // the polygon renderer hands the complete indexed draw (index-buffer base
    // offset plus ranges) to the backend in one entry instead of the legacy
    // Set_Index_Buffer_Index_Offset + Draw_Triangles/Draw_Strip pair. Returns
    // false when not implemented (DX8Backend); the caller then runs the legacy
    // sequence.
    virtual bool Submit_Rigid_Packet(int /*ib_base_offset*/,
                                     unsigned int /*start_index*/,
                                     unsigned int /*primitive_count*/,
                                     unsigned int /*min_vertex_index*/,
                                     unsigned int /*vertex_count*/,
                                     bool /*triangle_strip*/) { return false; }

    // TheSuperHackers @refactor bobtista 11/04/2026 Lets Draw_Sorting_IB_VB hand its inner
    // dynamic VB/IB to the backend so bgfx can claim their transient buffers and submit a
    // remapped draw, skipping the outer stale Draw_Triangles. No-op on DX8Backend.
    virtual void Submit_Sorted_Draw(const DynamicVBAccessClass & /*dyn_vb*/,
                                    const DynamicIBAccessClass & /*dyn_ib*/,
                                    unsigned short /*polygon_count*/,
                                    unsigned short /*vertex_count*/) {}

    // -------------------------------------------------------------------------
    // State: shaders, materials, textures
    // -------------------------------------------------------------------------

    virtual void Set_Shader(const ShaderClass & shader) {}
    virtual void Get_Shader(ShaderClass & shader) {}
    virtual void Set_Material(const VertexMaterialClass * material) {}
    virtual void Apply_Material_State(const RenderBackendMaterialState & material) {}
    virtual void Set_Material_Color_Source(RenderBackendMaterialColorSource ambient_source,
                                           RenderBackendMaterialColorSource diffuse_source,
                                           RenderBackendMaterialColorSource emissive_source) {}
    virtual void Set_Texture(unsigned int stage, TextureBaseClass * texture) {}
    // Immediate texture-stage bind for legacy custom passes that do not call
    // Apply_Render_State_Changes between pass setup and draw. DX8 binds the
    // native stage immediately; bgfx captures the same stage in its draw state.
    virtual void Bind_Texture_Immediate(unsigned int stage, TextureBaseClass * texture) { Set_Texture(stage, texture); }

    // TheSuperHackers @feature bobtista 01/06/2026 Backend-neutral CPU -> GPU
    // texture region upload. The DX8 backend implements this with a
    // POOL_SYSTEMMEM staging surface and IDirect3DDevice8::CopyRects -- the
    // only legal POOL_SYSTEMMEM -> POOL_DEFAULT transport in DX8. Callers
    // such as W3DShroud that previously talked to D3D8 directly via
    // DX8Wrapper::_Copy_DX8_Rects route through here instead.
    virtual void Upload_Texture_Region(
        TextureClass * dst_texture,
        unsigned int dst_level,
        unsigned int dst_x, unsigned int dst_y,
        const void * src_data,
        unsigned int src_pitch,
        unsigned int region_width, unsigned int region_height,
        WW3DFormat format) {}

    virtual void Apply_Render_State_Changes() {}
    virtual void Apply_Default_State() {}
    virtual void Invalidate_Cached_Render_States() {}

    // TheSuperHackers @refactor bobtista 10/04/2026 Typed blend +
    // color-write setters. These exist so subsystems can migrate without the
    // interface re-exposing raw D3D render-state identifiers.
    virtual void Set_Blend_Op(BlendOp op) {}
    virtual void Set_Blend_Factors(BlendFactor src, BlendFactor dest) {}
    virtual void Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha) {}
    // TheSuperHackers @refactor bobtista 10/04/2026 Natural complement
    // to theblend extension.
    virtual void Set_Alpha_Blend_Enable(bool enable) {}
    virtual void Set_Alpha_Test_Enable(bool enable) {}
    virtual void Set_Alpha_Test_Reference(unsigned ref) {}
    virtual void Set_Alpha_Test_Function(CompareFunc func) {}
    virtual void Set_Alpha_Test(bool enable, unsigned ref, CompareFunc func)
    {
        Set_Alpha_Test_Reference(ref);
        Set_Alpha_Test_Function(func);
        Set_Alpha_Test_Enable(enable);
    }
    virtual void Set_Normalize_Normals(bool enable) {}

    // TheSuperHackers @refactor bobtista 10/04/2026 Hardware cursor
    // extension. Lets W3DMouse drive the device's hardware cursor without
    // touching the raw device directly.
    virtual void Show_Hardware_Cursor(bool show) {}
    virtual void Set_Hardware_Cursor_Image(int hotspot_x, int hotspot_y, const RenderBackendImage & image) {}
    virtual void Set_Hardware_Cursor_Position(int x, int y) {}

    // TheSuperHackers @refactor bobtista 10/04/2026 Stencil state
    // group. Each method maps 1:1 onto an existing legacy stencil state. The
    // CompareFunc and StencilOp enums above are reusable for future depth
    // and stencil work.
    virtual void Set_Stencil_Enable(bool enable) {}
    virtual void Set_Stencil_Func(CompareFunc func) {}
    virtual void Set_Stencil_Ref(unsigned int ref) {}
    virtual void Set_Stencil_Mask(unsigned int mask) {}
    virtual void Set_Stencil_Write_Mask(unsigned int mask) {}
    virtual void Set_Stencil_Pass_Op(StencilOp op) {}
    virtual void Set_Stencil_Fail_Op(StencilOp op) {}
    virtual void Set_Stencil_ZFail_Op(StencilOp op) {}

    // TheSuperHackers @refactor bobtista 14/04/2026 Render-state remainders.
    // These wrap the last legacy render-state values still
    // being set directly by the terrain / scene / water / snow code
    // (ZBIAS, FILLMODE, ZENABLE/ZFUNC, COLORWRITEENABLE as DWORD mask).
    // The DWORD variant of Set_Color_Write_Mask coexists with the
    // boolean Set_Color_Write_Enable — callers that receive a saved
    // bitmask from GetRenderState use this, callers that know the four
    // channel flags use the boolean form.
    virtual void Set_Z_Bias(int bias) {}
    virtual void Set_Normal_Bias(float bias) {}
    virtual void Set_Fill_Mode(FillMode mode) {}
    virtual void Set_Shade_Mode(ShadeMode mode) {}
    virtual void Set_Depth_Test_Enable(bool enable) {}
    virtual void Set_Depth_Write_Enable(bool enable) {}
    virtual void Set_Depth_Func(CompareFunc func) {}
    virtual bool Supports_Color_Write_Mask() const { return true; }
    virtual unsigned Get_Color_Write_Mask() const { return RB_COLOR_RGBA; }
    virtual void Set_Color_Write_Mask(unsigned mask) {}
    virtual void Set_Lighting_Enable(bool enable) {}
    virtual void Set_Point_Sprite_Enable(bool enable) {}
    virtual void Set_Point_Scale_Enable(bool enable) {}
    virtual void Set_Point_Size(float size, float min_size, float max_size) {}
    virtual void Set_Point_Scale(float a, float b, float c) {}
    virtual void Set_Texture_Factor(unsigned argb) {}
    virtual CullMode Get_Cull_Mode() const { return RB_CULL_CW; }
    virtual void Set_Cull_Mode(CullMode mode) {}

    // TheSuperHackers @refactor bobtista 14/04/2026 Tree /
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

    // TheSuperHackers @feature bobtista 20/04/2026 Grayscale
    // output for disabled 2D UI elements (Render2DClass::Enable_Grayscale).
    // DX8 backend programs the DOTPRODUCT3/MODULATE TSS cascade for the
    // legacy path. bgfx backend drives a luminance-conversion uniform in
    // fs_uber and does not need texture-stage setup.
    virtual void Set_Grayscale_Mode(bool enable) {}
    virtual void Configure_Grayscale_Texture_Stages() {}
    virtual void Configure_Custom_Edging_Cloud_Texture_Stages() {}
    virtual void Configure_Shadow_Volume_Fill_Texture_Stages() {}

    // TheSuperHackers @feature bobtista 20/04/2026 Cloud-shadow
    // modulation state. Engine calls this per frame to hand over the
    // scrolling cloud offset (in world-XY units that match the
    // TerrainShader2Stage::m_xOffset/m_yOffset domain) + the stretch
    // factor (= 1 / (63 * MAP_XY_FACTOR / 2) on DX8). DX8 backend
    // default no-op: DX8 still drives its own TSS-based cloud pass.
    // bgfx backend wires these into u_cloudParams/s_cloudMap which
    // fs_uber multiplies into the final terrain color.
    virtual void Set_Cloud_Shadow_Params(bool enable, float scroll_x, float scroll_y,
                                         float stretch, TextureClass * cloud_tex) {}
    virtual void Set_Light_Map_Params(bool /*enable*/, float /*stretch*/, TextureClass * /*noise_tex*/) {}

    // -------------------------------------------------------------------------
    // Transforms
    // -------------------------------------------------------------------------

    virtual void Set_Transform(TransformKind transform, const Matrix4x4 & m) {}
    virtual void Set_Transform(TransformKind transform, const Matrix3D & m) {}
    virtual void Get_Transform(TransformKind transform, Matrix4x4 & m) const {}
    virtual void Set_World_Identity() {}
    virtual void Set_View_Identity() {}
    virtual bool Is_World_Identity() const { return false; }
    virtual bool Is_View_Identity() const { return false; }
    virtual void Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix,
                                                      float znear, float zfar) {}

    // -------------------------------------------------------------------------
    // Lighting and fog
    // -------------------------------------------------------------------------

    virtual void Set_Light(unsigned int index, const LightClass & light) {}
    virtual void Clear_Light(unsigned int index) {}
    virtual void Set_Ambient(const Vector3 & color) {}
    // Returns a reference — no sensible default without the Vector3 constructor.
    // Standalone backends must override; ref-popup DX8Backend provides the real value.
    virtual const Vector3 & Get_Ambient() const = 0;
    virtual void Set_Fog(bool enable, const Vector3 & color, float start, float end) {}
    virtual void Set_Fog_Enable(bool enable) {}
    virtual void Set_Fog_Color(unsigned argb) {}
    virtual unsigned Get_Fog_Color() const { return 0; }
    virtual bool Get_Fog_Enable() const { return false; }
    virtual void Set_Light_Environment(LightEnvironmentClass * light_env) {}
    virtual LightEnvironmentClass * Get_Light_Environment() const { return nullptr; }
    virtual void Set_Specular_Enable(bool enable) {}
    virtual void Set_Patch_Segments(float level) {}

    // Post-ShaderClass render state overrides. The terrain edge blending
    // and other systems set D3D blend/alpha-test state AFTER ShaderClass
    // applies. These methods let the bgfx backend capture the overrides.
    // Empty defaults = forward to DX8Wrapper only (DX8Backend behavior).
    virtual void Override_Blend(BlendFactor srcBlend, BlendFactor dstBlend) {}
    virtual void Override_Alpha_Test(bool enable, unsigned ref, CompareFunc func) {}
    virtual void Override_Alpha_Blend_Enable(bool enable) {}
    virtual void Override_Texcoord_Index(unsigned stage, unsigned uvIndex) {}
    virtual void Override_Terrain_Blend(bool enable) {}
    virtual void Override_Material_Opacity(float opacity) {}
    virtual void Set_Texture_Transform(unsigned stage, const Matrix4x4& matrix) {}
    virtual void Clear_Texture_Transform(unsigned stage) {}
    virtual void Set_Texture_Coord_Source(unsigned stage,
                                          RenderBackendTexcoordSource source,
                                          unsigned uv_array_index = 0) {}
    virtual void Set_Texture_Transform_Mode(unsigned stage, unsigned coord_count, bool projected) {}
    virtual void Set_Texture_Bump_Env_Matrix(unsigned stage,
                                             float m00,
                                             float m01,
                                             float m10,
                                             float m11) {}
    virtual void Set_Texture_Bump_Env_Luminance(unsigned stage,
                                                float scale,
                                                float offset) {}
    virtual void Set_Texture_Color_Operation(unsigned stage,
                                             RenderBackendTextureOperation op) {}
    virtual void Set_Texture_Alpha_Operation(unsigned stage,
                                             RenderBackendTextureOperation op) {}
    virtual void Set_Texture_Color_Argument(unsigned stage,
                                            unsigned argument_index,
                                            RenderBackendTextureArgument arg) {}
    virtual void Set_Texture_Alpha_Argument(unsigned stage,
                                            unsigned argument_index,
                                            RenderBackendTextureArgument arg) {}
    virtual void Set_Texture_Coord_Generation(unsigned stage, bool cameraPosEnabled)
    {
        Set_Texture_Coord_Source(stage,
                                 cameraPosEnabled ? RB_TEXCOORD_CAMERA_SPACE_POSITION : RB_TEXCOORD_MESH_UV,
                                 stage);
    }
    virtual void Set_Texture_UV_Wrap(unsigned stage, bool enable) {}
    virtual void Set_Texture_Address_Mode(unsigned stage,
                                          RenderBackendTextureAddressMode u,
                                          RenderBackendTextureAddressMode v,
                                          RenderBackendTextureAddressMode w) {}
    virtual void Set_Texture_Sample_Filter(unsigned stage,
                                           RenderBackendTextureSampleFilter min_filter,
                                           RenderBackendTextureSampleFilter mag_filter,
                                           RenderBackendTextureSampleFilter mip_filter) {}
    virtual void Set_Texture_Min_Mag_Filter(unsigned stage,
                                            RenderBackendTextureSampleFilter min_filter,
                                            RenderBackendTextureSampleFilter mag_filter) {}
    virtual void Set_Texture_Mip_Filter(unsigned stage,
                                        RenderBackendTextureSampleFilter mip_filter) {}
    virtual void Set_Texture_Max_Anisotropy(unsigned stage, unsigned max_anisotropy) {}
    virtual void Set_Texture_Clamp_Mode(unsigned stage, bool clampU, bool clampV) {}
    virtual void Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value) {}
    virtual void Set_Shroud_Texture_Pass_Active(bool active, unsigned stage) {}
    virtual void Set_Object_Shroud_Texture_Pass_Active(bool active) {}
    virtual void Set_Object_Shroud_Alpha_Mask_Texture(TextureBaseClass * texture) {}
    virtual void Set_Shroud_Texture_Params(float offset_x, float offset_y,
                                           float scale_x, float scale_y) {}
    // Some backends need object shroud material passes delayed until after all
    // opaque object base draws so the multiplicative pass cannot be overwritten
    // by another FVF container's later base pass.
    virtual bool Requires_Delayed_Object_Shroud_Pass() const { return false; }
    virtual void Begin_Water_Overlay() {}
    virtual void End_Water_Overlay() {}
    // Route subsequent draws to the sort view instead of the opaque view.
    // Used by dazzle/lens-flare effects that need to render on top of water.
    virtual void Begin_Effect_Overlay() {}
    virtual void End_Effect_Overlay() {}
    virtual bool Begin_Smudge_Distortion(float tactical_width_fraction = 1.0f,
                                         float tactical_height_fraction = 1.0f) { return false; }
    virtual void End_Smudge_Distortion() {}
    virtual void Clear_State_Overrides() {}

    // -------------------------------------------------------------------------
    // Draw calls
    // -------------------------------------------------------------------------

    virtual void Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) {}

    virtual void Draw_Triangles(unsigned int buffer_type,
                                unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) {}

    virtual bool Is_Triangle_Draw_Enabled() const { return true; }
    virtual void Set_Triangle_Draw_Enabled(bool /*enable*/) {}
    virtual void Draw_Screen_Color_Quad(unsigned /*color*/,
                                        int /*x*/,
                                        int /*y*/,
                                        int /*width*/,
                                        int /*height*/) {}
    virtual void Draw_Screen_Multiply_Quad(unsigned /*color*/,
                                           int /*x*/,
                                           int /*y*/,
                                           int /*width*/,
                                           int /*height*/) {}

    virtual void Draw_Strip(unsigned short start_index,
                            unsigned short index_count,
                            unsigned short min_vertex_index,
                            unsigned short vertex_count) {}

    // TheSuperHackers @refactor bobtista 15/04/2026 Lets the
    // caller request that the very next Draw_Triangles dispatches only
    // to the DX8 reference path, skipping the bgfx submit. Used by
    // stencil shadow volume passes that have no bgfx shader/state yet
    // and would otherwise render as garbage geometry on the bgfx view.
    // No-op in non-bgfx backends.
    virtual void Skip_Next_Bgfx_Submit() {}

    // TheSuperHackers @refactor bobtista 07/05/2026 Marks the W3D projected
    // shadow decal flush. bgfx uses this pass provenance to distinguish real
    // blob shadows from effect draws that may bind the same texture names.
    // No-op in non-bgfx backends.
    virtual void Set_Projected_Shadow_Decal_Active(bool /*active*/) {}
    virtual void Set_Projected_Decal_Mode(RenderBackendProjectedDecalMode /*mode*/) {}

    // TheSuperHackers @refactor bobtista 15/04/2026 Toggles the
    // stencil shadow volume program + state override. When active, bgfx
    // submits shadow extrusion geometry using vs_shadow_volume/
    // fs_shadow_volume with color writes disabled and the engine's
    // stencil state mapped into bgfx state bits. No-op in non-bgfx
    // backends. The caller sets active before the shadow volume draw
    // and clears it immediately after.
    virtual void Set_Shadow_Volume_Shader_Active(bool /*active*/) {}

    // TheSuperHackers @refactor bobtista 15/04/2026 Fullscreen
    // shadow darkening pass. Draws a screen-space quad with stencil
    // test ref<=stencil && (stencil & read_mask) and DEST_COLOR*SRC
    // blend so stenciled pixels multiply against the shadow color.
    // shadow_color is ARGB like the engine's getShadowColor() return.
    virtual void Apply_Stencil_Shadow_Darken(unsigned /*shadow_color*/,
                                             unsigned /*stencil_read_mask*/,
                                             unsigned /*stencil_ref*/,
                                             int /*x*/,
                                             int /*y*/,
                                             int /*width*/,
                                             int /*height*/) {}

    // TheSuperHackers @refactor bobtista 15/04/2026 Called after the side-wall draw to submit
    // front+back caps so bgfx's stencil algorithm sees a closed volume. Silhouette verts sit
    // at strip_start_vertex + 2*i (caster) and + 2*i + 1 (extruded). No-op on DX8Backend.
    virtual void Submit_Shadow_Volume_Caps(unsigned /*strip_start_vertex*/,
                                           unsigned /*num_silhouette_verts*/) {}

    // TheSuperHackers @refactor bobtista 15/04/2026 Caller-triangulated caps: cap_indices holds
    // local silhouette indices (3 per triangle); the backend maps front caps to
    // strip_start_vertex + 2*i and back caps to + 2*i + 1 with reversed winding. No-op on DX8.
    virtual void Submit_Shadow_Volume_Triangulated_Caps(
        unsigned /*strip_start_vertex*/,
        const short * /*local_cap_indices*/,
        unsigned /*cap_index_count*/) {}

    // DX8's original stencil-volume path used open side-wall tubes. Modern
    // backends need closed volumes because near-plane clipping otherwise
    // leaves unbalanced stencil counts. Default false keeps DX8 unchanged.
    virtual bool Needs_Closed_Shadow_Volumes() const { return false; }

    // TheSuperHackers @feature bobtista 17/04/2026 Push shroud system-memory pixels to the
    // backend (bgfx cannot lock the POOL_DEFAULT destination). border_pixel fills texels
    // outside the source rect; off-map terrain samples that border ring. No-op on DX8Backend.
    virtual void Capture_Shroud_Texture(TextureClass * /*dst_texture*/,
                                        const void * /*pixel_data*/,
                                        unsigned /*dst_width*/,
                                        unsigned /*dst_height*/,
                                        unsigned /*src_width*/,
                                        unsigned /*src_height*/,
                                        unsigned /*src_x*/,
                                        unsigned /*src_y*/,
                                        unsigned /*dst_x*/,
                                        unsigned /*dst_y*/,
                                        unsigned /*pitch*/,
                                        WW3DFormat /*format*/,
                                        unsigned /*border_pixel*/ = 0xFFFFu) {}

    // -------------------------------------------------------------------------
    // Programmable pipeline (GPU vertex / pixel shaders)
    // -------------------------------------------------------------------------
    //
    // These correspond to DX8's programmable shader slots. Modern backends
    // will re-interpret the handles internally; the interface treats the
    // shader id as an opaque unsigned long.

    // Legacy shader-object lifetime. File-backed shaders may be
    // handled directly by a backend before the caller loads bytecode; bgfx
    // uses this to provide compatibility handles for native shader paths
    // without requiring obsolete .vso/.pso bytecode files.
    virtual const unsigned int * Get_Legacy_Vertex_Shader_Declaration(
        RenderBackendLegacyVertexDeclaration /*declaration*/) const { return nullptr; }
    virtual bool Load_Legacy_Shader(const char * /*path*/,
                                    const unsigned int * /*declaration*/,
                                    unsigned int /*usage*/,
                                    RenderBackendShaderKind /*kind*/,
                                    unsigned long * /*handle*/) { return false; }
    virtual bool Create_Vertex_Shader(const unsigned int * /*declaration*/,
                                      const unsigned int * /*shader*/,
                                      unsigned int /*usage*/,
                                      unsigned long * /*handle*/) { return false; }
    virtual bool Create_Pixel_Shader(const unsigned int * /*shader*/,
                                     unsigned long * /*handle*/) { return false; }
    virtual bool Create_Legacy_Pixel_Shader(RenderBackendLegacyPixelShaderMode /*mode*/,
                                            unsigned long * /*handle*/) { return false; }
    virtual void Delete_Vertex_Shader(unsigned long /*vertex_shader*/) {}
    virtual void Delete_Pixel_Shader(unsigned long /*pixel_shader*/) {}
    virtual void Set_Vertex_Shader(unsigned long vertex_shader) {}
    virtual void Set_Pixel_Shader(unsigned long pixel_shader) {}
    virtual void Set_Vertex_Shader_Constant(int reg, const void * data, int count) {}
    virtual void Set_Pixel_Shader_Constant(int reg, const void * data, int count) {}

    // -------------------------------------------------------------------------
    // Render targets
    // -------------------------------------------------------------------------

    virtual TextureClass * Create_Render_Target(int width, int height, WW3DFormat format = WW3D_FORMAT_UNKNOWN) { return nullptr; }
    virtual void Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture = nullptr) {}
    virtual bool Is_Render_To_Texture() const { return false; }
    virtual void Set_Shadow_Map(int idx, ZTextureClass * ztex) {}
    virtual ZTextureClass * Get_Shadow_Map(int idx) const { return nullptr; }

    // -------------------------------------------------------------------------
    // Resource creation (asset ingress)
    // -------------------------------------------------------------------------
    //
    // These methods let asset loaders produce CPU-side pixel / vertex / index
    // data and hand it to whichever backend is active. Each backend creates
    // its native GPU resource from the bytes and returns an opaque
    // RenderResource handle.
    //
    // W3D asset wrapper classes (TextureBaseClass, VertexBufferClass,
    // IndexBufferClass) store the returned handle beside their existing
    // raw legacy resource field; on DX8 builds that field still drives
    // rendering, on bgfx builds the handle returned here is authoritative.

    virtual bool Requires_Legacy_Buffer_Resources() const { return true; }
    virtual RenderResource Create_Texture(const TextureDesc & desc) { return kInvalidRenderResource; }
    virtual RenderResource Create_Vertex_Buffer(const BufferDesc & desc,
                                                const void *       initial_data) { return kInvalidRenderResource; }
    virtual RenderResource Create_Index_Buffer(const BufferDesc & desc,
                                               const void *       initial_data,
                                               bool               indices_are_32bit) { return kInvalidRenderResource; }
    virtual void Destroy_Resource(RenderResource h) {}

    // Transitional owner-backed resource hooks: populate m_backendHandle at the end of
    // wrapper construction. Default returns the invalid handle.

    virtual RenderResource Register_Texture_Resource(TextureBaseClass * /*tex*/) { return kInvalidRenderResource; }
    virtual RenderResource Register_Vertex_Buffer_Resource(VertexBufferClass * /*vb*/) { return kInvalidRenderResource; }
    virtual RenderResource Register_Index_Buffer_Resource(IndexBufferClass * /*ib*/) { return kInvalidRenderResource; }
};
