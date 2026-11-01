// Ronin @feature 28/09/2026 DX9: the WaterType 2 water, SM3. Began as a port of Water/wave.nvv (Kenny Mitchell 2001); now
// drawn on the map's own water polygons, and on the old infinite sea behind `water sea 1`. docs/Water_Work.md.
//
// Compile with: fxc /T vs_3_0 /Fo WaterSea.vso WaterSea_vs.hlsl

float4 g_ViewProj[4] : register(c0);	// world -> clip, uploaded transposed: one dp4 per clip component
float4 g_World[4]    : register(c4);	// object -> world, transposed: identity for the polygons, the patch for the sea
float4 g_UVScale     : register(c8);	// xy: bump tiles per world unit; zw: river edge uv from uv0 (2,1), else (0,0)
// Ronin @feature 28/09/2026 DX9: surface model - two normal-map layers, scale / rotation / scroll set per frame by C++.
// Ronin @feature 01/11/2026 DX9: layer A turns too (the sea's ripples follow the swell's wind): a matrix like B's; its
// scroll moved to c11.zw.
float4 g_NormalA     : register(c9);	// layer A: world xy -> uv rows (x0, y0, x1, y1), scale and rotation baked in
float4 g_NormalBRot  : register(c10);	// layer B: world xy -> uv rows (x0, y0, x1, y1), scale and rotation baked in
float4 g_NormalBOff  : register(c11);	// xy: layer B's scroll offset, zw: layer A's
// Ronin @feature 29/09/2026 DX9: phase 2 - Gerstner swell (R3DWaterSea.h), per wave two rows.
float4 g_Wave[8]     : register(c12);	// [2i] (k dx, k dy, omega, height), [2i+1] (travel dx, travel dy, 0, 0)
float4 g_SwellCfg    : register(c20);	// x: seconds, y: the mesh moves (the swell, or wakes - 25/10/2026), z: 1 / `swelldepth`, w: world z per sampled height
float4 g_HeightUV    : register(c21);	// world xy -> height uv: xy scale, zw offset
// Ronin @feature 02/10/2026 DX9: phase 4 - the river's flow map (R3DWaterSea.h).
float4 g_Flow        : register(c22);	// x: the current, world units per second; y: one flow cycle, seconds;
										// z: Ronin @bugfix 03/10/2026 DX9: the swell grid's real cell, world units
// Ronin @feature 03/10/2026 DX9: phase 5 - wakes: moving units raise the mesh (W3DWater.cpp renderWakes).
float4 g_WakeMap     : register(c23);	// world xy -> wake uv: xy scale, zw offset
float4 g_WakeCfg     : register(c24);	// x: world z per unit of the texture's height (`wakeheight`; 0 = no wakes here)

sampler2D s_Height : register(s0);		// D3DVERTEXTEXTURESAMPLER0: the terrain heights, R32F
sampler2D s_Wake   : register(s1);		// D3DVERTEXTEXTURESAMPLER1: the wake texture, w = the height the mesh takes

// Only what both layouts carry: the sea's declaration (FLOAT3, D3DCOLOR, FLOAT2; its NORMAL aliases the uv, unused) and the
// polygons' XYZNDUV2 FVF.
struct VSInput
{
	float4 pos   : POSITION0;
	float3 flow  : NORMAL0;		// Ronin @feature 02/10/2026 DX9: a river's flow - downstream x the width factor (drawRiverWater)
	float4 color : COLOR0;
	float2 uv    : TEXCOORD0;
};

struct VSOutput
{
	float4 pos      : POSITION0;
	float2 bumpUV   : TEXCOORD0;
	float2 texUV    : TEXCOORD1;
	float2 edgeUV   : TEXCOORD2;
	float4 clipPos  : TEXCOORD3;
	float3 worldPos : TEXCOORD4;
	float4 normalUV : TEXCOORD5;	// xy layer A, zw layer B
	float3 swell    : TEXCOORD6;	// Ronin @feature 29/09/2026 DX9: xy the undisplaced world xy, z the swell weight
	float4 flowUV   : TEXCOORD7;	// Ronin @feature 02/10/2026 DX9: a river's drift over one flow cycle - xy layer A, zw layer B
	float4 color    : COLOR0;
};

// Ronin @bugfix 03/10/2026 DX9: a ring one grid cell out, for the lowest ground around a vertex.
static const float2 RING[8] =
{
	float2( 1.0f,  0.0f), float2(-1.0f,  0.0f), float2( 0.0f,  1.0f), float2( 0.0f, -1.0f),
	float2( 0.7f,  0.7f), float2(-0.7f,  0.7f), float2( 0.7f, -0.7f), float2(-0.7f, -0.7f),
};

