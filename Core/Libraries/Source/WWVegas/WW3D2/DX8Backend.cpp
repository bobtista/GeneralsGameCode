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

// TheSuperHackers @refactor bobtista 10/04/2026 DX8Backend forwarding adapter.
// Every method in this file is a one-line trampoline to the existing
// DX8Wrapper static API. Keep it that way — if behavior needs to change it
// should change in DX8Wrapper, not here.

#include "DX8Backend.h"

#include "dx8fvf.h"
#include "texturecompatibilityinterop.h"
#include "dx8wrapper.h"
#include "DrawCallLog.h"
#include "RenderDocTrigger.h"
#include "dx8formatconv.h"
#include "FixedFunctionState.h"
#include "vector3.h"
#include "matrix4.h"
#include "matrix3d.h"
#include "light.h"
#include "lightenvironment.h"
#include "surfaceclass.h"
#include "texture.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <d3dx8core.h>

namespace
{
struct DX8ViewCaptureState
{
    IDirect3DSurface8 * oldRenderSurface;
    IDirect3DTexture8 * renderTexture;
    IDirect3DSurface8 * newRenderSurface;
    IDirect3DSurface8 * oldDepthSurface;
    bool active;
};

static DX8ViewCaptureState g_tacticalViewCapture = { nullptr, nullptr, nullptr, nullptr, false };
static DWORD g_profilerSwizzleShader = 0;
static const DWORD kGrayscaleLuminanceWeights = 0x80A5CA8E;
static const DWORD kGrayscaleFlatGray = 0x60606060;

static DWORD FloatAsDword(float value)
{
    DWORD bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static const DWORD * GetXYZNDUV1Declaration()
{
    static const DWORD declaration[] =
    {
        D3DVSD_STREAM(0),
        D3DVSD_REG(0, D3DVSDT_FLOAT3),
        D3DVSD_REG(1, D3DVSDT_FLOAT3),
        D3DVSD_REG(2, D3DVSDT_D3DCOLOR),
        D3DVSD_REG(7, D3DVSDT_FLOAT2),
        D3DVSD_END()
    };
    return declaration;
}

static DX8ViewCaptureState * GetViewCaptureState(RenderBackendViewCaptureKind kind)
{
    if (kind != RB_VIEW_CAPTURE_TACTICAL) {
        return nullptr;
    }
    return &g_tacticalViewCapture;
}

static void ReleaseViewCaptureState(DX8ViewCaptureState & state)
{
    if (state.newRenderSurface != nullptr) {
        state.newRenderSurface->Release();
    }
    if (state.renderTexture != nullptr) {
        state.renderTexture->Release();
    }
    if (state.oldRenderSurface != nullptr) {
        state.oldRenderSurface->Release();
    }
    if (state.oldDepthSurface != nullptr) {
        state.oldDepthSurface->Release();
    }
    state.oldRenderSurface = nullptr;
    state.renderTexture = nullptr;
    state.newRenderSurface = nullptr;
    state.oldDepthSurface = nullptr;
    state.active = false;
}

static DWORD GetScreenQuadFVF(bool use_second_uv)
{
    return D3DFVF_XYZRHW | D3DFVF_DIFFUSE | (use_second_uv ? D3DFVF_TEX2 : D3DFVF_TEX1);
}

static bool EnsureProfilerSwizzleShader()
{
    if (g_profilerSwizzleShader != 0) {
        return true;
    }

    ID3DXBuffer * compiledShader = nullptr;
    const char * shader =
        "ps.1.4\n"
        "texld r0, t0\n"
        "mov r1.a, r0.r\n"
        "mov r2.a, r0.g\n"
        "mov r3.a, r0.b\n"
        "mul r0.rgb, r3.a, c0\n"
        "mad r0.rgb, r2.a, c1, r0\n"
        "mad r0.rgb, r1.a, c2, r0\n";

    HRESULT hr = D3DXAssembleShader(shader, strlen(shader), 0, nullptr, &compiledShader, nullptr);
    if (FAILED(hr) || compiledShader == nullptr) {
        return false;
    }

    hr = DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader(
        reinterpret_cast<const DWORD *>(compiledShader->GetBufferPointer()),
        &g_profilerSwizzleShader);
    compiledShader->Release();

    if (FAILED(hr)) {
        g_profilerSwizzleShader = 0;
        return false;
    }

    return true;
}
}

DX8Backend::DX8Backend()
{
}

DX8Backend::~DX8Backend()
{
}

// -- Backend lifecycle -------------------------------------------------------
//
// DX8Backend is a passive forwarder: DX8Wrapper::Init has already done the
// real device creation before this runs, so there is nothing to do here.
// These exist so the abstract interface has a uniform lifecycle hook.

void DX8Backend::Initialize(void * /*hwnd*/, int /*width*/, int /*height*/)
{
}

void DX8Backend::Shutdown()
{
    Release_View_Capture(RB_VIEW_CAPTURE_TACTICAL);
    if (g_profilerSwizzleShader != 0 && DX8Wrapper::_Get_D3D_Device8() != nullptr) {
        DX8Wrapper::_Get_D3D_Device8()->DeletePixelShader(g_profilerSwizzleShader);
        g_profilerSwizzleShader = 0;
    }
}

// -- Device state queries ----------------------------------------------------

bool DX8Backend::Is_Device_Lost() const
{
    return DX8Wrapper::Is_Device_Lost();
}

RenderBackendDeviceStatus DX8Backend::Get_Device_Status() const
{
    IDirect3DDevice8 * device = DX8Wrapper::_Get_D3D_Device8();
    if (device == nullptr) {
        return RB_DEVICE_OK;
    }

    HRESULT hr = device->TestCooperativeLevel();
    if (hr == D3DERR_DEVICELOST) {
        return RB_DEVICE_LOST;
    }
    if (hr == D3DERR_DEVICENOTRESET) {
        return RB_DEVICE_NOT_RESET;
    }
    return RB_DEVICE_OK;
}

void DX8Backend::Reset_Device()
{
    DX8Wrapper::Reset_Device();
}

void DX8Backend::Set_Device_Cleanup_Hook(RenderDeviceCleanupHook * hook)
{
    DX8Wrapper::SetCleanupHook(hook);
}

void DX8Backend::Set_MSAA_Mode(RenderBackendMSAAMode mode)
{
    switch (mode) {
    default:
    case RB_MSAA_NONE:
        DX8Wrapper::Set_MSAA_Mode(D3DMULTISAMPLE_NONE);
        break;

    case RB_MSAA_2X:
        DX8Wrapper::Set_MSAA_Mode(D3DMULTISAMPLE_2_SAMPLES);
        break;

    case RB_MSAA_4X:
        DX8Wrapper::Set_MSAA_Mode(D3DMULTISAMPLE_4_SAMPLES);
        break;

    case RB_MSAA_8X:
        DX8Wrapper::Set_MSAA_Mode(D3DMULTISAMPLE_8_SAMPLES);
        break;
    }
}

RenderBackendMSAAMode DX8Backend::Get_MSAA_Mode() const
{
    switch (DX8Wrapper::Get_MSAA_Mode()) {
    default:
    case D3DMULTISAMPLE_NONE:
        return RB_MSAA_NONE;

    case D3DMULTISAMPLE_2_SAMPLES:
        return RB_MSAA_2X;

    case D3DMULTISAMPLE_4_SAMPLES:
        return RB_MSAA_4X;

    case D3DMULTISAMPLE_8_SAMPLES:
        return RB_MSAA_8X;
    }
}

bool DX8Backend::Supports_Dot3() const
{
    return DX8Wrapper::Get_Current_Caps() != nullptr
        && DX8Wrapper::Get_Current_Caps()->Support_Dot3();
}

bool DX8Backend::Get_Device_Identity(RenderBackendDeviceIdentity & identity) const
{
    const DX8Caps * caps = DX8Wrapper::Get_Current_Caps();
    if (caps == nullptr)
    {
        return false;
    }

    identity = {};
    identity.max_simultaneous_textures = caps->Get_Max_Simultaneous_Textures();
    identity.pixel_shader_major = caps->Get_Pixel_Shader_Major_Version();
    identity.pixel_shader_minor = caps->Get_Pixel_Shader_Minor_Version();

    IDirect3D8 * d3d = DX8Wrapper::_Get_D3D8();
    if (d3d != nullptr)
    {
        D3DADAPTER_IDENTIFIER8 adapter;
        ::ZeroMemory(&adapter, sizeof(adapter));
        if (SUCCEEDED(d3d->GetAdapterIdentifier(0, D3DENUM_NO_WHQL_LEVEL, &adapter)))
        {
            identity.vendor_id = adapter.VendorId;
            identity.device_id = adapter.DeviceId;
#ifdef _WIN32
            identity.driver_version = static_cast<std::uint64_t>(adapter.DriverVersion.QuadPart);
#else
            identity.driver_version =
                (static_cast<std::uint64_t>(adapter.DriverVersionHighPart) << 32) |
                adapter.DriverVersionLowPart;
#endif
        }
    }

    return true;
}

bool DX8Backend::Has_Stencil() const
{
    return DX8Wrapper::Has_Stencil();
}

WW3DFormat DX8Backend::Get_Back_Buffer_Format() const
{
    return DX8Wrapper::getBackBufferFormat();
}

bool DX8Backend::Get_Back_Buffer_Description(unsigned int num, RenderBackendSurfaceDescription & desc) const
{
    desc = RenderBackendSurfaceDescription();

    SurfaceClass * back_buffer = DX8Wrapper::_Get_DX8_Back_Buffer(num);
    if (back_buffer == nullptr)
    {
        return false;
    }

    SurfaceClass::SurfaceDescription surface_desc;
    back_buffer->Get_Description(surface_desc);
    REF_PTR_RELEASE(back_buffer);

    desc.Width = surface_desc.Width;
    desc.Height = surface_desc.Height;
    desc.Format = surface_desc.Format;
    return desc.Is_Valid();
}

static SurfaceClass * Capture_Back_Buffer_Surface(unsigned int num)
{
    SurfaceClass * back_buffer = DX8Wrapper::_Get_DX8_Back_Buffer(num);
    if (back_buffer == nullptr)
    {
        return nullptr;
    }

    SurfaceClass::SurfaceDescription desc;
    back_buffer->Get_Description(desc);
    // TheSuperHackers @build bobtista 01/06/2026 SurfaceClass(void*) is
    // private; use the public Create_Legacy_Surface_Wrapper factory.
    SurfaceClass * copy = Create_Legacy_Surface_Wrapper(
        DX8Wrapper::_Create_DX8_Surface(desc.Width, desc.Height, desc.Format));
    if (copy != nullptr)
    {
        DX8Wrapper::_Copy_DX8_Rects(
            Peek_Legacy_Surface(*back_buffer),
            nullptr,
            0,
            Peek_Legacy_Surface(*copy),
            nullptr);
    }

    back_buffer->Release_Ref();
    return copy;
}

bool DX8Backend::Capture_Back_Buffer_Image(unsigned int num, RenderBackendImage & image)
{
    image = RenderBackendImage();

    SurfaceClass * copy = Capture_Back_Buffer_Surface(num);
    if (copy == nullptr)
    {
        return false;
    }

    SurfaceClass::SurfaceDescription desc;
    copy->Get_Description(desc);

    int source_pitch = 0;
    unsigned char *source_bits = static_cast<unsigned char *>(copy->Lock(&source_pitch));
    if (source_bits == nullptr)
    {
        copy->Release_Ref();
        return false;
    }

    const unsigned row_bytes = desc.Width * 4;
    image.Width = desc.Width;
    image.Height = desc.Height;
    image.Format = desc.Format;
    image.Pitch = row_bytes;
    image.Bytes.resize(static_cast<size_t>(image.Pitch) * image.Height);

    for (unsigned row = 0; row < image.Height; ++row)
    {
        memcpy(image.Bytes.data() + static_cast<size_t>(row) * image.Pitch,
               source_bits + static_cast<size_t>(row) * source_pitch,
               row_bytes);
    }

    copy->Unlock();
    copy->Release_Ref();
    return true;
}

// TheSuperHackers @bugfix bobtista 03/06/2026 GPU-direct back-buffer →
// texture-surface copy for the smudge background snapshot. Replaces the
// per-frame Capture_Back_Buffer_Image + CPU readback that was leaking
// ~4 MB of system memory per call (causing dx8 to exhaust the 2 GB
// virtual address space within ~14 s on heavy combat saves). CopyRects
// stays entirely on the GPU and reuses dst_texture's POOL_DEFAULT
// surface across frames — zero allocations.
bool DX8Backend::Copy_Back_Buffer_To_Texture(unsigned int num, TextureClass * dst_texture)
{
    if (dst_texture == nullptr) {
        return false;
    }
    SurfaceClass * back_buffer = DX8Wrapper::_Get_DX8_Back_Buffer(num);
    if (back_buffer == nullptr) {
        return false;
    }
    // TextureClass::Get_Surface_Level returns a fresh wrapper around the
    // CPU mip data — copying onto that wouldn't update the GPU texture.
    // Pull the actual D3D8 surface level via the compatibility interop.
    // TheSuperHackers @bugfix bobtista 10/07/2026 Get_Native_Compatibility_Surface_Level goes through
    // GetSurfaceLevel, which AddRefs the returned surface, so dst_native must be released on every path.
    // bb_native comes from Peek_Legacy_Surface (no AddRef) and must not be.
    IDirect3DSurface8 * dst_native = Get_Native_Compatibility_Surface_Level(*dst_texture, 0);
    IDirect3DSurface8 * bb_native = Peek_Legacy_Surface(*back_buffer);
    if (dst_native == nullptr || bb_native == nullptr) {
        if (dst_native != nullptr) { dst_native->Release(); }
        REF_PTR_RELEASE(back_buffer);
        return false;
    }
    DX8Wrapper::_Copy_DX8_Rects(bb_native, nullptr, 0, dst_native, nullptr);
    dst_native->Release();
    REF_PTR_RELEASE(back_buffer);
    return true;
}

void DX8Backend::Set_Texture_Bitdepth(int bitdepth)
{
    DX8Wrapper::Set_Texture_Bitdepth(bitdepth);
}

int DX8Backend::Get_Texture_Bitdepth() const
{
    return DX8Wrapper::Get_Texture_Bitdepth();
}

bool DX8Backend::Supports_Texture_Format(WW3DFormat format) const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    return caps != nullptr && caps->Support_Texture_Format(format);
}

