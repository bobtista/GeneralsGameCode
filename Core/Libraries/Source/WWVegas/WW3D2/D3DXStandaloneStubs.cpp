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
*/

// TheSuperHackers @refactor bobtista 22/04/2026 Stage 5 —
// Minimal in-tree implementations of the D3DX8 helper functions the
// engine actually calls, so the standalone bgfx build can drop the
// d3dx8.lib static-link dependency entirely. This file only compiles
// under GGC_BGFX_STANDALONE; in ref-popup mode d3dx8.lib continues to
// provide these symbols.
//
// Not implemented here:
// - D3DXMatrixIdentity / D3DXMatrixIsIdentity — these are D3DXINLINE
//   in d3dx8math.inl, so every translation unit already has its own
//   copy; redefining them in a .cpp produces a type-modifier mismatch.
// - Non-critical functions (shader assemble from file/resource, mesh
//   loaders, font create, etc.) that aren't called at runtime.

#if defined(GGC_BGFX_STANDALONE)

#include <cmath>
#include <cstring>
#include <cstdint>

#include <d3d8.h>
#include <d3dx8.h>

#include "wwdebug.h"

// -----------------------------------------------------------------------------
// Matrix helpers (the non-inline ones; D3DXMatrixIdentity is inlined in
// d3dx8math.inl).
// -----------------------------------------------------------------------------

extern "C" D3DXMATRIX * WINAPI D3DXMatrixTranspose(D3DXMATRIX * out, CONST D3DXMATRIX * in)
{
	if (out == nullptr || in == nullptr) return out;
	D3DXMATRIX tmp;
	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c)
			tmp.m[c][r] = in->m[r][c];
	*out = tmp;
	return out;
}

extern "C" D3DXMATRIX * WINAPI D3DXMatrixMultiply(D3DXMATRIX * out, CONST D3DXMATRIX * a, CONST D3DXMATRIX * b)
{
	if (out == nullptr || a == nullptr || b == nullptr) return out;
	D3DXMATRIX tmp;
	for (int r = 0; r < 4; ++r)
	{
		for (int c = 0; c < 4; ++c)
		{
			tmp.m[r][c] =
				a->m[r][0] * b->m[0][c] +
				a->m[r][1] * b->m[1][c] +
				a->m[r][2] * b->m[2][c] +
				a->m[r][3] * b->m[3][c];
		}
	}
	*out = tmp;
	return out;
}

extern "C" D3DXMATRIX * WINAPI D3DXMatrixScaling(D3DXMATRIX * out, FLOAT sx, FLOAT sy, FLOAT sz)
{
	if (out == nullptr) return nullptr;
	std::memset(out, 0, sizeof(*out));
	out->_11 = sx;
	out->_22 = sy;
	out->_33 = sz;
	out->_44 = 1.0f;
	return out;
}

extern "C" D3DXMATRIX * WINAPI D3DXMatrixTranslation(D3DXMATRIX * out, FLOAT x, FLOAT y, FLOAT z)
{
	if (out == nullptr) return nullptr;
	std::memset(out, 0, sizeof(*out));
	out->_11 = out->_22 = out->_33 = out->_44 = 1.0f;
	out->_41 = x;
	out->_42 = y;
	out->_43 = z;
	return out;
}

extern "C" D3DXMATRIX * WINAPI D3DXMatrixRotationZ(D3DXMATRIX * out, FLOAT angle)
{
	if (out == nullptr) return nullptr;
	std::memset(out, 0, sizeof(*out));
	const FLOAT c = std::cos(angle);
	const FLOAT s = std::sin(angle);
	out->_11 =  c;  out->_12 =  s;
	out->_21 = -s;  out->_22 =  c;
	out->_33 = 1.0f;
	out->_44 = 1.0f;
	return out;
}