VSOutput main(VSInput input)
{
	VSOutput output;

	float4 world;
	world.x = dot(input.pos, g_World[0]);
	world.y = dot(input.pos, g_World[1]);
	world.z = dot(input.pos, g_World[2]);
	world.w = 1.0f;

	// Ronin @feature 29/09/2026 DX9: phase 2 - Gerstner swell, flat at the waterline and full from the ramp depth down.
	// P += sum( travel D cos(th), height sin(th) ), th = k D.xy - omega t.
	output.swell = float3(world.xy, 0.0f);
	float swellH = 0.0f;	// Ronin @diagnostic 03/10/2026 DX9: how far the swell moved this vertex up or down (`view 10`)
	if (g_SwellCfg.y > 0.5f)
	{
		const float2 uv     = world.xy * g_HeightUV.xy + g_HeightUV.zw;
		const float  ground = tex2Dlod(s_Height, float4(uv, 0.0f, 0.0f)).r * g_SwellCfg.w;
		// Ronin @bugfix 03/10/2026 DX9: the swell fades by the LOWEST ground within a grid cell, not the ground under the vertex:
		// at a cliff a vertex just inside the land must move with the water beside it, or a triangle climbs the rock.
		float lowest = ground;
		const float2 reach = g_Flow.z * g_HeightUV.xy;
		for (int j = 0; j < 8; j++)
			lowest = min(lowest, tex2Dlod(s_Height, float4(uv + RING[j] * reach, 0.0f, 0.0f)).r * g_SwellCfg.w);
		const float  weight = saturate((world.z - lowest) * g_SwellCfg.z);
		float3 move = float3(0.0f, 0.0f, 0.0f);
		for (int i = 0; i < 4; i++)
		{
			float s, c;
			sincos(dot(g_Wave[2 * i].xy, world.xy) - g_Wave[2 * i].z * g_SwellCfg.x, s, c);
			move += float3(g_Wave[2 * i + 1].xy * c, g_Wave[2 * i].w * s);
		}
		const float restZ = world.z;
		world.xyz   += move * weight;
		// Ronin @feature 03/10/2026 DX9: phase 5 - a wake's broad swell, read where the swell left this vertex and faded by
		// the same depth ramp; before the stops below, so a wake washes a beach as a crest does.
		world.z     += tex2Dlod(s_Wake, float4(world.xy * g_WakeMap.xy + g_WakeMap.zw, 0.0f, 0.0f)).w * g_WakeCfg.x * weight;
		// Ronin @bugfix 03/10/2026 DX9: a trough stops just above the seabed, only where the water is deeper than that (the
		// polygons run on under the land). At the shoreline a crest washes at most 0.4 over the ground.
		world.z      = (ground + 0.5f < restZ) ? max(world.z, ground + 0.5f) : min(world.z, max(restZ, ground + 0.4f));
		swellH       = world.z - restZ;
		output.swell.z = weight;
	}
	// Ronin @feature 02/10/2026 DX9: phase 4 - rivers never swell: their swell.z carries the speed factor / 2 instead (the flow
	// vector's length, drawRiverWater's average width / this width, 0.5..2), for the current's foam. `view 10` shows it.
	if (g_UVScale.w > 0.5f)
		output.swell.z = saturate(length(input.flow.xy) * 0.5f);

	float4 clip;
	clip.x = dot(world, g_ViewProj[0]);
	clip.y = dot(world, g_ViewProj[1]);
	clip.z = dot(world, g_ViewProj[2]);
	clip.w = dot(world, g_ViewProj[3]);
	output.pos = clip;

	// Ronin @feature 28/09/2026 DX9: divided per pixel. The original divided per vertex, exact only at the vertices; the
	// flat water's 80-unit cells and the river strips are too big for that.
	output.clipPos = clip;

	// Ronin @feature 28/09/2026 DX9: bump in world space, so lakes, rivers and the sea ripple as one surface. The sea's
	// tiling is unchanged: 3 tiles per 40-unit patch cell.
	output.bumpUV = world.xy * g_UVScale.xy;
	output.texUV  = input.uv;
	output.edgeUV = input.uv * g_UVScale.zw;	// a river's uv2 is its uv1 with u doubled (drawRiverWater)
	output.color  = input.color;

	// Ronin @feature 28/09/2026 DX9: world position for the view vector; the two normal layers cross, so no tiling shows.
	output.worldPos    = world.xyz;
	output.normalUV.xy = float2(dot(world.xy, g_NormalA.xy), dot(world.xy, g_NormalA.zw)) + g_NormalBOff.zw;
	output.normalUV.zw = float2(dot(world.xy, g_NormalBRot.xy), dot(world.xy, g_NormalBRot.zw)) + g_NormalBOff.xy;

	// Ronin @feature 02/10/2026 DX9: phase 4 - a river's flow map. Its ripples stay in world space (the river's uv kinks and
	// shears across its quads); the PS drags them downstream by this drift, two phases cross-faded (Valve, Portal 2).
	const float2 drift = (g_UVScale.w > 0.5f) ? input.flow.xy * (g_Flow.x * g_Flow.y) : float2(0.0f, 0.0f);
	output.flowUV.xy = float2(dot(drift, g_NormalA.xy), dot(drift, g_NormalA.zw));
	output.flowUV.zw = float2(dot(drift, g_NormalBRot.xy), dot(drift, g_NormalBRot.zw));
	output.flowUV.w += swellH;	// Ronin @diagnostic 03/10/2026 DX9: standing water has no drift; w carries the swell's height


	return output;
}