bool DX8Backend::Supports_Compressed_Textures() const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    return caps != nullptr && caps->Support_DXTC();
}

bool DX8Backend::Supports_Bump_Envmap() const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    return caps != nullptr && caps->Support_Bump_Envmap();
}

bool DX8Backend::Supports_Bump_Envmap_Luminance() const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    return caps != nullptr && caps->Support_Bump_Envmap_Luminance();
}

bool DX8Backend::Supports_Texture_Filter(RenderBackendTextureFilterCapability capability) const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    if (caps == nullptr)
    {
        return false;
    }

    const DWORD filter_caps = caps->Get_DX8_Caps().TextureFilterCaps;
    switch (capability)
    {
        case RB_TEXTURE_FILTER_MIN_LINEAR:
            return (filter_caps & D3DPTFILTERCAPS_MINFLINEAR) != 0;
        case RB_TEXTURE_FILTER_MAG_LINEAR:
            return (filter_caps & D3DPTFILTERCAPS_MAGFLINEAR) != 0;
        case RB_TEXTURE_FILTER_MIP_LINEAR:
            return (filter_caps & D3DPTFILTERCAPS_MIPFLINEAR) != 0;
        case RB_TEXTURE_FILTER_MIN_ANISOTROPIC:
            return (filter_caps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0;
        case RB_TEXTURE_FILTER_MAG_ANISOTROPIC:
            return (filter_caps & D3DPTFILTERCAPS_MAGFANISOTROPIC) != 0;
        default:
            return false;
    }
}

bool DX8Backend::Supports_Texture_Op(RenderBackendTextureOpCapability capability) const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    if (caps == nullptr)
    {
        return false;
    }

    const DWORD texture_op_caps = caps->Get_DX8_Caps().TextureOpCaps;
    switch (capability)
    {
        case RB_TEXTURE_OP_SELECTARG1:
            return (texture_op_caps & D3DTEXOPCAPS_SELECTARG1) != 0;
        case RB_TEXTURE_OP_MODULATE:
            return (texture_op_caps & D3DTEXOPCAPS_MODULATE) != 0;
        case RB_TEXTURE_OP_MODULATE2X:
            return (texture_op_caps & D3DTEXOPCAPS_MODULATE2X) != 0;
        case RB_TEXTURE_OP_ADD:
            return (texture_op_caps & D3DTEXOPCAPS_ADD) != 0;
        case RB_TEXTURE_OP_BUMPENVMAP:
            return (texture_op_caps & D3DTEXOPCAPS_BUMPENVMAP) != 0;
        case RB_TEXTURE_OP_BUMPENVMAPLUMINANCE:
            return (texture_op_caps & D3DTEXOPCAPS_BUMPENVMAPLUMINANCE) != 0;
        case RB_TEXTURE_OP_ADDSMOOTH:
            return (texture_op_caps & D3DTEXOPCAPS_ADDSMOOTH) != 0;
        case RB_TEXTURE_OP_SUBTRACT:
            return (texture_op_caps & D3DTEXOPCAPS_SUBTRACT) != 0;
        case RB_TEXTURE_OP_BLENDTEXTUREALPHA:
            return (texture_op_caps & D3DTEXOPCAPS_BLENDTEXTUREALPHA) != 0;
        case RB_TEXTURE_OP_BLENDCURRENTALPHA:
            return (texture_op_caps & D3DTEXOPCAPS_BLENDCURRENTALPHA) != 0;
        case RB_TEXTURE_OP_ADDSIGNED:
            return (texture_op_caps & D3DTEXOPCAPS_ADDSIGNED) != 0;
        case RB_TEXTURE_OP_ADDSIGNED2X:
            return (texture_op_caps & D3DTEXOPCAPS_ADDSIGNED2X) != 0;
        case RB_TEXTURE_OP_MODULATEALPHA_ADDCOLOR:
            return caps->Support_ModAlphaAddClr();
        default:
            return false;
    }
}

bool DX8Backend::Supports_Fog() const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    return caps != nullptr && caps->Is_Fog_Allowed();
}

bool DX8Backend::Is_Legacy_Voodoo3() const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    return caps != nullptr
        && caps->Get_Vendor() == DX8Caps::VENDOR_3DFX
        && caps->Get_Device() == DX8Caps::DEVICE_3DFX_VOODOO_3;
}

bool DX8Backend::Supports_NPatches() const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    return caps != nullptr && caps->Support_NPatches();
}

bool DX8Backend::Supports_Hardware_Transform_And_Lighting() const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    return caps != nullptr && caps->Support_TnL();
}

bool DX8Backend::Supports_Point_Sprites() const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    return caps != nullptr && caps->Support_PointSprites();
}

RenderBackendTextureLimits DX8Backend::Get_Texture_Limits() const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    if (caps == nullptr)
    {
        return IRenderBackend::Get_Texture_Limits();
    }

    const D3DCAPS8 & dx8caps = caps->Get_DX8_Caps();
    return {
        dx8caps.MaxTextureWidth,
        dx8caps.MaxTextureHeight,
        dx8caps.MaxVolumeExtent,
        dx8caps.MaxTextureAspectRatio != 0 ? dx8caps.MaxTextureAspectRatio : 8
    };
}

int DX8Backend::Get_Max_Texture_Stages() const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    return caps != nullptr ? caps->Get_Max_Textures_Per_Pass() : RB_MAX_TEXTURE_STAGES;
}

bool DX8Backend::Supports_Z_Bias() const
{
    const auto * caps = DX8Wrapper::Get_Current_Caps();
    return caps != nullptr && caps->Support_ZBias();
}