// 4x4 inverse via cofactor expansion. Returns out on success, nullptr
// when the matrix is singular. D3DX writes the determinant into
// pDeterminant when non-null.
extern "C" D3DXMATRIX * WINAPI D3DXMatrixInverse(D3DXMATRIX * out, FLOAT * det_out, CONST D3DXMATRIX * in)
{
	if (out == nullptr || in == nullptr) return nullptr;
	const FLOAT * m = &in->m[0][0];
	FLOAT inv[16];
	inv[ 0] =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
	         + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
	inv[ 4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
	         - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
	inv[ 8] =  m[4]*m[ 9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
	         + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[ 9];
	inv[12] = -m[4]*m[ 9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
	         - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[ 9];
	inv[ 1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
	         - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
	inv[ 5] =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
	         + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
	inv[ 9] = -m[0]*m[ 9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
	         - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[ 9];
	inv[13] =  m[0]*m[ 9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
	         + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[ 9];
	inv[ 2] =  m[1]*m[ 6]*m[15] - m[1]*m[ 7]*m[14] - m[5]*m[2]*m[15]
	         + m[5]*m[3]*m[14] + m[13]*m[2]*m[ 7] - m[13]*m[3]*m[ 6];
	inv[ 6] = -m[0]*m[ 6]*m[15] + m[0]*m[ 7]*m[14] + m[4]*m[2]*m[15]
	         - m[4]*m[3]*m[14] - m[12]*m[2]*m[ 7] + m[12]*m[3]*m[ 6];
	inv[10] =  m[0]*m[ 5]*m[15] - m[0]*m[ 7]*m[13] - m[4]*m[1]*m[15]
	         + m[4]*m[3]*m[13] + m[12]*m[1]*m[ 7] - m[12]*m[3]*m[ 5];
	inv[14] = -m[0]*m[ 5]*m[14] + m[0]*m[ 6]*m[13] + m[4]*m[1]*m[14]
	         - m[4]*m[2]*m[13] - m[12]*m[1]*m[ 6] + m[12]*m[2]*m[ 5];
	inv[ 3] = -m[1]*m[ 6]*m[11] + m[1]*m[ 7]*m[10] + m[5]*m[2]*m[11]
	         - m[5]*m[3]*m[10] - m[ 9]*m[2]*m[ 7] + m[ 9]*m[3]*m[ 6];
	inv[ 7] =  m[0]*m[ 6]*m[11] - m[0]*m[ 7]*m[10] - m[4]*m[2]*m[11]
	         + m[4]*m[3]*m[10] + m[ 8]*m[2]*m[ 7] - m[ 8]*m[3]*m[ 6];
	inv[11] = -m[0]*m[ 5]*m[11] + m[0]*m[ 7]*m[ 9] + m[4]*m[1]*m[11]
	         - m[4]*m[3]*m[ 9] - m[ 8]*m[1]*m[ 7] + m[ 8]*m[3]*m[ 5];
	inv[15] =  m[0]*m[ 5]*m[10] - m[0]*m[ 6]*m[ 9] - m[4]*m[1]*m[10]
	         + m[4]*m[2]*m[ 9] + m[ 8]*m[1]*m[ 6] - m[ 8]*m[2]*m[ 5];

	FLOAT d = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
	if (det_out != nullptr) *det_out = d;
	if (d == 0.0f) return nullptr;
	const FLOAT inv_d = 1.0f / d;
	FLOAT * out_m = &out->m[0][0];
	for (int i = 0; i < 16; ++i) out_m[i] = inv[i] * inv_d;
	return out;
}

// -----------------------------------------------------------------------------
// Vector transforms — full 4x4 multiply. D3DXVec3Transform expands v to
// (x,y,z,1) and writes a Vec4; D3DXVec4Transform is a straight Vec4
// times matrix. Both callers use these for picking/bezier math.
// -----------------------------------------------------------------------------

extern "C" D3DXVECTOR4 * WINAPI D3DXVec3Transform(D3DXVECTOR4 * out, CONST D3DXVECTOR3 * v, CONST D3DXMATRIX * m)
{
	if (out == nullptr || v == nullptr || m == nullptr) return out;
	D3DXVECTOR4 tmp;
	tmp.x = v->x * m->_11 + v->y * m->_21 + v->z * m->_31 + m->_41;
	tmp.y = v->x * m->_12 + v->y * m->_22 + v->z * m->_32 + m->_42;
	tmp.z = v->x * m->_13 + v->y * m->_23 + v->z * m->_33 + m->_43;
	tmp.w = v->x * m->_14 + v->y * m->_24 + v->z * m->_34 + m->_44;
	*out = tmp;
	return out;
}

extern "C" D3DXVECTOR4 * WINAPI D3DXVec4Transform(D3DXVECTOR4 * out, CONST D3DXVECTOR4 * v, CONST D3DXMATRIX * m)
{
	if (out == nullptr || v == nullptr || m == nullptr) return out;
	D3DXVECTOR4 tmp;
	tmp.x = v->x * m->_11 + v->y * m->_21 + v->z * m->_31 + v->w * m->_41;
	tmp.y = v->x * m->_12 + v->y * m->_22 + v->z * m->_32 + v->w * m->_42;
	tmp.z = v->x * m->_13 + v->y * m->_23 + v->z * m->_33 + v->w * m->_43;
	tmp.w = v->x * m->_14 + v->y * m->_24 + v->z * m->_34 + v->w * m->_44;
	*out = tmp;
	return out;
}

// -----------------------------------------------------------------------------
// FVF helpers
// -----------------------------------------------------------------------------

extern "C" UINT WINAPI D3DXGetFVFVertexSize(DWORD fvf)
{
	UINT size = 0;
	switch (fvf & D3DFVF_POSITION_MASK)
	{
	case D3DFVF_XYZ:    size += 3 * sizeof(float); break;
	case D3DFVF_XYZRHW: size += 4 * sizeof(float); break;
	case D3DFVF_XYZB1:  size += 3 * sizeof(float) + 1 * sizeof(float); break;
	case D3DFVF_XYZB2:  size += 3 * sizeof(float) + 2 * sizeof(float); break;
	case D3DFVF_XYZB3:  size += 3 * sizeof(float) + 3 * sizeof(float); break;
	case D3DFVF_XYZB4:  size += 3 * sizeof(float) + 4 * sizeof(float); break;
	case D3DFVF_XYZB5:  size += 3 * sizeof(float) + 5 * sizeof(float); break;
	default: break;
	}
	if (fvf & D3DFVF_NORMAL)   size += 3 * sizeof(float);
	if (fvf & D3DFVF_PSIZE)    size += sizeof(float);
	if (fvf & D3DFVF_DIFFUSE)  size += sizeof(DWORD);
	if (fvf & D3DFVF_SPECULAR) size += sizeof(DWORD);
	const DWORD tex_count = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	// Default 2 floats per set — D3DFVF_TEXCOORDSIZE1..4 overrides aren't
	// used anywhere in the engine.
	size += tex_count * 2 * sizeof(float);
	return size;
}

// -----------------------------------------------------------------------------
// Error strings — minimal static table. Real D3DX has a big message pool
// keyed by HRESULT; we just need enough for debug logs.
// -----------------------------------------------------------------------------

extern "C" HRESULT WINAPI D3DXGetErrorStringA(HRESULT hr, LPSTR pBuffer, UINT BufferLen)
{
	if (pBuffer == nullptr || BufferLen == 0) return D3D_OK;
	const char * msg;
	switch (static_cast<UINT>(hr))
	{
	case (UINT)D3D_OK:                   msg = "D3D_OK"; break;
	case (UINT)D3DERR_INVALIDCALL:       msg = "D3DERR_INVALIDCALL"; break;
	case (UINT)D3DERR_NOTAVAILABLE:      msg = "D3DERR_NOTAVAILABLE"; break;
	case (UINT)D3DERR_OUTOFVIDEOMEMORY:  msg = "D3DERR_OUTOFVIDEOMEMORY"; break;
	case (UINT)E_OUTOFMEMORY:            msg = "E_OUTOFMEMORY"; break;
	case (UINT)E_NOTIMPL:                msg = "E_NOTIMPL"; break;
	case (UINT)E_FAIL:                   msg = "E_FAIL"; break;
	default:                              msg = "D3DX unknown error"; break;
	}
	std::strncpy(pBuffer, msg, BufferLen - 1);
	pBuffer[BufferLen - 1] = '\0';
	return D3D_OK;
}

// -----------------------------------------------------------------------------
// Texture / surface helpers
// -----------------------------------------------------------------------------

extern "C" HRESULT WINAPI D3DXCreateTexture(
	LPDIRECT3DDEVICE8 device,
	UINT width,
	UINT height,
	UINT mip_levels,
	DWORD usage,
	D3DFORMAT format,
	D3DPOOL pool,
	LPDIRECT3DTEXTURE8 * out_texture)
{
	if (device == nullptr || out_texture == nullptr) return E_POINTER;
	if (mip_levels == D3DX_DEFAULT) mip_levels = 0;
	if (format == D3DFMT_UNKNOWN) format = D3DFMT_A8R8G8B8;
	return device->CreateTexture(width, height, mip_levels, usage, format, pool, out_texture);
}

extern "C" HRESULT WINAPI D3DXCreateCubeTexture(
	LPDIRECT3DDEVICE8 device,
	UINT size,
	UINT mip_levels,
	DWORD usage,
	D3DFORMAT format,
	D3DPOOL pool,
	LPDIRECT3DCUBETEXTURE8 * out_texture)
{
	if (device == nullptr || out_texture == nullptr) return E_POINTER;
	if (mip_levels == D3DX_DEFAULT) mip_levels = 0;
	if (format == D3DFMT_UNKNOWN) format = D3DFMT_A8R8G8B8;
	return device->CreateCubeTexture(size, mip_levels, usage, format, pool, out_texture);
}

extern "C" HRESULT WINAPI D3DXCreateVolumeTexture(
	LPDIRECT3DDEVICE8 device,
	UINT width,
	UINT height,
	UINT depth,
	UINT mip_levels,
	DWORD usage,
	D3DFORMAT format,
	D3DPOOL pool,
	LPDIRECT3DVOLUMETEXTURE8 * out_texture)
{
	if (device == nullptr || out_texture == nullptr) return E_POINTER;
	if (mip_levels == D3DX_DEFAULT) mip_levels = 0;
	if (format == D3DFMT_UNKNOWN) format = D3DFMT_A8R8G8B8;
	return device->CreateVolumeTexture(width, height, depth, mip_levels, usage, format, pool, out_texture);
}

// Box-filter mip generator. Walks 2D textures level 1..N-1 and fills each
// by averaging the previous level's 2x2 quads. Implemented per-format so
// packed 16-bit formats (A1R5G5B5, R5G6B5, A4R4G4B4) get correct channel
// averaging instead of byte-wise garbage.
//
// TheSuperHackers @bugfix bobtista 22/04/2026 Terrain textures
// use D3DFMT_A1R5G5B5 (2 bpp). The previous 32bpp-only implementation
// both (a) read at a 32bpp stride that did not match what the stub now
// reports for 16bpp surfaces, and (b) averaged bytes as if they were
// separate channels, which silently shredded the R/G/B/A bitfields.
static inline uint16_t Filter_A1R5G5B5_4(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
	// A:1, R:5, G:5, B:5  (bits 15,14..10,9..5,4..0)
	const UINT aA = (a >> 15) & 0x1, aR = (a >> 10) & 0x1F, aG = (a >> 5) & 0x1F, aB = a & 0x1F;
	const UINT bA = (b >> 15) & 0x1, bR = (b >> 10) & 0x1F, bG = (b >> 5) & 0x1F, bB = b & 0x1F;
	const UINT cA = (c >> 15) & 0x1, cR = (c >> 10) & 0x1F, cG = (c >> 5) & 0x1F, cB = c & 0x1F;
	const UINT dA = (d >> 15) & 0x1, dR = (d >> 10) & 0x1F, dG = (d >> 5) & 0x1F, dB = d & 0x1F;
	const UINT mA = (aA + bA + cA + dA) >= 2 ? 1 : 0;
	const UINT mR = (aR + bR + cR + dR + 2) >> 2;
	const UINT mG = (aG + bG + cG + dG + 2) >> 2;
	const UINT mB = (aB + bB + cB + dB + 2) >> 2;
	return static_cast<uint16_t>((mA << 15) | (mR << 10) | (mG << 5) | mB);
}

static inline uint16_t Filter_R5G6B5_4(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
	const UINT aR = (a >> 11) & 0x1F, aG = (a >> 5) & 0x3F, aB = a & 0x1F;
	const UINT bR = (b >> 11) & 0x1F, bG = (b >> 5) & 0x3F, bB = b & 0x1F;
	const UINT cR = (c >> 11) & 0x1F, cG = (c >> 5) & 0x3F, cB = c & 0x1F;
	const UINT dR = (d >> 11) & 0x1F, dG = (d >> 5) & 0x3F, dB = d & 0x1F;
	const UINT mR = (aR + bR + cR + dR + 2) >> 2;
	const UINT mG = (aG + bG + cG + dG + 2) >> 2;
	const UINT mB = (aB + bB + cB + dB + 2) >> 2;
	return static_cast<uint16_t>((mR << 11) | (mG << 5) | mB);
}

static inline uint16_t Filter_A4R4G4B4_4(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
	const UINT aA = (a >> 12) & 0xF, aR = (a >> 8) & 0xF, aG = (a >> 4) & 0xF, aB = a & 0xF;
	const UINT bA = (b >> 12) & 0xF, bR = (b >> 8) & 0xF, bG = (b >> 4) & 0xF, bB = b & 0xF;
	const UINT cA = (c >> 12) & 0xF, cR = (c >> 8) & 0xF, cG = (c >> 4) & 0xF, cB = c & 0xF;
	const UINT dA = (d >> 12) & 0xF, dR = (d >> 8) & 0xF, dG = (d >> 4) & 0xF, dB = d & 0xF;
	const UINT mA = (aA + bA + cA + dA + 2) >> 2;
	const UINT mR = (aR + bR + cR + dR + 2) >> 2;
	const UINT mG = (aG + bG + cG + dG + 2) >> 2;
	const UINT mB = (aB + bB + cB + dB + 2) >> 2;
	return static_cast<uint16_t>((mA << 12) | (mR << 8) | (mG << 4) | mB);
}

extern "C" HRESULT WINAPI D3DXFilterTexture(
	LPDIRECT3DBASETEXTURE8 base_texture,
	CONST PALETTEENTRY * /*palette*/,
	UINT src_level,
	DWORD /*filter*/)
{
	if (base_texture == nullptr) return E_POINTER;
	if (base_texture->GetType() != D3DRTYPE_TEXTURE) return D3D_OK;
	LPDIRECT3DTEXTURE8 texture = static_cast<LPDIRECT3DTEXTURE8>(base_texture);
	const DWORD levels = texture->GetLevelCount();
	if (src_level == D3DX_DEFAULT) src_level = 0;
	if (src_level >= levels) return D3D_OK;

	D3DSURFACE_DESC src_desc = {};
	if (FAILED(texture->GetLevelDesc(src_level, &src_desc))) return E_FAIL;
	UINT prev_w = src_desc.Width;
	UINT prev_h = src_desc.Height;
	const D3DFORMAT fmt = src_desc.Format;

	UINT bpp;
	switch (fmt)
	{
	case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8:
		bpp = 4; break;
	case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5:
	case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4:
		bpp = 2; break;
	case D3DFMT_A8: case D3DFMT_L8: case D3DFMT_P8: case D3DFMT_A4L4:
		bpp = 1; break;
	default:
		bpp = 4; break;
	}

	for (DWORD level = src_level + 1; level < levels; ++level)
	{
		UINT lw = prev_w >> 1; if (lw == 0) lw = 1;
		UINT lh = prev_h >> 1; if (lh == 0) lh = 1;
		D3DLOCKED_RECT src_lr = { 0 };
		D3DLOCKED_RECT dst_lr = { 0 };
		if (FAILED(texture->LockRect(level - 1, &src_lr, nullptr, 0))) break;
		if (FAILED(texture->LockRect(level, &dst_lr, nullptr, 0)))
		{
			texture->UnlockRect(level - 1);
			break;
		}
		// Clamp the second sample to the parent's extent so 1D edges
		// (2048x1, 1x1024) average with themselves instead of reading
		// past the allocated mip.
		const UINT parent_w = prev_w;
		const UINT parent_h = prev_h;
		const uint8_t * src = static_cast<const uint8_t *>(src_lr.pBits);
		uint8_t * dst = static_cast<uint8_t *>(dst_lr.pBits);
		for (UINT y = 0; y < lh; ++y)
		{
			const UINT y0 = 2 * y;
			const UINT y1 = (y0 + 1 < parent_h) ? (y0 + 1) : y0;
			for (UINT x = 0; x < lw; ++x)
			{
				const UINT x0 = 2 * x;
				const UINT x1 = (x0 + 1 < parent_w) ? (x0 + 1) : x0;
				if (bpp == 4)
				{
					const uint8_t * p00 = src + y0 * src_lr.Pitch + x0 * 4;
					const uint8_t * p10 = src + y0 * src_lr.Pitch + x1 * 4;
					const uint8_t * p01 = src + y1 * src_lr.Pitch + x0 * 4;
					const uint8_t * p11 = src + y1 * src_lr.Pitch + x1 * 4;
					uint8_t * d = dst + y * dst_lr.Pitch + x * 4;
					d[0] = static_cast<uint8_t>((p00[0] + p10[0] + p01[0] + p11[0] + 2) >> 2);
					d[1] = static_cast<uint8_t>((p00[1] + p10[1] + p01[1] + p11[1] + 2) >> 2);
					d[2] = static_cast<uint8_t>((p00[2] + p10[2] + p01[2] + p11[2] + 2) >> 2);
					d[3] = static_cast<uint8_t>((p00[3] + p10[3] + p01[3] + p11[3] + 2) >> 2);
				}
				else if (bpp == 2)
				{
					const uint16_t p00 = *reinterpret_cast<const uint16_t *>(src + y0 * src_lr.Pitch + x0 * 2);
					const uint16_t p10 = *reinterpret_cast<const uint16_t *>(src + y0 * src_lr.Pitch + x1 * 2);
					const uint16_t p01 = *reinterpret_cast<const uint16_t *>(src + y1 * src_lr.Pitch + x0 * 2);
					const uint16_t p11 = *reinterpret_cast<const uint16_t *>(src + y1 * src_lr.Pitch + x1 * 2);
					uint16_t * d = reinterpret_cast<uint16_t *>(dst + y * dst_lr.Pitch + x * 2);
					if (fmt == D3DFMT_R5G6B5)
						*d = Filter_R5G6B5_4(p00, p10, p01, p11);
					else if (fmt == D3DFMT_A4R4G4B4 || fmt == D3DFMT_X4R4G4B4)
						*d = Filter_A4R4G4B4_4(p00, p10, p01, p11);
					else
						*d = Filter_A1R5G5B5_4(p00, p10, p01, p11);
				}
				else
				{
					const UINT p00 = src[y0 * src_lr.Pitch + x0];
					const UINT p10 = src[y0 * src_lr.Pitch + x1];
					const UINT p01 = src[y1 * src_lr.Pitch + x0];
					const UINT p11 = src[y1 * src_lr.Pitch + x1];
					dst[y * dst_lr.Pitch + x] = static_cast<uint8_t>((p00 + p10 + p01 + p11 + 2) >> 2);
				}
			}
		}
		texture->UnlockRect(level);
		texture->UnlockRect(level - 1);
		prev_w = lw;
		prev_h = lh;
	}
	return D3D_OK;
}

// Copy src surface rows into dst surface through a pair of LockRect
// calls. Honors dst_rect for offset/extent, derives bytes-per-pixel
// from the src format so non-32bpp surfaces copy correctly. Palette,
// filter, and color-key arguments are ignored. Format conversion is
// not supported — caller must match dst and src formats (the engine's
// SurfaceClass::Copy/Stretch_Copy callers always do).
extern "C" HRESULT WINAPI D3DXLoadSurfaceFromSurface(
	LPDIRECT3DSURFACE8 dst,
	CONST PALETTEENTRY * /*dst_palette*/,
	CONST RECT * dst_rect,
	LPDIRECT3DSURFACE8 src,
	CONST PALETTEENTRY * /*src_palette*/,
	CONST RECT * src_rect,
	DWORD /*filter*/,
	D3DCOLOR /*color_key*/)
{
	if (dst == nullptr || src == nullptr) return E_POINTER;
	D3DSURFACE_DESC sd = {}, dd = {};
	if (FAILED(src->GetDesc(&sd))) return E_FAIL;
	if (FAILED(dst->GetDesc(&dd))) return E_FAIL;

	UINT bpp;
	switch (sd.Format)
	{
	case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8:
	case D3DFMT_A2B10G10R10: case D3DFMT_G16R16:
	case D3DFMT_X8L8V8U8: case D3DFMT_Q8W8V8U8: case D3DFMT_V16U16:
		bpp = 4; break;
	case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5:
	case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4: case D3DFMT_A8L8:
	case D3DFMT_A8R3G3B2: case D3DFMT_V8U8: case D3DFMT_L6V5U5:
		bpp = 2; break;
	case D3DFMT_R3G3B2:
	case D3DFMT_A8: case D3DFMT_L8: case D3DFMT_P8: case D3DFMT_A4L4:
		bpp = 1; break;
	default:
		bpp = 4; break;
	}

	UINT src_x = 0, src_y = 0, src_w = sd.Width, src_h = sd.Height;
	if (src_rect != nullptr)
	{
		src_x = static_cast<UINT>(src_rect->left);
		src_y = static_cast<UINT>(src_rect->top);
		src_w = static_cast<UINT>(src_rect->right - src_rect->left);
		src_h = static_cast<UINT>(src_rect->bottom - src_rect->top);
	}
	UINT dst_x = 0, dst_y = 0, dst_w = dd.Width, dst_h = dd.Height;
	if (dst_rect != nullptr)
	{
		dst_x = static_cast<UINT>(dst_rect->left);
		dst_y = static_cast<UINT>(dst_rect->top);
		dst_w = static_cast<UINT>(dst_rect->right - dst_rect->left);
		dst_h = static_cast<UINT>(dst_rect->bottom - dst_rect->top);
	}

	D3DLOCKED_RECT sl = { 0 };
	D3DLOCKED_RECT dl = { 0 };
	if (FAILED(src->LockRect(&sl, nullptr, 0))) return E_FAIL;
	if (FAILED(dst->LockRect(&dl, nullptr, 0)))
	{
		src->UnlockRect();
		return E_FAIL;
	}
	const UINT h = src_h < dst_h ? src_h : dst_h;
	const UINT w = src_w < dst_w ? src_w : dst_w;
	const UINT rowBytes = w * bpp;
	for (UINT y = 0; y < h; ++y)
	{
		std::memcpy(
			static_cast<uint8_t *>(dl.pBits) + (dst_y + y) * dl.Pitch + dst_x * bpp,
			static_cast<const uint8_t *>(sl.pBits) + (src_y + y) * sl.Pitch + src_x * bpp,
			rowBytes);
	}
	dst->UnlockRect();
	src->UnlockRect();
	return D3D_OK;
}

// Non-TGA file loading: dx8wrapper.cpp only hits this as a fallback when
// the TGA fast path rejects the file. In standalone we just surface
// MissingTexture through the engine's normal error handling.
extern "C" HRESULT WINAPI D3DXCreateTextureFromFileExA(
	LPDIRECT3DDEVICE8 /*device*/,
	LPCSTR src_file,
	UINT /*width*/,
	UINT /*height*/,
	UINT /*mip_levels*/,
	DWORD /*usage*/,
	D3DFORMAT /*format*/,
	D3DPOOL /*pool*/,
	DWORD /*filter*/,
	DWORD /*mip_filter*/,
	D3DCOLOR /*color_key*/,
	D3DXIMAGE_INFO * /*src_info*/,
	PALETTEENTRY * /*palette*/,
	LPDIRECT3DTEXTURE8 * out_texture)
{
	if (out_texture != nullptr) *out_texture = nullptr;
	static bool s_logged = false;
	if (!s_logged)
	{
		s_logged = true;
		WWDEBUG_SAY(("[D3DXStub] D3DXCreateTextureFromFileExA rejected (non-TGA path) file=%s",
			src_file ? src_file : "(null)"));
	}
	return E_NOTIMPL;
}

// Shader assembler — the bgfx backend uses its own .sc shaders, so if
// some code path calls this the result isn't driving bgfx output.
// Return E_NOTIMPL; callers already handle D3DX assembly failure by
// falling back to the fixed function pipeline.
extern "C" HRESULT WINAPI D3DXAssembleShader(
	LPCVOID /*src*/,
	UINT /*src_len*/,
	DWORD /*flags*/,
	LPD3DXBUFFER * constants,
	LPD3DXBUFFER * compiled_shader,
	LPD3DXBUFFER * errors)
{
	static bool s_logged = false;
	if (!s_logged)
	{
		s_logged = true;
		WWDEBUG_SAY(("[D3DXStub] D3DXAssembleShader stubbed (bgfx uses its own shaders)"));
	}
	if (constants) *constants = nullptr;
	if (compiled_shader) *compiled_shader = nullptr;
	if (errors) *errors = nullptr;
	return E_NOTIMPL;
}

#endif // GGC_BGFX_STANDALONE
