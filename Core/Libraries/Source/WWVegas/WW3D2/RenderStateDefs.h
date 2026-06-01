/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#pragma once

// Render state indices for RenderStateCache::Get/Set_Render_State.
// Values match legacy render state ordinals so the DX8 backend can use the same cache.
namespace RS {
constexpr unsigned ZENABLE = 7;
constexpr unsigned FILLMODE = 8;
constexpr unsigned SHADEMODE = 9;
constexpr unsigned ZWRITEENABLE = 14;
constexpr unsigned ALPHATESTENABLE = 15;
constexpr unsigned SRCBLEND = 19;
constexpr unsigned DESTBLEND = 20;
constexpr unsigned CULLMODE = 22;
constexpr unsigned ZFUNC = 23;
constexpr unsigned ALPHAREF = 24;
constexpr unsigned ALPHAFUNC = 25;
constexpr unsigned ALPHABLENDENABLE = 27;
constexpr unsigned FOGENABLE = 28;
constexpr unsigned SPECULARENABLE = 29;
constexpr unsigned FOGCOLOR = 34;
constexpr unsigned ZBIAS = 47;
constexpr unsigned STENCILENABLE = 52;
constexpr unsigned STENCILFAIL = 53;
constexpr unsigned STENCILZFAIL = 54;
constexpr unsigned STENCILPASS = 55;
constexpr unsigned STENCILFUNC = 56;
constexpr unsigned STENCILREF = 57;
constexpr unsigned STENCILMASK = 58;
constexpr unsigned STENCILWRITEMASK = 59;
constexpr unsigned TEXTUREFACTOR = 60;
constexpr unsigned LIGHTING = 137;
constexpr unsigned AMBIENT = 139;
constexpr unsigned NORMALIZENORMALS = 143;
constexpr unsigned DIFFUSEMATERIALSOURCE = 145;
constexpr unsigned AMBIENTMATERIALSOURCE = 147;
constexpr unsigned EMISSIVEMATERIALSOURCE = 148;
constexpr unsigned POINTSIZE = 154;
constexpr unsigned POINTSIZEMIN = 155;
constexpr unsigned POINTSPRITEENABLE = 156;
constexpr unsigned POINTSCALEENABLE = 157;
constexpr unsigned POINTSCALE_A = 158;
constexpr unsigned POINTSCALE_B = 159;
constexpr unsigned POINTSCALE_C = 160;
constexpr unsigned PATCHSEGMENTS = 164;
constexpr unsigned POINTSIZEMAX = 166;
constexpr unsigned COLORWRITEENABLE = 168;
constexpr unsigned BLENDOP = 171;
} // namespace RS

// Texture stage state indices for RenderStateCache::Get/Set_Texture_Stage_State.
// Values match legacy texture stage state ordinals so the DX8 backend can use the same cache.
namespace TSS {
constexpr unsigned COLOROP = 1;
constexpr unsigned COLORARG1 = 2;
constexpr unsigned COLORARG2 = 3;
constexpr unsigned ALPHAOP = 4;
constexpr unsigned ALPHAARG1 = 5;
constexpr unsigned ALPHAARG2 = 6;
constexpr unsigned BUMPENVMAT00 = 7;
constexpr unsigned BUMPENVMAT01 = 8;
constexpr unsigned BUMPENVMAT10 = 9;
constexpr unsigned BUMPENVMAT11 = 10;
constexpr unsigned TEXCOORDINDEX = 11;
constexpr unsigned ADDRESSU = 13;
constexpr unsigned ADDRESSV = 14;
constexpr unsigned MAGFILTER = 16;
constexpr unsigned MINFILTER = 17;
constexpr unsigned MIPFILTER = 18;
constexpr unsigned MAXANISOTROPY = 21;
constexpr unsigned BUMPENVLSCALE = 22;
constexpr unsigned BUMPENVLOFFSET = 23;
constexpr unsigned TEXTURETRANSFORMFLAGS = 24;
constexpr unsigned ADDRESSW = 25;
constexpr unsigned COLORARG0 = 26;
constexpr unsigned ALPHAARG0 = 27;
} // namespace TSS