void DX8Backend::Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit)
{
    DX8Wrapper::Set_Gamma(gamma, bright, contrast, calibrate, uselimit);
}

// -- Frame lifecycle ---------------------------------------------------------
//
// TheSuperHackers @refactor bobtista 11/04/2026 The g_renderBackend Begin/End_Scene pair is
// a parallel per-frame hook alongside ww3d.cpp's direct DX8Wrapper::Begin_Scene/End_Scene calls.

void DX8Backend::Begin_Scene()
{
}

namespace
{

// TheSuperHackers @feature bobtista 01/06/2026 In-engine back-buffer screenshot for dx8 via
// GGC_DX8_SCREENSHOT_AFTER/INTERVAL/PATH (persistent user env vars), mirroring the bgfx
// mechanism; GDI capture cannot see D3D8 surfaces, so this writes a BMP from the back buffer.
struct Dx8ScreenshotState
{
    int targetFrame = -1;
    int interval = 0;
    char basePath[512] = {};
    bool envResolved = false;
    int frameIndex = 0;
};

Dx8ScreenshotState & Get_Dx8_Screenshot_State()
{
    static Dx8ScreenshotState s;
    return s;
}

void Resolve_Dx8_Screenshot_Env()
{
    Dx8ScreenshotState & s = Get_Dx8_Screenshot_State();
    if (s.envResolved) {
        return;
    }
    s.envResolved = true;
    if (const char * a = std::getenv("GGC_DX8_SCREENSHOT_AFTER")) {
        s.targetFrame = std::atoi(a);
    }
    if (const char * i = std::getenv("GGC_DX8_SCREENSHOT_INTERVAL")) {
        s.interval = std::atoi(i);
    }
    if (const char * p = std::getenv("GGC_DX8_SCREENSHOT_PATH")) {
        std::strncpy(s.basePath, p, sizeof(s.basePath) - 1);
    }
}

bool Should_Take_Dx8_Screenshot()
{
    Dx8ScreenshotState & s = Get_Dx8_Screenshot_State();
    if (s.targetFrame <= 0 || s.basePath[0] == '\0') {
        return false;
    }
    if (s.frameIndex == s.targetFrame) {
        return true;
    }
    if (s.interval > 0
        && s.frameIndex > s.targetFrame
        && ((s.frameIndex - s.targetFrame) % s.interval) == 0) {
        return true;
    }
    return false;
}

// Write a 32-bit top-down BGRA bitmap. Treats the source as BGRA8 regardless
// of the reported format — D3D8's swap buffer is normally A8R8G8B8 (which is
// little-endian BGRA) so this is correct for the common case. Returns true on
// successful write.
bool Save_Bgra_Bmp(const char * path, unsigned width, unsigned height,
                   unsigned src_pitch, const std::uint8_t * src_bytes)
{
    FILE * f = std::fopen(path, "wb");
    if (f == nullptr) {
        return false;
    }
    const std::uint32_t row_bytes = width * 4;
    const std::uint32_t pixel_data_bytes = row_bytes * height;
    const std::uint32_t bf_size = 14 + 40 + pixel_data_bytes;

    // BITMAPFILEHEADER
    std::uint8_t header[14] = {};
    header[0] = 'B'; header[1] = 'M';
    header[2] = static_cast<std::uint8_t>(bf_size & 0xFF);
    header[3] = static_cast<std::uint8_t>((bf_size >> 8) & 0xFF);
    header[4] = static_cast<std::uint8_t>((bf_size >> 16) & 0xFF);
    header[5] = static_cast<std::uint8_t>((bf_size >> 24) & 0xFF);
    header[10] = 14 + 40;  // offset to pixel data
    std::fwrite(header, 1, sizeof(header), f);

    // BITMAPINFOHEADER (40 bytes), height NEGATIVE so it's top-down
    std::uint8_t info[40] = {};
    info[0] = 40;
    info[4] = static_cast<std::uint8_t>(width & 0xFF);
    info[5] = static_cast<std::uint8_t>((width >> 8) & 0xFF);
    info[6] = static_cast<std::uint8_t>((width >> 16) & 0xFF);
    info[7] = static_cast<std::uint8_t>((width >> 24) & 0xFF);
    const std::int32_t neg_h = -static_cast<std::int32_t>(height);
    std::memcpy(info + 8, &neg_h, 4);
    info[12] = 1; // planes
    info[14] = 32; // bits per pixel
    std::memcpy(info + 20, &pixel_data_bytes, 4);
    std::fwrite(info, 1, sizeof(info), f);

    for (unsigned row = 0; row < height; ++row) {
        std::fwrite(src_bytes + static_cast<size_t>(row) * src_pitch, 1, row_bytes, f);
    }
    std::fclose(f);
    return true;
}

} // namespace

void DX8Backend::End_Scene(bool /*flip_frame*/)
{
    DrawCallLog_End_Frame();
    RenderDoc_Maybe_Trigger_Capture();

    Resolve_Dx8_Screenshot_Env();
    if (Should_Take_Dx8_Screenshot()) {
        RenderBackendImage img;
        if (Capture_Back_Buffer_Image(0, img) && img.Is_Valid()) {
            Dx8ScreenshotState & s = Get_Dx8_Screenshot_State();
            char path[640];
            std::snprintf(path, sizeof(path), "%s.%06d.bmp",
                s.basePath, s.frameIndex);
            Save_Bgra_Bmp(path, img.Width, img.Height, img.Pitch, img.Bytes.data());
        }
    }
    ++Get_Dx8_Screenshot_State().frameIndex;
}

void DX8Backend::Flip_To_Primary()
{
    DX8Wrapper::Flip_To_Primary();
}

void DX8Backend::Begin_Device_Statistics()
{
    DX8Wrapper::Begin_Statistics();
}

void DX8Backend::End_Device_Statistics()
{
    DX8Wrapper::End_Statistics();
}

void DX8Backend::Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha, float z, unsigned int stencil)
{
    DX8Wrapper::Clear(clear_color, clear_z_stencil, color, dest_alpha, z, stencil);
}

void DX8Backend::Set_Viewport(const RenderBackendViewport & viewport)
{
    D3DVIEWPORT8 vp;
    vp.X      = viewport.x;
    vp.Y      = viewport.y;
    vp.Width  = viewport.width;
    vp.Height = viewport.height;
    vp.MinZ   = viewport.min_z;
    vp.MaxZ   = viewport.max_z;
    DX8Wrapper::Set_Viewport(&vp);
}

bool DX8Backend::Initialize_View_Capture(RenderBackendViewCaptureKind kind)
{
    DX8ViewCaptureState * state = GetViewCaptureState(kind);
    if (state == nullptr) {
        return false;
    }

    ReleaseViewCaptureState(*state);

    IDirect3DDevice8 * device = DX8Wrapper::_Get_D3D_Device8();
    if (device == nullptr) {
        return false;
    }

    HRESULT hr = device->GetRenderTarget(&state->oldRenderSurface);
    if (hr != S_OK || state->oldRenderSurface == nullptr) {
        ReleaseViewCaptureState(*state);
        return false;
    }

    D3DSURFACE_DESC desc;
    state->oldRenderSurface->GetDesc(&desc);

    // The legacy DX8 RTT path cannot pair a non-MSAA texture with an MSAA
    // depth surface. Preserve the existing failure behavior instead of
    // trying to paper over driver-forced MSAA.
    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
        ReleaseViewCaptureState(*state);
        return false;
    }

    hr = device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET,
                               desc.Format, D3DPOOL_DEFAULT, &state->renderTexture);
    if (hr != S_OK || state->renderTexture == nullptr) {
        ReleaseViewCaptureState(*state);
        return false;
    }

    hr = state->renderTexture->GetSurfaceLevel(0, &state->newRenderSurface);
    if (hr != S_OK || state->newRenderSurface == nullptr) {
        ReleaseViewCaptureState(*state);
        return false;
    }

    hr = device->GetDepthStencilSurface(&state->oldDepthSurface);
    if (hr != S_OK || state->oldDepthSurface == nullptr) {
        ReleaseViewCaptureState(*state);
        return false;
    }

    return true;
}

void DX8Backend::Release_View_Capture(RenderBackendViewCaptureKind kind)
{
    DX8ViewCaptureState * state = GetViewCaptureState(kind);
    if (state != nullptr) {
        ReleaseViewCaptureState(*state);
    }
}

bool DX8Backend::Supports_View_Capture(RenderBackendViewCaptureKind kind) const
{
    const DX8ViewCaptureState * state = GetViewCaptureState(kind);
    return state != nullptr && state->newRenderSurface != nullptr && state->oldDepthSurface != nullptr;
}

bool DX8Backend::Begin_View_Capture(RenderBackendViewCaptureKind kind)
{
    DX8ViewCaptureState * state = GetViewCaptureState(kind);
    if (state == nullptr || state->active || state->newRenderSurface == nullptr || state->oldDepthSurface == nullptr) {
        return false;
    }

    HRESULT hr = DX8Wrapper::_Get_D3D_Device8()->SetRenderTarget(state->newRenderSurface, state->oldDepthSurface);
    if (hr != S_OK) {
        ReleaseViewCaptureState(*state);
        return false;
    }

    state->active = true;
    return true;
}

bool DX8Backend::End_View_Capture(RenderBackendViewCaptureKind kind)
{
    DX8ViewCaptureState * state = GetViewCaptureState(kind);
    if (state == nullptr || !state->active) {
        return false;
    }

    HRESULT hr = DX8Wrapper::_Get_D3D_Device8()->SetRenderTarget(state->oldRenderSurface, state->oldDepthSurface);
    if (hr != S_OK) {
        state->active = false;
        return false;
    }

    DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSW, D3DTADDRESS_CLAMP);
    DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);

    state->active = false;
    return true;
}

bool DX8Backend::Is_View_Capture_Active(RenderBackendViewCaptureKind kind) const
{
    const DX8ViewCaptureState * state = GetViewCaptureState(kind);
    return state != nullptr && state->active;
}

