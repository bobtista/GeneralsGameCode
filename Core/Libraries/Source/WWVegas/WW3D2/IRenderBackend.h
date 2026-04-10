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
    virtual void Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha, float z, unsigned int stencil) = 0;
    virtual void Set_Viewport(const RenderBackendViewport & viewport) = 0;

    // -------------------------------------------------------------------------
    // Vertex / index buffers
    // -------------------------------------------------------------------------

    virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream) = 0;
    virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba) = 0;
    virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset) = 0;
    virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset) = 0;
    virtual void Set_Index_Buffer_Index_Offset(unsigned int offset) = 0;

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

    virtual TextureClass * Create_Render_Target(int width, int height, WW3DFormat format) = 0;
    virtual void Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture) = 0;
    virtual bool Is_Render_To_Texture() = 0;
    virtual void Set_Shadow_Map(int idx, ZTextureClass * ztex) = 0;
    virtual ZTextureClass * Get_Shadow_Map(int idx) = 0;
};