bool DX8Backend::Has_View_Capture(RenderBackendViewCaptureKind kind) const
{
    const DX8ViewCaptureState * state = GetViewCaptureState(kind);
    return state != nullptr && state->renderTexture != nullptr;
}

bool DX8Backend::Bind_View_Capture_Texture(RenderBackendViewCaptureKind kind, unsigned int stage)
{
    DX8ViewCaptureState * state = GetViewCaptureState(kind);
    if (state == nullptr || state->renderTexture == nullptr) {
        return false;
    }

    DX8Wrapper::Set_DX8_Texture(stage, state->renderTexture);
    DX8Wrapper::Set_Texture(stage, nullptr);
    return true;
}

bool DX8Backend::Draw_View_Capture_Quad(RenderBackendViewCaptureKind kind,
                                        const RenderBackendScreenVertex * vertices,
                                        unsigned int vertex_count,
                                        bool use_second_uv)
{
    if (vertex_count < 4 || vertices == nullptr || !Bind_View_Capture_Texture(kind, 0)) {
        return false;
    }

    return Draw_Screen_Quad(vertices, vertex_count, use_second_uv);
}

bool DX8Backend::Draw_Screen_Quad(const RenderBackendScreenVertex * vertices,
                                  unsigned int vertex_count,
                                  bool use_second_uv)
{
    if (vertex_count < 4 || vertices == nullptr) {
        return false;
    }

    IDirect3DDevice8 * device = DX8Wrapper::_Get_D3D_Device8();
    if (device == nullptr) {
        return false;
    }

    device->SetVertexShader(GetScreenQuadFVF(use_second_uv));
    HRESULT hr = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(RenderBackendScreenVertex));
    return hr == S_OK;
}

bool DX8Backend::Capture_Back_Buffer_RGBA(unsigned int display_width,
                                          unsigned int display_height,
                                          unsigned int image_size,
                                          unsigned char * output_pixels,
                                          unsigned int output_capacity,
                                          unsigned int * output_width,
                                          unsigned int * output_height)
{
    if (display_width == 0 || display_height == 0 || image_size == 0 ||
        output_pixels == nullptr || output_width == nullptr || output_height == nullptr ||
        !EnsureProfilerSwizzleShader())
    {
        return false;
    }

    const float aspect_ratio = static_cast<float>(display_height) / static_cast<float>(display_width);
    unsigned int capture_height = static_cast<unsigned int>((image_size * aspect_ratio) + 0.5f);
    if (capture_height > image_size) {
        capture_height = image_size;
    }
    if (capture_height == 0) {
        return false;
    }

    const unsigned int row_bytes = image_size * 4;
    const unsigned int required_bytes = row_bytes * capture_height;
    if (output_capacity < required_bytes) {
        return false;
    }
    *output_width = 0;
    *output_height = 0;

    bool result = false;
    TextureClass * render_target = nullptr;
    SurfaceClass * surface_class = nullptr;
    SurfaceClass * back_buffer = nullptr;
    IDirect3DTexture8 * intermediate_texture = nullptr;
    IDirect3DSurface8 * intermediate_surface = nullptr;
    IDirect3DSurface8 * small_render_target_surface = nullptr;
    D3DVIEWPORT8 restore_viewport;
    memset(&restore_viewport, 0, sizeof(restore_viewport));
    bool viewport_valid = false;
    bool render_target_changed = false;
    bool shader_changed = false;
    bool texture_changed = false;

    render_target = DX8Wrapper::Create_Render_Target(image_size, image_size, WW3D_FORMAT_A8R8G8B8);
    surface_class = NEW_REF(SurfaceClass, (image_size, capture_height, WW3D_FORMAT_A8R8G8B8));
    back_buffer = DX8Wrapper::_Get_DX8_Back_Buffer();
    if (render_target == nullptr || surface_class == nullptr || back_buffer == nullptr) {
        goto cleanup;
    }

    IDirect3DSurface8 * back_buffer_surface;
    back_buffer_surface = Peek_Legacy_Surface(*back_buffer);
    if (back_buffer_surface == nullptr) {
        goto cleanup;
    }

    D3DSURFACE_DESC back_buffer_desc;
    if (FAILED(back_buffer_surface->GetDesc(&back_buffer_desc))) {
        goto cleanup;
    }

    if (FAILED(DX8Wrapper::_Get_D3D_Device8()->CreateTexture(
            back_buffer_desc.Width,
            back_buffer_desc.Height,
            1,
            D3DUSAGE_RENDERTARGET,
            back_buffer_desc.Format,
            D3DPOOL_DEFAULT,
            &intermediate_texture)) ||
        intermediate_texture == nullptr)
    {
        goto cleanup;
    }

    if (FAILED(intermediate_texture->GetSurfaceLevel(0, &intermediate_surface)) ||
        intermediate_surface == nullptr)
    {
        goto cleanup;
    }
    DX8Wrapper::_Copy_DX8_Rects(back_buffer_surface, nullptr, 0, intermediate_surface, nullptr);

    small_render_target_surface = Get_Native_Compatibility_Surface_Level(*render_target);
    if (small_render_target_surface == nullptr) {
        goto cleanup;
    }
    DX8Wrapper::Set_Render_Target(small_render_target_surface, false);
    render_target_changed = true;

    IDirect3DDevice8 * device;
    device = DX8Wrapper::_Get_D3D_Device8();
    if (device == nullptr || FAILED(device->GetViewport(&restore_viewport))) {
        goto cleanup;
    }
    viewport_valid = true;

    D3DVIEWPORT8 viewport;
    viewport.X = 0;
    viewport.Y = 0;
    viewport.Width = image_size;
    viewport.Height = capture_height;
    viewport.MinZ = 0.0f;
    viewport.MaxZ = 1.0f;
    DX8Wrapper::Set_Viewport(&viewport);

    DX8Wrapper::Set_Pixel_Shader(g_profilerSwizzleShader);
    shader_changed = true;
    static const float kMaskR[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    static const float kMaskG[4] = {0.0f, 1.0f, 0.0f, 0.0f};
    static const float kMaskB[4] = {0.0f, 0.0f, 1.0f, 0.0f};
    Set_Pixel_Shader_Constant(0, kMaskR, 1);
    Set_Pixel_Shader_Constant(1, kMaskG, 1);
    Set_Pixel_Shader_Constant(2, kMaskB, 1);

    // TheSuperHackers @build bobtista 01/06/2026 Wrap initialized locals in
    // block scopes so the subsequent `goto cleanup` statements don't cross
    // their initialization (ill-formed in C++ -- C2362 under MSVC).
    {
        struct QuadVertex
        {
            float x, y, z, rhw;
            float u, v;
        } vtx[4];
        const float left = -0.5f;
        const float top = -0.5f;
        const float right = static_cast<float>(image_size) - 0.5f;
        const float bottom = static_cast<float>(capture_height) - 0.5f;
        vtx[0] = {right, bottom, 0.0f, 1.0f, 1.0f, 1.0f};
        vtx[1] = {right, top,    0.0f, 1.0f, 1.0f, 0.0f};
        vtx[2] = {left,  bottom, 0.0f, 1.0f, 0.0f, 1.0f};
        vtx[3] = {left,  top,    0.0f, 1.0f, 0.0f, 0.0f};
        DX8Wrapper::Set_DX8_Texture(0, intermediate_texture);
        texture_changed = true;
        DX8Wrapper::Set_Vertex_Shader(D3DFVF_XYZRHW | D3DFVF_TEX1);
        if (FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vtx, sizeof(QuadVertex)))) {
            goto cleanup;
        }
    }

    {
        RECT src_rect;
        src_rect.left = 0;
        src_rect.top = 0;
        src_rect.right = image_size;
        src_rect.bottom = capture_height;
        POINT dst_point;
        dst_point.x = 0;
        dst_point.y = 0;
        DX8Wrapper::_Copy_DX8_Rects(
            small_render_target_surface,
            &src_rect,
            1,
            Peek_Legacy_Surface(*surface_class),
            &dst_point);

        int pitch = 0;
        void * bits = surface_class->Lock(&pitch);
        if (bits == nullptr) {
            goto cleanup;
        }

        for (unsigned int row = 0; row < capture_height; ++row)
        {
            memcpy(output_pixels + row * row_bytes,
                   static_cast<const unsigned char *>(bits) + row * pitch,
                   row_bytes);
        }
        surface_class->Unlock();
    }

    *output_width = image_size;
    *output_height = capture_height;
    result = true;

cleanup:
    if (shader_changed) {
        DX8Wrapper::Set_Pixel_Shader(0);
    }
    if (texture_changed) {
        DX8Wrapper::Set_DX8_Texture(0, nullptr);
    }
    if (viewport_valid) {
        DX8Wrapper::Set_Viewport(&restore_viewport);
    }
    if (render_target_changed) {
        DX8Wrapper::Set_Render_Target(static_cast<IDirect3DSurface8 *>(nullptr));
    }
    if (small_render_target_surface != nullptr) {
        small_render_target_surface->Release();
    }
    if (intermediate_surface != nullptr) {
        intermediate_surface->Release();
    }
    if (intermediate_texture != nullptr) {
        intermediate_texture->Release();
    }
    REF_PTR_RELEASE(back_buffer);
    REF_PTR_RELEASE(surface_class);
    REF_PTR_RELEASE(render_target);
    return result;
}

// -- Vertex / index buffers --------------------------------------------------

void DX8Backend::Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream)
{
    DX8Wrapper::Set_Vertex_Buffer(vb, stream);
}

void DX8Backend::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
    DX8Wrapper::Set_Vertex_Buffer(vba);
}

void DX8Backend::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset)
{
    DX8Wrapper::Set_Index_Buffer(ib, index_base_offset);
}

void DX8Backend::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
    DX8Wrapper::Set_Index_Buffer(iba, index_base_offset);
}

void DX8Backend::Set_Index_Buffer_Index_Offset(unsigned int offset)
{
    DX8Wrapper::Set_Index_Buffer_Index_Offset(offset);
}

void DX8Backend::Apply_Sorted_Batch_State(const RenderBackendSortedBatchState & state)
{
    if (state.shader != nullptr)
    {
        Set_Shader(*state.shader);
    }
    Set_Material(state.material);
    for (unsigned i = 0; i < RB_MAX_TEXTURE_STAGES; ++i)
    {
        Set_Texture(i, state.textures[i]);
    }
    if (state.world != nullptr)
    {
        DX8Wrapper::_Set_DX8_Transform(
            D3DTS_WORLD,
            To_D3DMATRIX(*state.world));
    }
    if (state.view != nullptr)
    {
        DX8Wrapper::_Set_DX8_Transform(
            D3DTS_VIEW,
            To_D3DMATRIX(*state.view));
    }
    for (int i = 0; i < 4; ++i)
    {
        if (state.lights.enabled[i])
        {
            const RenderBackendLight & src = state.lights.lights[i];
            D3DLIGHT8 light;
            memset(&light, 0, sizeof(light));
            light.Type = static_cast<D3DLIGHTTYPE>(src.type);
            light.Position.x = src.position[0];
            light.Position.y = src.position[1];
            light.Position.z = src.position[2];
            light.Direction.x = src.direction[0];
            light.Direction.y = src.direction[1];
            light.Direction.z = src.direction[2];
            light.Diffuse.r = src.diffuse[0];
            light.Diffuse.g = src.diffuse[1];
            light.Diffuse.b = src.diffuse[2];
            light.Diffuse.a = 1.0f;
            light.Ambient.r = src.ambient[0];
            light.Ambient.g = src.ambient[1];
            light.Ambient.b = src.ambient[2];
            light.Ambient.a = 1.0f;
            light.Specular.r = src.specular[0];
            light.Specular.g = src.specular[1];
            light.Specular.b = src.specular[2];
            light.Specular.a = 1.0f;
            light.Range = src.range;
            light.Falloff = src.falloff;
            light.Attenuation0 = src.attenuation[0];
            light.Attenuation1 = src.attenuation[1];
            light.Attenuation2 = src.attenuation[2];
            light.Theta = src.theta;
            light.Phi = src.phi;
            DX8Wrapper::Set_DX8_Light(i, &light);
        }
        else
        {
            DX8Wrapper::Set_DX8_Light(i, nullptr);
        }
    }
}

void DX8Backend::Restore_Legacy_Render_State_For_Sorted_Draw(const RenderStateStruct & state)
{
    FixedFunctionState::Restore_Render_State(state);
}

void DX8Backend::Capture_Legacy_Render_State_For_Sorted_Draw(RenderStateStruct & state)
{
    FixedFunctionState::Capture_Render_State(state);
}

void DX8Backend::Release_Legacy_Render_State_For_Sorted_Draw()
{
    FixedFunctionState::Release_Render_State();
}

// -- State: shaders, materials, textures ------------------------------------

void DX8Backend::Set_Shader(const ShaderClass & shader)
{
    DX8Wrapper::Set_Shader(shader);
}

void DX8Backend::Get_Shader(ShaderClass & shader)
{
    DX8Wrapper::Get_Shader(shader);
}

void DX8Backend::Set_Material(const VertexMaterialClass * material)
{
    DX8Wrapper::Set_Material(material);
}

void DX8Backend::Apply_Material_State(const RenderBackendMaterialState & material)
{
    D3DMATERIAL8 dx_material;
    memcpy(&dx_material.Diffuse, material.diffuse, sizeof(dx_material.Diffuse));
    memcpy(&dx_material.Ambient, material.ambient, sizeof(dx_material.Ambient));
    memcpy(&dx_material.Specular, material.specular, sizeof(dx_material.Specular));
    memcpy(&dx_material.Emissive, material.emissive, sizeof(dx_material.Emissive));
    dx_material.Power = material.power;
    DX8Wrapper::Set_DX8_Material(&dx_material);
}

void DX8Backend::Set_Material_Color_Source(RenderBackendMaterialColorSource ambient_source,
                                           RenderBackendMaterialColorSource diffuse_source,
                                           RenderBackendMaterialColorSource emissive_source)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_AMBIENTMATERIALSOURCE, static_cast<unsigned>(ambient_source));
    DX8Wrapper::Set_DX8_Render_State(D3DRS_DIFFUSEMATERIALSOURCE, static_cast<unsigned>(diffuse_source));
    DX8Wrapper::Set_DX8_Render_State(D3DRS_EMISSIVEMATERIALSOURCE, static_cast<unsigned>(emissive_source));
}

void DX8Backend::Set_Texture(unsigned int stage, TextureBaseClass * texture)
{
    DX8Wrapper::Set_Texture(stage, texture);
}

void DX8Backend::Upload_Texture_Region(
    TextureClass * dst_texture,
    unsigned int dst_level,
    unsigned int dst_x, unsigned int dst_y,
    const void * src_data,
    unsigned int src_pitch,
    unsigned int region_width, unsigned int region_height,
    WW3DFormat format)
{
    // TheSuperHackers @feature bobtista 01/06/2026 POOL_SYSTEMMEM staging +
    // IDirect3DDevice8::CopyRects is the only valid transport for writing
    // POOL_DEFAULT texture levels (which cannot be locked). Callers like
    // W3DShroud's destination texture rely on this path.
    if (dst_texture == nullptr ||
        src_data == nullptr ||
        region_width == 0 ||
        region_height == 0 ||
        format == WW3D_FORMAT_UNKNOWN)
    {
        return;
    }
    IDirect3DTexture8 * native_texture = Peek_Legacy_Texture2D(*dst_texture);
    IDirect3DDevice8 * device = DX8Wrapper::_Get_D3D_Device8();
    if (native_texture == nullptr || device == nullptr)
    {
        return;
    }
    IDirect3DSurface8 * dst_surface = nullptr;
    if (FAILED(native_texture->GetSurfaceLevel(dst_level, &dst_surface)) ||
        dst_surface == nullptr)
    {
        return;
    }
    IDirect3DSurface8 * staging = nullptr;
    const D3DFORMAT d3d_format = WW3DFormat_To_D3DFormat(format);
    if (SUCCEEDED(device->CreateImageSurface(
            region_width, region_height, d3d_format, &staging)) &&
        staging != nullptr)
    {
        D3DLOCKED_RECT staging_lock;
        ::ZeroMemory(&staging_lock, sizeof(staging_lock));
        if (SUCCEEDED(staging->LockRect(&staging_lock, nullptr, 0)))
        {
            const unsigned bytes_per_pixel = ::Get_Bytes_Per_Pixel(format);
            const unsigned row_bytes = region_width * bytes_per_pixel;
            const unsigned char * src_row =
                static_cast<const unsigned char *>(src_data);
            unsigned char * dst_row =
                static_cast<unsigned char *>(staging_lock.pBits);
            for (unsigned int y = 0; y < region_height; ++y)
            {
                memcpy(dst_row, src_row, row_bytes);
                src_row += src_pitch;
                dst_row += staging_lock.Pitch;
            }
            DX8_ErrorCode(staging->UnlockRect());
            RECT cr_src_rect = {
                0, 0,
                static_cast<LONG>(region_width),
                static_cast<LONG>(region_height) };
            POINT cr_dst_point = {
                static_cast<LONG>(dst_x),
                static_cast<LONG>(dst_y) };
            DX8_ErrorCode(device->CopyRects(
                staging, &cr_src_rect, 1, dst_surface, &cr_dst_point));
        }
        staging->Release();
    }
    dst_surface->Release();
}

void DX8Backend::Bind_Texture_Immediate(unsigned int stage, TextureBaseClass * texture)
{
    IDirect3DBaseTexture8 * raw = (texture != nullptr) ? Peek_Legacy_Base_Texture(*texture) : nullptr;
    DX8Wrapper::Set_DX8_Texture(stage, raw);
    DX8Wrapper::Set_Texture(stage, texture);
}

void DX8Backend::Apply_Render_State_Changes()
{
    DX8Wrapper::Apply_Render_State_Changes();
}

void DX8Backend::Apply_Default_State()
{
    DX8Wrapper::Apply_Default_State();
}

void DX8Backend::Invalidate_Cached_Render_States()
{
    DX8Wrapper::Invalidate_Cached_Render_States();
}

void DX8Backend::Set_Blend_Op(BlendOp op)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_BLENDOP, static_cast<unsigned>(op));
}

void DX8Backend::Set_Blend_Factors(BlendFactor src, BlendFactor dest)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,  static_cast<unsigned>(src));
    DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, static_cast<unsigned>(dest));
}

void DX8Backend::Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha)
{
    unsigned mask = 0;
    if (red)
    {
        mask |= D3DCOLORWRITEENABLE_RED;
    }
    if (green)
    {
        mask |= D3DCOLORWRITEENABLE_GREEN;
    }
    if (blue)
    {
        mask |= D3DCOLORWRITEENABLE_BLUE;
    }
    if (alpha)
    {
        mask |= D3DCOLORWRITEENABLE_ALPHA;
    }
    DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, mask);
}

void DX8Backend::Set_Alpha_Blend_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Alpha_Test_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Alpha_Test_Reference(unsigned ref)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAREF, ref);
}

void DX8Backend::Set_Alpha_Test_Function(CompareFunc func)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAFUNC, static_cast<unsigned>(func));
}

void DX8Backend::Set_Normalize_Normals(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_NORMALIZENORMALS, enable ? TRUE : FALSE);
}

void DX8Backend::Show_Hardware_Cursor(bool show)
{
    IDirect3DDevice8 * pDev = DX8Wrapper::_Get_D3D_Device8();
    if (pDev != nullptr)
    {
        pDev->ShowCursor(show ? TRUE : FALSE);
    }
}

void DX8Backend::Set_Hardware_Cursor_Image(int hotspot_x, int hotspot_y, const RenderBackendImage & image)
{
    IDirect3DDevice8 * pDev = DX8Wrapper::_Get_D3D_Device8();
    if (pDev != nullptr && image.Is_Valid())
    {
        SurfaceClass surface(image.Width, image.Height, image.Format);
        surface.Copy(image.Bytes.data(), image.Pitch);
        pDev->SetCursorProperties(
            static_cast<UINT>(hotspot_x),
            static_cast<UINT>(hotspot_y),
            Peek_Legacy_Surface(surface));
    }
}

void DX8Backend::Set_Hardware_Cursor_Position(int x, int y)
{
    IDirect3DDevice8 * pDev = DX8Wrapper::_Get_D3D_Device8();
    if (pDev != nullptr)
    {
        pDev->SetCursorPosition(x, y, D3DCURSOR_IMMEDIATE_UPDATE);
    }
}

void DX8Backend::Set_Stencil_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Stencil_Func(CompareFunc func)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILFUNC, static_cast<unsigned>(func));
}

void DX8Backend::Set_Stencil_Ref(unsigned int ref)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILREF, ref);
}

void DX8Backend::Set_Stencil_Mask(unsigned int mask)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILMASK, mask);
}

void DX8Backend::Set_Stencil_Write_Mask(unsigned int mask)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILWRITEMASK, mask);
}

void DX8Backend::Set_Stencil_Pass_Op(StencilOp op)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILPASS, static_cast<unsigned>(op));
}

void DX8Backend::Set_Stencil_Fail_Op(StencilOp op)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILFAIL, static_cast<unsigned>(op));
}

void DX8Backend::Set_Stencil_ZFail_Op(StencilOp op)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILZFAIL, static_cast<unsigned>(op));
}

void DX8Backend::Set_Z_Bias(int bias)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ZBIAS, static_cast<unsigned>(bias));
}

void DX8Backend::Set_Fill_Mode(FillMode mode)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_FILLMODE, static_cast<unsigned>(mode));
}

void DX8Backend::Set_Shade_Mode(ShadeMode mode)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_SHADEMODE, static_cast<unsigned>(mode));
}

void DX8Backend::Set_Depth_Test_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Depth_Write_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Depth_Func(CompareFunc func)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, static_cast<unsigned>(func));
}

unsigned DX8Backend::Get_Color_Write_Mask() const
{
    return DX8Wrapper::Get_DX8_Render_State(D3DRS_COLORWRITEENABLE);
}

bool DX8Backend::Supports_Color_Write_Mask() const
{
    if (!DX8Wrapper::Get_Current_Caps()) {
        return false;
    }
    return (DX8Wrapper::Get_Current_Caps()->Get_DX8_Caps().PrimitiveMiscCaps & D3DPMISCCAPS_COLORWRITEENABLE) != 0;
}

void DX8Backend::Set_Color_Write_Mask(unsigned mask)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, mask);
}

void DX8Backend::Set_Lighting_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_LIGHTING, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Point_Sprite_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_POINTSPRITEENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Point_Scale_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_POINTSCALEENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Point_Size(float size, float min_size, float max_size)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_POINTSIZE, FloatAsDword(size));
    DX8Wrapper::Set_DX8_Render_State(D3DRS_POINTSIZE_MIN, FloatAsDword(min_size));
    DX8Wrapper::Set_DX8_Render_State(D3DRS_POINTSIZE_MAX, FloatAsDword(max_size));
}

void DX8Backend::Set_Point_Scale(float a, float b, float c)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_POINTSCALE_A, FloatAsDword(a));
    DX8Wrapper::Set_DX8_Render_State(D3DRS_POINTSCALE_B, FloatAsDword(b));
    DX8Wrapper::Set_DX8_Render_State(D3DRS_POINTSCALE_C, FloatAsDword(c));
}

void DX8Backend::Set_Texture_Factor(unsigned argb)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR, argb);
}

void DX8Backend::Configure_Grayscale_Texture_Stages()
{
    if (Supports_Dot3())
    {
        DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR, kGrayscaleLuminanceWeights);
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLORARG0, D3DTA_TFACTOR | D3DTA_ALPHAREPLICATE);
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLORARG2, D3DTA_TFACTOR | D3DTA_ALPHAREPLICATE);
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);

        DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
        DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_COLOROP, D3DTOP_DOTPRODUCT3);
    }
    else
    {
        // Fallback for hardware without DOT3 blend support.
        DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR, kGrayscaleFlatGray);
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

        DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    }
}

void DX8Backend::Set_Texture_Transform(unsigned stage, const Matrix4x4 & matrix)
{
    DX8Wrapper::_Set_DX8_Transform(
        static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + stage),
        To_D3DMATRIX(matrix));
}

void DX8Backend::Set_Texture_Coord_Source(unsigned stage,
                                           RenderBackendTexcoordSource source,
                                           unsigned uv_array_index)
{
    unsigned tci = uv_array_index;
    switch (source)
    {
    case RB_TEXCOORD_MESH_UV:
        tci = D3DTSS_TCI_PASSTHRU | uv_array_index;
        break;
    case RB_TEXCOORD_CAMERA_SPACE_NORMAL:
        tci = D3DTSS_TCI_CAMERASPACENORMAL | uv_array_index;
        break;
    case RB_TEXCOORD_CAMERA_SPACE_REFLECTION:
        tci = D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR | uv_array_index;
        break;
    case RB_TEXCOORD_CAMERA_SPACE_POSITION:
        tci = D3DTSS_TCI_CAMERASPACEPOSITION | uv_array_index;
        break;
    }

    DX8Wrapper::Set_DX8_Texture_Stage_State(
        stage,
        D3DTSS_TEXCOORDINDEX,
        tci);
}

void DX8Backend::Set_Texture_Transform_Mode(unsigned stage, unsigned coord_count, bool projected)
{
    unsigned flags = coord_count == 0 ? D3DTTFF_DISABLE : coord_count;
    if (projected)
    {
        flags |= D3DTTFF_PROJECTED;
    }

    DX8Wrapper::Set_DX8_Texture_Stage_State(
        stage,
        D3DTSS_TEXTURETRANSFORMFLAGS,
        flags);
}

void DX8Backend::Set_Texture_UV_Wrap(unsigned stage, bool enable)
{
    if (stage >= RB_MAX_TEXTURE_STAGES)
    {
        return;
    }

    DX8Wrapper::Set_DX8_Render_State(
        static_cast<D3DRENDERSTATETYPE>(D3DRS_WRAP0 + stage),
        enable ? (D3DWRAP_U | D3DWRAP_V) : 0);
}

static D3DTEXTUREADDRESS TextureAddressModeToDX8(RenderBackendTextureAddressMode mode)
{
    switch (mode)
    {
        case RB_TEXTURE_ADDRESS_CLAMP:
            return D3DTADDRESS_CLAMP;
        case RB_TEXTURE_ADDRESS_BORDER:
            return D3DTADDRESS_BORDER;
        case RB_TEXTURE_ADDRESS_WRAP:
        default:
            return D3DTADDRESS_WRAP;
    }
}

void DX8Backend::Set_Texture_Address_Mode(unsigned stage,
                                          RenderBackendTextureAddressMode u,
                                          RenderBackendTextureAddressMode v,
                                          RenderBackendTextureAddressMode w)
{
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, TextureAddressModeToDX8(u));
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, TextureAddressModeToDX8(v));
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSW, TextureAddressModeToDX8(w));
}

static D3DTEXTUREFILTERTYPE TextureSampleFilterToDX8(RenderBackendTextureSampleFilter filter)
{
    switch (filter)
    {
        case RB_TEXTURE_SAMPLE_NONE:
            return D3DTEXF_NONE;
        case RB_TEXTURE_SAMPLE_POINT:
            return D3DTEXF_POINT;
        case RB_TEXTURE_SAMPLE_ANISOTROPIC:
            return D3DTEXF_ANISOTROPIC;
        case RB_TEXTURE_SAMPLE_LINEAR:
        default:
            return D3DTEXF_LINEAR;
    }
}

void DX8Backend::Set_Texture_Sample_Filter(unsigned stage,
                                           RenderBackendTextureSampleFilter min_filter,
                                           RenderBackendTextureSampleFilter mag_filter,
                                           RenderBackendTextureSampleFilter mip_filter)
{
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, TextureSampleFilterToDX8(min_filter));
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, TextureSampleFilterToDX8(mag_filter));
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MIPFILTER, TextureSampleFilterToDX8(mip_filter));
}

void DX8Backend::Set_Texture_Min_Mag_Filter(unsigned stage,
                                            RenderBackendTextureSampleFilter min_filter,
                                            RenderBackendTextureSampleFilter mag_filter)
{
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, TextureSampleFilterToDX8(min_filter));
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, TextureSampleFilterToDX8(mag_filter));
}

void DX8Backend::Set_Texture_Mip_Filter(unsigned stage, RenderBackendTextureSampleFilter mip_filter)
{
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MIPFILTER, TextureSampleFilterToDX8(mip_filter));
}

void DX8Backend::Set_Texture_Max_Anisotropy(unsigned stage, unsigned max_anisotropy)
{
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAXANISOTROPY, max_anisotropy);
}

void DX8Backend::Set_Texture_Bump_Env_Matrix(unsigned stage,
                                             float m00,
                                             float m01,
                                             float m10,
                                             float m11)
{
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_BUMPENVMAT00, FloatAsDword(m00));
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_BUMPENVMAT01, FloatAsDword(m01));
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_BUMPENVMAT10, FloatAsDword(m10));
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_BUMPENVMAT11, FloatAsDword(m11));
}

void DX8Backend::Set_Texture_Bump_Env_Luminance(unsigned stage,
                                                float scale,
                                                float offset)
{
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_BUMPENVLSCALE, FloatAsDword(scale));
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_BUMPENVLOFFSET, FloatAsDword(offset));
}

void DX8Backend::Set_Texture_Color_Operation(unsigned stage, RenderBackendTextureOperation op)
{
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_COLOROP, static_cast<DWORD>(op));
}

void DX8Backend::Set_Texture_Alpha_Operation(unsigned stage, RenderBackendTextureOperation op)
{
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ALPHAOP, static_cast<DWORD>(op));
}

void DX8Backend::Set_Texture_Color_Argument(unsigned stage,
                                            unsigned argument_index,
                                            RenderBackendTextureArgument arg)
{
    static const D3DTEXTURESTAGESTATETYPE states[] = {
        D3DTSS_COLORARG0,
        D3DTSS_COLORARG1,
        D3DTSS_COLORARG2,
    };
    if (argument_index >= sizeof(states) / sizeof(states[0]))
    {
        return;
    }

    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, states[argument_index], static_cast<DWORD>(arg));
}

void DX8Backend::Set_Texture_Alpha_Argument(unsigned stage,
                                            unsigned argument_index,
                                            RenderBackendTextureArgument arg)
{
    static const D3DTEXTURESTAGESTATETYPE states[] = {
        D3DTSS_ALPHAARG0,
        D3DTSS_ALPHAARG1,
        D3DTSS_ALPHAARG2,
    };
    if (argument_index >= sizeof(states) / sizeof(states[0]))
    {
        return;
    }

    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, states[argument_index], static_cast<DWORD>(arg));
}

void DX8Backend::Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value)
{
    DX8Wrapper::Set_DX8_Texture_Stage_State(
        stage,
        static_cast<D3DTEXTURESTAGESTATETYPE>(state),
        value);
}

void DX8Backend::Configure_Custom_Edging_Cloud_Texture_Stages()
{
    Set_Texture_Stage_State(0, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
    Set_Texture_Stage_State(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

    Set_Texture_Stage_State(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
    Set_Texture_Stage_State(1, D3DTSS_COLORARG2, D3DTA_TEXTURE);
    Set_Texture_Stage_State(1, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    Set_Texture_Stage_State(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
    Set_Texture_Stage_State(1, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
    Set_Texture_Stage_State(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    Set_Texture_Stage_State(1, D3DTSS_TEXCOORDINDEX, 1);
}

void DX8Backend::Configure_Shadow_Volume_Fill_Texture_Stages()
{
    Set_Texture_Stage_State(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    Set_Texture_Stage_State(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    Set_Texture_Stage_State(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
    Set_Texture_Stage_State(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    Set_Texture_Stage_State(0, D3DTSS_TEXCOORDINDEX, 0);

    Set_Texture_Stage_State(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    Set_Texture_Stage_State(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    Set_Texture_Stage_State(1, D3DTSS_TEXCOORDINDEX, 1);
}

CullMode DX8Backend::Get_Cull_Mode() const
{
    return static_cast<CullMode>(DX8Wrapper::Get_DX8_Render_State(D3DRS_CULLMODE));
}

void DX8Backend::Set_Cull_Mode(CullMode mode)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_CULLMODE, static_cast<unsigned>(mode));
}

// TheSuperHackers @bugfix bobtista 01/06/2026 Forward Override_* state
// overrides 1:1 to the legacy DX8Wrapper render-state calls. See header
// for the failure mode they fix.
void DX8Backend::Override_Blend(BlendFactor srcBlend, BlendFactor dstBlend)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,  static_cast<unsigned>(srcBlend));
    DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, static_cast<unsigned>(dstBlend));
}

void DX8Backend::Override_Alpha_Test(bool enable, unsigned ref, CompareFunc func)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, enable ? TRUE : FALSE);
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAREF,        ref);
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAFUNC,       static_cast<unsigned>(func));
}

void DX8Backend::Override_Alpha_Blend_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Override_Texcoord_Index(unsigned stage, unsigned uvIndex)
{
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, uvIndex);
}

// -- Transforms --------------------------------------------------------------

void DX8Backend::Set_Transform(TransformKind transform, const Matrix4x4 & m)
{
    DX8Wrapper::Set_Transform(static_cast<D3DTRANSFORMSTATETYPE>(transform), m);
}

void DX8Backend::Set_Transform(TransformKind transform, const Matrix3D & m)
{
    DX8Wrapper::Set_Transform(static_cast<D3DTRANSFORMSTATETYPE>(transform), m);
}

void DX8Backend::Get_Transform(TransformKind transform, Matrix4x4 & m) const
{
    DX8Wrapper::Get_Transform(static_cast<D3DTRANSFORMSTATETYPE>(transform), m);
}

void DX8Backend::Set_World_Identity()
{
    DX8Wrapper::Set_World_Identity();
}

void DX8Backend::Set_View_Identity()
{
    DX8Wrapper::Set_View_Identity();
}

bool DX8Backend::Is_World_Identity() const
{
    return DX8Wrapper::Is_World_Identity();
}

bool DX8Backend::Is_View_Identity() const
{
    return DX8Wrapper::Is_View_Identity();
}

void DX8Backend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar)
{
    DX8Wrapper::Set_Projection_Transform_With_Z_Bias(matrix, znear, zfar);
}

// -- Lighting and fog --------------------------------------------------------

void DX8Backend::Set_Light(unsigned int index, const LightClass & light)
{
    DX8Wrapper::Set_Light(index, light);
}

void DX8Backend::Clear_Light(unsigned int index)
{
    DX8Wrapper::Set_Light(index, nullptr);
}

void DX8Backend::Set_Ambient(const Vector3 & color)
{
    DX8Wrapper::Set_Ambient(color);
}

const Vector3 & DX8Backend::Get_Ambient() const
{
    return DX8Wrapper::Get_Ambient();
}

void DX8Backend::Set_Fog(bool enable, const Vector3 & color, float start, float end)
{
    DX8Wrapper::Set_Fog(enable, color, start, end);
}

void DX8Backend::Set_Fog_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Fog_Color(unsigned argb)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGCOLOR, argb);
}

unsigned DX8Backend::Get_Fog_Color() const
{
    return DX8Wrapper::Get_Fog_Color();
}

// TheSuperHackers @bugfix bobtista 02/06/2026 Additional forwarders for
// the override calls W3DWater makes around its batched water draws (commit
// 0dc6548f2). Override_Alpha_Blend_Enable is already implemented above as
// part of the Override_* set; Override_Material_Opacity and Clear_State_Overrides
// are unique to the water batched draw and stay defensive.
//   Override_Material_Opacity: D3DRS_TEXTUREFACTOR alpha
//   Clear_State_Overrides:     TFACTOR alpha back to 1.0
void DX8Backend::Override_Material_Opacity(float opacity)
{
    unsigned a = static_cast<unsigned>(opacity * 255.0f);
    if (a > 255) { a = 255; }
    DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR, (a << 24) | 0x00ffffff);
}

void DX8Backend::Clear_State_Overrides()
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR, 0xffffffff);
}

void DX8Backend::Apply_Stencil_Shadow_Darken(unsigned shadow_color,
                                             unsigned stencil_read_mask,
                                             unsigned stencil_ref,
                                             int x,
                                             int y,
                                             int width,
                                             int height)
{
    LPDIRECT3DDEVICE8 device = DX8Wrapper::_Get_D3D_Device8();
    if (device == nullptr)
    {
        return;
    }

    struct TranslitVertex
    {
        float x;
        float y;
        float z;
        float w;
        DWORD color;
    } vertices[4];

    vertices[0].x = x + width; vertices[0].y = y + height; vertices[0].z = 0.0f; vertices[0].w = 1.0f;
    vertices[1].x = x + width; vertices[1].y = (float)y;   vertices[1].z = 0.0f; vertices[1].w = 1.0f;
    vertices[2].x = x;         vertices[2].y = y + height; vertices[2].z = 0.0f; vertices[2].w = 1.0f;
    vertices[3].x = x;         vertices[3].y = (float)y;   vertices[3].z = 0.0f; vertices[3].w = 1.0f;

    vertices[0].color = shadow_color;
    vertices[1].color = shadow_color;
    vertices[2].color = shadow_color;
    vertices[3].color = shadow_color;

    device->SetVertexShader(DX8_FVF_FLAG_XYZRHW | DX8_FVF_FLAG_DIFFUSE);

    Set_Alpha_Blend_Enable(true);
    Set_Blend_Factors(RB_BLEND_DEST_COLOR, RB_BLEND_ZERO);
    Set_Depth_Test_Enable(true);
    Set_Depth_Func(RB_CMP_ALWAYS);
    Set_Stencil_Enable(true);
    Set_Stencil_Func(RB_CMP_LESS_EQUAL);
    Set_Stencil_Pass_Op(RB_STENCIL_OP_KEEP);
    Set_Stencil_Mask(stencil_read_mask);
    Set_Stencil_Ref(stencil_ref);
    Set_Shade_Mode(RB_SHADE_FLAT);

    if (DX8Wrapper::_Is_Triangle_Draw_Enabled())
    {
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(TranslitVertex));
    }
}

bool DX8Backend::Get_Fog_Enable() const
{
    return DX8Wrapper::Get_Fog_Enable();
}

void DX8Backend::Set_Specular_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_SPECULARENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Patch_Segments(float level)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_PATCHSEGMENTS, FloatAsDword(level));
}

void DX8Backend::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    DX8Wrapper::Set_Light_Environment(light_env);
}

LightEnvironmentClass * DX8Backend::Get_Light_Environment() const
{
    return DX8Wrapper::Get_Light_Environment();
}

// -- Draw calls --------------------------------------------------------------

void DX8Backend::Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count)
{
    DX8Wrapper::Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);
}

void DX8Backend::Draw_Triangles(unsigned int buffer_type,
                                unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count)
{
    DX8Wrapper::Draw_Triangles(buffer_type, start_index, polygon_count, min_vertex_index, vertex_count);
}

bool DX8Backend::Is_Triangle_Draw_Enabled() const
{
    return DX8Wrapper::_Is_Triangle_Draw_Enabled();
}

void DX8Backend::Set_Triangle_Draw_Enabled(bool enable)
{
    DX8Wrapper::_Enable_Triangle_Draw(enable);
}

void DX8Backend::Draw_Screen_Color_Quad(unsigned color, int x, int y, int width, int height)
{
    LPDIRECT3DDEVICE8 device = DX8Wrapper::_Get_D3D_Device8();
    if (device == nullptr || !DX8Wrapper::_Is_Triangle_Draw_Enabled())
    {
        return;
    }

    struct TranslitVertex
    {
        float x;
        float y;
        float z;
        float w;
        DWORD color;
    } vertices[4];

    vertices[0].x = x + width; vertices[0].y = y + height; vertices[0].z = 0.0f; vertices[0].w = 1.0f;
    vertices[1].x = x + width; vertices[1].y = (float)y;   vertices[1].z = 0.0f; vertices[1].w = 1.0f;
    vertices[2].x = x;         vertices[2].y = y + height; vertices[2].z = 0.0f; vertices[2].w = 1.0f;
    vertices[3].x = x;         vertices[3].y = (float)y;   vertices[3].z = 0.0f; vertices[3].w = 1.0f;

    vertices[0].color = color;
    vertices[1].color = color;
    vertices[2].color = color;
    vertices[3].color = color;

    device->SetVertexShader(DX8_FVF_FLAG_XYZRHW | DX8_FVF_FLAG_DIFFUSE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(TranslitVertex));
}

void DX8Backend::Draw_Strip(unsigned short start_index,
                            unsigned short index_count,
                            unsigned short min_vertex_index,
                            unsigned short vertex_count)
{
    DX8Wrapper::Draw_Strip(start_index, index_count, min_vertex_index, vertex_count);
}

// -- Programmable pipeline ---------------------------------------------------

const unsigned int * DX8Backend::Get_Legacy_Vertex_Shader_Declaration(
    RenderBackendLegacyVertexDeclaration declaration) const
{
    switch (declaration) {
        case RB_LEGACY_VERTEX_DECL_XYZNDUV1:
            return reinterpret_cast<const unsigned int *>(GetXYZNDUV1Declaration());
    }

    return nullptr;
}

bool DX8Backend::Create_Vertex_Shader(const unsigned int * declaration,
                                      const unsigned int * shader,
                                      unsigned int usage,
                                      unsigned long * handle)
{
    if (handle == nullptr) {
        return false;
    }

    DWORD dx_handle = 0;
    HRESULT hr = DX8Wrapper::_Get_D3D_Device8()->CreateVertexShader(
        reinterpret_cast<const DWORD *>(declaration),
        reinterpret_cast<const DWORD *>(shader),
        &dx_handle,
        static_cast<DWORD>(usage));
    if (FAILED(hr)) {
        return false;
    }

    *handle = static_cast<unsigned long>(dx_handle);
    return true;
}

bool DX8Backend::Create_Pixel_Shader(const unsigned int * shader,
                                     unsigned long * handle)
{
    if (handle == nullptr) {
        return false;
    }

    DWORD dx_handle = 0;
    HRESULT hr = DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader(
        reinterpret_cast<const DWORD *>(shader),
        &dx_handle);
    if (FAILED(hr)) {
        return false;
    }

    *handle = static_cast<unsigned long>(dx_handle);
    return true;
}

void DX8Backend::Delete_Vertex_Shader(unsigned long vertex_shader)
{
    DX8Wrapper::_Get_D3D_Device8()->DeleteVertexShader(static_cast<DWORD>(vertex_shader));
}

void DX8Backend::Delete_Pixel_Shader(unsigned long pixel_shader)
{
    DX8Wrapper::_Get_D3D_Device8()->DeletePixelShader(static_cast<DWORD>(pixel_shader));
}

void DX8Backend::Set_Vertex_Shader(unsigned long vertex_shader)
{
    DX8Wrapper::Set_Vertex_Shader(static_cast<DWORD>(vertex_shader));
}

void DX8Backend::Set_Pixel_Shader(unsigned long pixel_shader)
{
    DX8Wrapper::Set_Pixel_Shader(static_cast<DWORD>(pixel_shader));
}

void DX8Backend::Set_Vertex_Shader_Constant(int reg, const void * data, int count)
{
    DX8Wrapper::Set_Vertex_Shader_Constant(reg, data, count);
}

void DX8Backend::Set_Pixel_Shader_Constant(int reg, const void * data, int count)
{
    DX8Wrapper::Set_Pixel_Shader_Constant(reg, data, count);
}

// -- Render targets ----------------------------------------------------------

TextureClass * DX8Backend::Create_Render_Target(int width, int height, WW3DFormat format)
{
    TextureClass * tex = DX8Wrapper::Create_Render_Target(width, height, format);
    return tex;
}

void DX8Backend::Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture)
{
    DX8Wrapper::Set_Render_Target_With_Z(texture, ztexture);
}

bool DX8Backend::Is_Render_To_Texture() const
{
    return DX8Wrapper::Is_Render_To_Texture();
}

void DX8Backend::Set_Shadow_Map(int idx, ZTextureClass * ztex)
{
    DX8Wrapper::Set_Shadow_Map(idx, ztex);
}

ZTextureClass * DX8Backend::Get_Shadow_Map(int idx) const
{
    return DX8Wrapper::Get_Shadow_Map(idx);
}

// -- Resource creation (asset ingress) -------------------------------
//
// RenderResource.id encoding for DX8Backend: the raw IDirect3D*8 pointer
// cast to uint64. This keeps the handle trivially invertible back to the
// D3D8 resource type for any DX8-specific code that needs it, and lets
// the BgfxBackend ref-popup mirror path look up the D3D8 pointer from the
// returned handle without any side table.

RenderResource DX8Backend::Create_Texture(const TextureDesc & desc)
{
    const MipCountType mip_count = static_cast<MipCountType>(desc.mip_count);
    IDirect3DTexture8 * tex = DX8Wrapper::_Create_DX8_Texture(
        desc.width, desc.height, desc.format, mip_count,
        D3DPOOL_MANAGED, desc.is_render_target);

    if (tex != nullptr && !desc.is_render_target && desc.mips != nullptr) {
        for (unsigned char level = 0; level < desc.mip_count; ++level) {
            const MipSlice & slice = desc.mips[level];
            if (slice.data == nullptr || slice.size_bytes == 0) {
                continue;
            }
            D3DLOCKED_RECT locked;
            if (SUCCEEDED(tex->LockRect(level, &locked, nullptr, 0))) {
                if (slice.pitch != 0 && static_cast<unsigned>(locked.Pitch) == slice.pitch) {
                    memcpy(locked.pBits, slice.data, slice.size_bytes);
                } else if (slice.pitch != 0) {
                    const unsigned rows = slice.size_bytes / slice.pitch;
                    for (unsigned row = 0; row < rows; ++row) {
                        memcpy(
                            static_cast<unsigned char *>(locked.pBits) + row * locked.Pitch,
                            static_cast<const unsigned char *>(slice.data) + row * slice.pitch,
                            slice.pitch);
                    }
                } else {
                    // Compressed: the caller's size_bytes already accounts
                    // for block-compressed row packing.
                    memcpy(locked.pBits, slice.data, slice.size_bytes);
                }
                tex->UnlockRect(level);
            }
        }
    }

    RenderResource rr;
    rr.id = reinterpret_cast<std::uint64_t>(tex);
    return rr;
}

RenderResource DX8Backend::Create_Vertex_Buffer(const BufferDesc & desc, const void * initial_data)
{
    IDirect3DVertexBuffer8 * vb = nullptr;
    const DWORD usage = desc.dynamic ? (D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY) : D3DUSAGE_WRITEONLY;
    const D3DPOOL pool = desc.dynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
    DX8Wrapper::_Get_D3D_Device8()->CreateVertexBuffer(
        desc.size_bytes, usage, desc.layout.fvf, pool, &vb);

    if (vb != nullptr && initial_data != nullptr)
    {
        unsigned char * dst = nullptr;
        if (SUCCEEDED(vb->Lock(0, desc.size_bytes, &dst, 0)))
        {
            memcpy(dst, initial_data, desc.size_bytes);
            vb->Unlock();
        }
    }

    RenderResource rr;
    rr.id = reinterpret_cast<std::uint64_t>(vb);
    return rr;
}

RenderResource DX8Backend::Create_Index_Buffer(const BufferDesc & desc, const void * initial_data, bool indices_are_32bit)
{
    IDirect3DIndexBuffer8 * ib = nullptr;
    const DWORD usage = desc.dynamic ? (D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY) : D3DUSAGE_WRITEONLY;
    const D3DPOOL pool = desc.dynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
    const D3DFORMAT fmt = indices_are_32bit ? D3DFMT_INDEX32 : D3DFMT_INDEX16;
    DX8Wrapper::_Get_D3D_Device8()->CreateIndexBuffer(
        desc.size_bytes, usage, fmt, pool, &ib);

    if (ib != nullptr && initial_data != nullptr)
    {
        unsigned char * dst = nullptr;
        if (SUCCEEDED(ib->Lock(0, desc.size_bytes, &dst, 0)))
        {
            memcpy(dst, initial_data, desc.size_bytes);
            ib->Unlock();
        }
    }

    RenderResource rr;
    rr.id = reinterpret_cast<std::uint64_t>(ib);
    return rr;
}

void DX8Backend::Destroy_Resource(RenderResource h)
{
    IUnknown * obj = reinterpret_cast<IUnknown *>(h.id);
    if (obj != nullptr)
    {
        obj->Release();
    }
}

// Transitional: the legacy DX8 path already owns these
// resources through TextureBaseClass / DX8VertexBufferClass /
// DX8IndexBufferClass. DX8Backend must stay a passive forwarding adapter,
// so registered legacy resources do not get an owning RenderResource.

RenderResource DX8Backend::Register_Texture_Resource(TextureBaseClass * /*tex*/)
{
    return kInvalidRenderResource;
}

RenderResource DX8Backend::Register_Vertex_Buffer_Resource(VertexBufferClass * /*vb*/)
{
    return kInvalidRenderResource;
}

RenderResource DX8Backend::Register_Index_Buffer_Resource(IndexBufferClass * /*ib*/)
{
    return kInvalidRenderResource;
}
