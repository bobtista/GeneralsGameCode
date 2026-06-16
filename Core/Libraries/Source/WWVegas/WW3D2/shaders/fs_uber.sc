$input v_color0, v_texcoord0, v_texcoord1, v_normal, v_cloudUV, v_stage0UV, v_stage1UV, v_stage2UV, v_sceneDepth, v_worldPos

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);
SAMPLER2D(s_tex1, 1);
SAMPLER2D(s_tex2, 2);
SAMPLER2D(s_tex3, 3);
// Terrain cloud-shadow scroll texture (BASE_NOISE1/NOISE12 paths on DX8).
SAMPLER2D(s_cloudMap, 5);
SAMPLER2D(s_sceneDepth, 6);
// Specular/rim uniforms not in trunk's packed u_material[] array (which already
// provides u_matDiffuse/Ambient/Emissive/tssOps/atest via #define).
uniform vec4 u_matSpecular; // rgb = specular color, w = shininess (Blinn-Phong power)
uniform vec4 u_matFx; // x specular strength, y rim strength, z rim power, w emissive boost
uniform vec4 u_eyePos; // xyz = world-space camera position
SAMPLER2D(s_shadowMap, 7);
uniform mat4 u_shadowMatrices[3]; // per-cascade world -> sun shadow clip
uniform vec4 u_shadowParams; // x atlas texel size, y depth bias, z strength, w enabled
uniform vec4 u_lightDirs[4];    // per-light direction (xyz=toward light, w=enabled)
uniform vec4 u_lightColors[4]; // per-light diffuse color (rgb)
uniform vec4 u_lightAmbients[4]; // per-light ambient color (rgb)
uniform vec4 u_lightPositions[4]; // per-light world position (xyz)
uniform vec4 u_lightParams[4]; // x inner range, y outer/range, z > 0.5 point, w enabled
uniform vec4 u_sceneAmbient;   // scene ambient color (rgb)
uniform vec4 u_lightingEnabled; // .x > 0.5 = apply N.L lighting; else vertex is pre-lit
uniform vec4 u_texcoordSelect; // .x > 0.5 = use v_texcoord1 for stage 0 sampling
uniform vec4 u_shroudParams; // xy = offset, zw = scale
uniform vec4 u_softParticleParams; // .x enable, .y fade scale, zw inverse scene size

// TheSuperHackers @performance bobtista 15/06/2026 Packed per-draw material uniforms.
// Index order MUST match MaterialUniformSlot in BgfxBackend.cpp.
uniform vec4 u_material[24];
#define u_matDiffuse            u_material[0]
#define u_matAmbient            u_material[1]
#define u_matEmissive           u_material[2]
#define u_tssOps0               u_material[3]
#define u_tssOps1               u_material[4]
#define u_atestParams           u_material[5]
#define u_texcoordSource        u_material[6]
#define u_vertexColorFlags      u_material[7]
#define u_texcoordSelect2       u_material[8]
#define u_projectedDecalMode    u_material[9]
#define u_grayscaleEnable       u_material[10]
#define u_objectShroudDim       u_material[11]
#define u_cloudParams           u_material[12]
#define u_texTransform0         u_material[13]
#define u_texTransform1         u_material[14]
#define u_texTransform0Z        u_material[15]
#define u_tex1Transform0        u_material[16]
#define u_tex1Transform1        u_material[17]
#define u_tex1TransformZ        u_material[18]
#define u_tex2Transform0        u_material[19]
#define u_tex2Transform1        u_material[20]
#define u_texProjected          u_material[21]
#define u_legacyPixelShaderMode u_material[22]
#define u_zBias                 u_material[23]

// TSS operation IDs (must match BgfxBackend.cpp encoding)
#define TSS_DISABLE         0.0
#define TSS_SELECTARG1      1.0
#define TSS_SELECTARG2      2.0
#define TSS_MODULATE        3.0
#define TSS_MODULATE2X      4.0
#define TSS_ADD             5.0
#define TSS_ADDSIGNED       6.0
#define TSS_SUBTRACT        7.0
#define TSS_BLENDTEXALPHA   8.0
#define TSS_BLENDCURALPHA   9.0
#define TSS_ADDSMOOTH      10.0

// Arg source IDs
#define SRC_TEXTURE  0.0
#define SRC_DIFFUSE  1.0
#define SRC_CURRENT  2.0

// RenderBackendProjectedDecalMode values from IRenderBackend.h.
#define PROJECTED_DECAL_BLOB_SHADOW 1.0
#define PROJECTED_DECAL_ADDITIVE    2.0
#define PROJECTED_DECAL_ALPHA       3.0
#define PROJECTED_DECAL_MULTIPLY    4.0

#define CLOUD_SHADOW_MIN 0.72
// BT.601 luminance weights (matches the BGRA bytes of the D3D8 TFACTOR=0x80A5CA8E cascade used by the disabled-button grayscale path).
#define LUMA_WEIGHTS vec3(0.299, 0.587, 0.114)
// Additive projector/effect textures often carry a pure-black matte around
// useful glow pixels. In D3D8 that matte is a no-op for additive blending; on
// bgfx it can survive as visible matte fragments unless discarded.
// Half of one 8-bit color step is "rounds to black" in authored effect mattes.
#define ADDITIVE_MATTE_EPSILON (0.5 / 255.0)
#define ALPHA_MASK_EPSILON (0.5 / 255.0)
// Multiplier applied to shadowed pixels. 1.0 = unshadowed, 0.0 = fully black; we darken to 60% for visible but not crushed shadows.
#define SHADOW_DARKNESS 0.6

// Maximum darkening applied at the blob center under the multiplicative blend.
// The blob darkening is driven directly by ShadowI's soft alpha (no smoothstep):
// the alpha over the coarse heightmap receiver is mostly below 0.5, so a smoothstep
// gate crushed it to nothing and made infantry shadows invisible on bgfx.
#define BLOB_MASK_MAX_DARKNESS 0.7

bool alphaTestPass(float alpha, float ref, float func)
{
	if (func < 0.5)
	{
		return true;
	}
	if (func < 1.5) { return false; }              // D3DCMP_NEVER
	if (func < 2.5) { return alpha < ref; }        // D3DCMP_LESS
	if (func < 3.5) { return abs(alpha - ref) <= ALPHA_MASK_EPSILON; }
	if (func < 4.5) { return alpha <= ref; }       // D3DCMP_LESSEQUAL
	if (func < 5.5) { return alpha > ref; }        // D3DCMP_GREATER
	if (func < 6.5) { return abs(alpha - ref) > ALPHA_MASK_EPSILON; }
	if (func < 7.5) { return alpha >= ref; }       // D3DCMP_GREATEREQUAL
	return true;                                  // D3DCMP_ALWAYS
}

vec3 applyColorOp(float op, vec3 arg1, vec3 arg2)
{
	// Binary split to reduce worst-case from 7 sequential comparisons to 4.
	if (op < 5.5)
	{
		if (op < 2.5)
		{
			return (op < 1.5) ? arg1 : arg2;
		}
		if (op < 3.5)
		{
			return arg1 * arg2;
		}
		if (op < 4.5)
		{
			return min(arg1 * arg2 * 2.0, vec3_splat(1.0));
		}
		return arg1 + arg2;
	}
	// ADDSIGNED(6), SUBTRACT(7), BLENDTEX(8)/BLENDCUR(9) handled by caller, ADDSMOOTH(10)
	if (op < 6.5)
	{
		return arg1 + arg2 - vec3_splat(0.5);
	}
	if (op < 7.5)
	{
		return arg1 - arg2;
	}
	if (op > 9.5)
	{
		return arg1 + arg2 - arg1 * arg2;
	}
	return arg1;
}

float applyAlphaOp(float op, float arg1, float arg2)
{
	if (op < 5.5)
	{
		if (op < 2.5)
		{
			return (op < 1.5) ? arg1 : arg2;
		}
		if (op < 3.5)
		{
			return arg1 * arg2;
		}
		if (op < 4.5)
		{
			return min(arg1 * arg2 * 2.0, 1.0);
		}
		return arg1 + arg2;
	}
	if (op < 6.5)
	{
		return arg1 + arg2 - 0.5;
	}
	if (op < 7.5)
	{
		return arg1 - arg2;
	}
	if (op > 9.5)
	{
		return arg1 + arg2 - arg1 * arg2;
	}
	return arg1;
}

vec3 sampleCloudShadow(vec2 cloudUV)
{
	vec3 cloudSample = texture2D(s_cloudMap, cloudUV).rgb;
	return max(cloudSample, vec3_splat(CLOUD_SHADOW_MIN));
}

// World-space normal offset (cascade 0) for the shadow receive point, tuned to the
// finest cascade's texel footprint; coarser cascades scale it up.
#define SUN_SHADOW_NORMAL_OFFSET 1.2

// TheSuperHackers @feature bobtista 16/06/2026 Cascaded sun shadow lookup. Three
// concentric cascades share one 2x2 atlas (tile scale 0.5; offsets 0:(0,0) 1:(.5,0)
// 2:(0,.5)). For each pixel the tightest cascade that contains it (with a margin so the
// 5x5 PCF kernel never crosses a tile boundary) is sampled. Returns the lit fraction in
// [0,1]: 1 = fully lit, 0 = fully shadowed. nrm is the world-space surface normal.
float sampleSunShadow(vec3 worldPos, vec3 nrm)
{
	if (u_shadowParams.w < 0.5)
	{
		return 1.0;
	}
	vec3 n = normalize(nrm);
	float ndotl = clamp(dot(n, normalize(u_lightDirs[0].xyz)), 0.0, 1.0);
	float slope = 1.0 + 2.0 * (1.0 - ndotl);
	float texel = u_shadowParams.x; // atlas texel = 1/atlasSize
	float bias = u_shadowParams.y;
	for (int c = 0; c < 3; ++c)
	{
		// Normal-offset bias scaled per cascade (coarser cascades = bigger texels =
		// need more offset to clear self-shadow acne without peter-panning).
		vec3 biasedPos = worldPos + n * (SUN_SHADOW_NORMAL_OFFSET * (1.0 + float(c)) * slope);
		vec4 sc = mul(u_shadowMatrices[c], vec4(biasedPos, 1.0));
		if (sc.w <= 0.0)
		{
			continue;
		}
		vec3 ndc = sc.xyz / sc.w;
		vec2 cuv = ndc.xy * 0.5 + 0.5;
#if !BGFX_SHADER_LANGUAGE_GLSL
		cuv.y = 1.0 - cuv.y;
#endif
		// Tightest containing cascade, with a margin to keep the PCF kernel in-tile.
		if (cuv.x > 0.02 && cuv.x < 0.98 && cuv.y > 0.02 && cuv.y < 0.98)
		{
			vec2 tileOffset = vec2(0.0, 0.0);
			if (c == 1) { tileOffset = vec2(0.5, 0.0); }
			else if (c == 2) { tileOffset = vec2(0.0, 0.5); }
			vec2 auv = tileOffset + cuv * 0.5;
			float curDepth = clamp(ndc.z, 0.0, 1.0) - bias;
			float lit = 0.0;
			for (int dy = -1; dy <= 1; ++dy)
			{
				for (int dx = -1; dx <= 1; ++dx)
				{
					float storedDepth = texture2D(s_shadowMap, auv + vec2(float(dx), float(dy)) * texel).x;
					lit += (curDepth <= storedDepth) ? 1.0 : 0.0;
				}
			}
			return lit / 9.0;
		}
	}
	return 1.0; // outside all cascades = lit
}

// TheSuperHackers @feature bobtista 16/06/2026 Sun-shadow color multiplier. Returns the
// factor to multiply a lit surface by. Only sun-facing surfaces are shadowed (back faces
// are already dark; shadowing them just crushes them to black), and shadowed pixels are
// darkened toward an ambient floor (1 - strength), never to black. Shared by the terrain
// pixel-shader branch and the generic material path so both ground and objects shadow.
float sunShadowFactor(vec3 worldPos, vec3 rawNormal)
{
	if (u_shadowParams.w < 0.5)
	{
		return 1.0;
	}
	float nLen = length(rawNormal);
	vec3 n = (nLen > 1e-5) ? (rawNormal / nLen) : vec3(0.0, 0.0, 1.0);
	float sunFacing = smoothstep(0.0, 0.35, dot(n, normalize(u_lightDirs[0].xyz)));
	float lit = sampleSunShadow(worldPos, n);
	return mix(1.0 - u_shadowParams.z * sunFacing, 1.0, lit);
}

void main()
{
	vec4 diffuse = v_color0;
	vec2 stage0UV = v_stage0UV;
	vec2 stage1UV = v_stage1UV;
	if (u_texProjected.x > 0.5 && abs(v_sceneDepth.x) > 1e-6)
	{
		stage0UV /= v_sceneDepth.x;
	}
	if (u_texProjected.y > 0.5 && abs(v_sceneDepth.y) > 1e-6)
	{
		stage1UV /= v_sceneDepth.y;
	}
	vec2 baseStage0UV = v_texcoord0;
	// --- Terrain pixel shader path ---
	// The D3D8 terrain system uses a hardware pixel shader (terrain.nvp)
	// that completely replaces the TSS pipeline:
	//   tex t0              ; sample tex0 with UV set 0
	//   tex t1              ; sample tex1 with UV set 1
	//   lrp r0, v0.a, t1, t0  ; mix(t0, t1, vertex_alpha)
	//   mul r0, r0, v0        ; multiply by diffuse (baked lighting)
	if (u_texcoordSelect.y > 0.5)
	{
		vec4 baseTex  = texture2D(s_tex0, (u_texProjected.x > 0.5) ? stage0UV : baseStage0UV);
		vec4 blendTex = texture2D(s_tex1, (u_texProjected.y > 0.5) ? stage1UV : v_texcoord1);
		float blendAlpha = diffuse.a;
		vec3 blended = mix(baseTex.rgb, blendTex.rgb, blendAlpha);
		vec4 result = vec4(blended * diffuse.rgb, 1.0);

		if (!alphaTestPass(result.a, u_atestParams.x, u_atestParams.y))
		{
			discard;
		}

		// TheSuperHackers @bugfix bobtista 28/04/2026 Apply the cloudmap
		// inside the terrain pixel-shader path. Terrain returns from this
		// branch before the generic material path below, so placing cloud
		// modulation only after the branch made the effect invisible.
		if (u_cloudParams.w > 0.5)
		{
			result.rgb *= sampleCloudShadow(v_cloudUV);
		}

		// TheSuperHackers @bugfix bobtista 16/06/2026 Terrain returns from this branch
		// before the generic shadow apply below, so the sun shadow has to be applied here
		// too - otherwise cast shadows land on roads/decals/objects but skip the ground.
		result.rgb *= sunShadowFactor(v_worldPos, v_normal);

		gl_FragColor = result;
		return;
	}

	vec4 tex0 = texture2D(s_tex0, stage0UV);
	vec4 tex1 = texture2D(s_tex1, stage1UV);
	vec4 tex2 = texture2D(s_tex2, v_stage2UV);
	vec2 stage3UV = v_texcoord0;
	if (u_texcoordSource.w > 2.5)
	{
		stage3UV = (v_worldPos.xy + u_shroudParams.xy) * u_shroudParams.zw;
	}
	else if (u_texcoordSource.w > 0.5 && u_texcoordSource.w < 1.5)
	{
		stage3UV = v_texcoord1;
	}
	vec4 tex3 = texture2D(s_tex3, stage3UV);

	if (u_texcoordSelect2.z > 3.5)
	{
		// Zero Hour command-center driveway emblems are player-recolored
		// sorted decals. The generic material path can inherit zero opacity
		// from nearby water/shroud work, while the remapped texture carries
		// its matte as black RGB rather than a reliable alpha channel.
		float mask = max(max(tex0.r, tex0.g), tex0.b);
		float alpha = clamp(mask * 4.0 * diffuse.a, 0.0, 1.0);
		if (alpha <= ALPHA_MASK_EPSILON)
		{
			discard;
		}
		gl_FragColor = vec4(tex0.rgb, alpha);
		return;
	}

	if (u_texcoordSelect2.z > 2.5)
	{
		// Sneak Attack ground dirt is authored as a sorted translucent W3D
		// quad. Use the texture alpha directly; inherited material opacity can
		// be zero on this replay path and erase the broad dirt stain.
		float alpha = clamp(tex0.a * diffuse.a, 0.0, 1.0);
		if (alpha <= ALPHA_MASK_EPSILON)
		{
			discard;
		}
		gl_FragColor = vec4(tex0.rgb * diffuse.rgb, alpha);
		return;
	}

	if (u_texcoordSelect2.z > 1.5)
	{
		// Chinook rotor blur is authored as a sorted translucent mask. Keep
		// it out of the generic material path so stale vehicle material alpha
		// cannot erase the cards after the sorted pool is replayed.
		float mask = max(tex0.a, dot(tex0.rgb, LUMA_WEIGHTS));
		float alpha = clamp(mask * diffuse.a, 0.0, 1.0);
		if (alpha <= ALPHA_MASK_EPSILON)
		{
			discard;
		}
		vec3 color = tex0.rgb * diffuse.rgb;
		if (max(max(color.r, color.g), color.b) <= ADDITIVE_MATTE_EPSILON)
		{
			color = vec3_splat(mask) * diffuse.rgb;
		}
		gl_FragColor = vec4(color, alpha);
		return;
	}

	if (u_texcoordSelect.z > 0.5)
	{
		// Projected shroud overlays are destination multipliers. The terrain
		// vertex buffer they reuse carries baked terrain diffuse, but the DX8
		// shroud pass contributes only the shroud texture multiplier here.
		vec4 shroud = tex0;
		if (u_objectShroudDim.y > 0.5)
		{
			float maskAlpha = texture2D(s_tex1, v_texcoord0).a;
			if (maskAlpha < u_objectShroudDim.z)
			{
				discard;
			}
			shroud.a *= maskAlpha;
		}
		// TheSuperHackers @bugfix bobtista 01/07/2026 The shroud texture RGB already
		// encodes the per-cell level (level * shroudColor: fogged ~0.5, clear 1.0,
		// shrouded 0), so it is the sole darkening term here just like the terrain
		// shroud pass and retail DX8. An extra fogAlpha/clearAlpha dim factor would
		// darken fogged structures a second time (0.5 * 0.5), rendering them near-black.
		gl_FragColor = shroud;
		return;
	}

	if (u_legacyPixelShaderMode.x > 0.5)
	{
		vec4 water = vec4(tex0.rgb * diffuse.rgb, tex0.a * diffuse.a);
		if (u_legacyPixelShaderMode.x > 2.5 && u_legacyPixelShaderMode.x < 3.5)
		{
			// The trapezoid-water vertex format carries the authored
			// world-space noise UV in TEXCOORD1. The legacy D3D path also
			// derives this sample through TCI_CAMERASPACEPOSITION, but using
			// the explicit UV keeps bgfx out of a fragile generated-coordinate
			// path and matches the same world-space mapping.
			vec4 waterNoise = texture2D(s_tex2, v_texcoord1);
			// Trapezoid / standing water ps.1.1:
			//   r0 = diffuse * t0
			//   r0.rgb += t1.rgb * t2.rgb
			//   r0.rgb *= t3.rgb
			water.rgb += tex1.rgb * waterNoise.rgb;
			water.rgb *= tex3.rgb;
		}
		else if (u_legacyPixelShaderMode.x > 0.5 && u_legacyPixelShaderMode.x < 1.5)
		{
			// River water ps.1.1 uses the same sparkle/noise contribution
			// and adds the stage-3 shroud contribution before the final add.
			water.rgb += tex3.rgb;
			water.a *= tex3.a;
			water.rgb += tex1.rgb * tex2.rgb;
		}
		gl_FragColor = water;
		return;
	}

	vec2 projectedDecalUV = v_texcoord0;
	vec4 projectedDecalTex = tex0;
	if (u_projectedDecalMode.x > 0.5)
	{
		projectedDecalTex = texture2D(s_tex0, projectedDecalUV);
		// TheSuperHackers @bugfix bobtista 31/05/2026 Default infantry blob shadows
		// (mode 1) project onto a coarse heightmap receiver quad much larger than the
		// [0,1] decal image, so most blob fragments have out-of-range UVs. Discarding
		// them (needed for the other projected-decal modes) erased almost the whole
		// blob, leaving infantry shadows nearly invisible. ShadowI is clamp-sampled
		// with a transparent (alpha 0) border, so out-of-range fragments already add
		// no darkening; keep them for the blob branch instead of discarding.
		bool isBlobMode = u_projectedDecalMode.x > (PROJECTED_DECAL_BLOB_SHADOW - 0.5)
			&& u_projectedDecalMode.x < (PROJECTED_DECAL_BLOB_SHADOW + 0.5);
		if (!isBlobMode
			&& (projectedDecalUV.x < 0.0 || projectedDecalUV.x > 1.0 || projectedDecalUV.y < 0.0 || projectedDecalUV.y > 1.0))
		{
			// W3D projects decals onto terrain cell meshes that can extend beyond
			// the decal image. DX8 treats that area as non-contributing; clamping the
			// bgfx sample instead can repeat dark texture padding into blocky patches.
			discard;
		}
	}

	if (u_projectedDecalMode.x > (PROJECTED_DECAL_ADDITIVE - 0.5)
		&& u_projectedDecalMode.x < (PROJECTED_DECAL_ADDITIVE + 0.5))
	{
		// W3D projected additive decals (Spy Satellite / Spy Drone reveal
		// grid, radius decals) use the fixed-function additive preset:
		// stage0 texture modulated by vertex diffuse, then ONE/ONE blending.
		// They are not lit meshes and must not inherit stale material,
		// secondary-stage or blob-shadow state from the shared decal
		// batch path.
		vec4 projected = vec4(projectedDecalTex.rgb * diffuse.rgb, 0.0);
		if (max(max(projected.r, projected.g), projected.b) <= ADDITIVE_MATTE_EPSILON)
		{
			discard;
		}
		gl_FragColor = projected;
		return;
	}

	if (u_projectedDecalMode.x > (PROJECTED_DECAL_ALPHA - 0.5)
		&& u_projectedDecalMode.x < (PROJECTED_DECAL_ALPHA + 0.5))
	{
		// W3D projected alpha decals (selection/guard/reveal overlays) use
		// the fixed-function stage-0 texture modulated by vertex diffuse and
		// the draw's SRC_ALPHA/INV_SRC_ALPHA blend. Keep them out of the
		// generic TSS path so stale secondary-stage/shadow state cannot turn
		// transparent matte pixels into dark geometry.
		float alpha = clamp(projectedDecalTex.a * diffuse.a, 0.0, 1.0);
		if (alpha <= ALPHA_MASK_EPSILON)
		{
			discard;
		}
		gl_FragColor = vec4(projectedDecalTex.rgb * diffuse.rgb, alpha);
		return;
	}

	if (u_projectedDecalMode.x > (PROJECTED_DECAL_BLOB_SHADOW - 0.5)
		&& u_projectedDecalMode.x < (PROJECTED_DECAL_BLOB_SHADOW + 0.5))
	{
		// Default infantry blobs are authored as a soft ALPHA mask (ShadowI RGB is
		// white). Darken the terrain directly by that alpha under the ZERO/SRC_COLOR
		// multiplicative blend; emitting (1 - mask) darkens by mask at the blob
		// center and falls off softly to the transparent edge. No smoothstep: the
		// alpha over the coarse heightmap receiver is mostly below 0.5, so a
		// smoothstep gate crushed it to nothing and made the blobs invisible.
		float mask = clamp(projectedDecalTex.a * diffuse.a, 0.0, 1.0) * BLOB_MASK_MAX_DARKNESS;
		gl_FragColor = vec4(vec3_splat(1.0 - mask), 1.0);
		return;
	}

	if (u_projectedDecalMode.x > (PROJECTED_DECAL_MULTIPLY - 0.5)
		&& u_projectedDecalMode.x < (PROJECTED_DECAL_MULTIPLY + 0.5))
	{
		// Non-blob projected shadows use W3D's preset multiplicative
		// fixed-function shader: COLOROP=MODULATE and blend ZERO/SRC_COLOR.
		// Their textures can still carry black RGB in transparent padding.
		// DX8's texture/decal setup lets that padding contribute as neutral
		// destination color; in bgfx it must be made explicit or large shadow
		// receiver meshes stamp blocky black patches around the actual mask.
		float mask = clamp(projectedDecalTex.a * diffuse.a, 0.0, 1.0);
		vec3 multiplier = projectedDecalTex.rgb * diffuse.rgb;
		gl_FragColor = vec4(mix(vec3_splat(1.0), multiplier, mask), 1.0);
		return;
	}

	// --- TSS stage evaluation ---
	// u_tssOps0 = (priColorOp, priAlphaOp, secColorOp, secAlphaOp)
	// u_tssOps1 = (priCArg1Src, priAArg1Src, secCArg1Src, secAArg1Src)
	float priColorOp = u_tssOps0.x;
	float priAlphaOp = u_tssOps0.y;
	float secColorOp = u_tssOps0.z;
	float secAlphaOp = u_tssOps0.w;
	float priArg1Src = u_tssOps1.x;

	vec4 current;

	// Fast paths for the most common TSS combos. These are uniform branches
	// (all fragments in a draw take the same path) so the GPU skips the
	// not-taken side entirely. For the ~90% of draws that hit a fast path,
	// applyColorOp/applyAlphaOp and the secondary stage are never entered.

	if (priColorOp > 2.5 && priColorOp < 3.5
		&& priAlphaOp > 2.5 && priAlphaOp < 3.5
		&& secColorOp < 0.5 && secAlphaOp < 0.5
		&& priArg1Src < 0.5)
	{
		// Fast path: MODULATE primary, DISABLE secondary (~80% of draws).
		// tex0 * diffuse for both color and alpha.
		current = vec4(tex0.rgb * diffuse.rgb, tex0.a * diffuse.a);
	}
	else if (priColorOp > 0.5 && priColorOp < 1.5
		&& priAlphaOp > 0.5 && priAlphaOp < 1.5
		&& secColorOp < 0.5 && secAlphaOp < 0.5
		&& priArg1Src < 0.5)
	{
		// Fast path: SELECTARG1 primary, DISABLE secondary.
		// Texture only — shell map, pre-lit terrain, baked-lit textures.
		current = tex0;
	}
	else if (priColorOp > 1.5 && priColorOp < 2.5
		&& priAlphaOp > 1.5 && priAlphaOp < 2.5
		&& secColorOp < 0.5 && secAlphaOp < 0.5)
	{
		// Fast path: SELECTARG2 primary, DISABLE secondary.
		// Diffuse only — untextured lit meshes.
		current = diffuse;
	}
	else
	{
		// General TSS path — handles all remaining combinations via
		// applyColorOp/applyAlphaOp. This covers detail textures,
		// additive blending, bump env map fallback, etc.
		float priAlphaArg1Src = u_tssOps1.y;

		vec4 priArg1 = (priArg1Src < 0.5) ? tex0 : diffuse;
		vec4 priArg2 = (priArg1Src < 0.5) ? diffuse : tex0;
		vec4 priAlphaA1 = (priAlphaArg1Src < 0.5) ? tex0 : diffuse;
		vec4 priAlphaA2 = (priAlphaArg1Src < 0.5) ? diffuse : tex0;

		vec3 priColor;
		float priAlpha;

		if (priColorOp < 0.5)
		{
			priColor = diffuse.rgb;
			priAlpha = diffuse.a;
		}
		else
		{
			priColor = applyColorOp(priColorOp, priArg1.rgb, priArg2.rgb);
			priAlpha = applyAlphaOp(priAlphaOp, priAlphaA1.a, priAlphaA2.a);
		}

		current = vec4(priColor, priAlpha);

		// Secondary/detail stage
		if (secColorOp > 0.5)
		{
			vec3 secArg1 = tex1.rgb;
			vec3 secArg2 = current.rgb;

			if (secColorOp > 7.5 && secColorOp < 8.5)
			{
				current.rgb = mix(secArg2, secArg1, tex1.a);
			}
			else if (secColorOp > 8.5 && secColorOp < 9.5)
			{
				current.rgb = mix(secArg2, secArg1, current.a);
			}
			else
			{
				current.rgb = applyColorOp(secColorOp, secArg1, secArg2);
			}
		}

		if (secAlphaOp > 0.5)
		{
			float secAArg1 = tex1.a;
			float secAArg2 = current.a;
			current.a = applyAlphaOp(secAlphaOp, secAArg1, secAArg2);
		}
	}

	// --- Stages 2-3: legacy multi-stage multiply path. In the bgfx
	// backend the terrain pixel-shader branch handles cloud+noise by
	// itself (u_texcoordSelect.y > 0.5 at the top of main). Non-terrain
	// meshes (vehicles, buildings, infantry) only bind stage 0/1 and
	// inherit stale cloud/noise handles from the previous terrain draw
	// — multiplying by them turned tank turrets and GLA quad-cannon
	// tops pure black.
	// TheSuperHackers @bugfix bobtista 23/04/2026 gate the
	// stage 2/3 multiply on the terrain texcoord select so only the
	// legacy DX8 multipass path (which never takes this branch in
	// standalone — terrain uses the pixel-shader branch above) can
	// opt back in.
	if (secColorOp > 0.5 && u_texcoordSelect.y > 0.5)
	{
		current *= tex2 * tex3;
	}

	// --- Lighting ---
	// The lit path covers MODULATE/ADD and SELECTARG2 (priColorOp=2, texturing disabled).
	// priColorOp=1 (SELECTARG1) is excluded: it outputs tex0 directly as a baked-lit texture.
	bool needsLit = (priColorOp > 2.5 && priColorOp < 5.5)
	             || (priColorOp > 1.5 && priColorOp < 2.5);
	if (needsLit && u_lightingEnabled.x > 0.5)
	{
		// Material has lighting enabled. D3D's T&L pipeline REPLACES the
		// vertex color with the computed lit result. Our vertex shader
		// passes through the raw vertex attribute which is meaningless
		// for lit meshes. Recompute current as tex-only * lighting.
		vec3 matDiffuse = (u_vertexColorFlags.y > 0.5) ? diffuse.rgb : u_matDiffuse.rgb;
		vec3 matAmbient = (u_vertexColorFlags.z > 0.5) ? diffuse.rgb : u_matAmbient.rgb;
		// TheSuperHackers @feature bobtista 15/06/2026 Optional emissive boost (u_matFx.w,
		// neutral 1.0) brightens self-illuminated material so it reads and feeds bloom.
		vec3 matEmissive = (u_vertexColorFlags.w > 0.5) ? diffuse.rgb : (u_matEmissive.rgb * u_matFx.w);

		// TheSuperHackers @bugfix bobtista 05/06/2026 Guard normalize against a zero
		// normal (degenerate geometry) which would yield NaN and bleed into the color.
		float nrmLen = length(v_normal);
		vec3 nrm = (nrmLen > 1e-5) ? (v_normal / nrmLen) : vec3(0.0, 0.0, 1.0);
		// TheSuperHackers @feature bobtista 15/06/2026 View vector for Blinn-Phong
		// specular and rim lighting, both driven entirely from existing W3D material
		// data and geometry normals (no new art).
		vec3 viewDir = normalize(u_eyePos.xyz - v_worldPos);
		vec3 specAccum = vec3(0.0, 0.0, 0.0);
		// D3D fixed-function folds emissive into the material color before
		// texture-stage modulation. Adding it after sampling bleaches tinted
		// self-lit textures like the shellmap police roof lights to white.
		vec3 litColor = u_sceneAmbient.rgb * matAmbient + matEmissive;
		for (int li = 0; li < 4; ++li)
		{
			if (u_lightParams[li].w > 0.5 || u_lightDirs[li].w > 0.5)
			{
				// TheSuperHackers @bugfix bobtista 05/06/2026 Guard against a zero light
				// direction (unset but enabled slot) to avoid NaN from normalize.
				float ldirLen = length(u_lightDirs[li].xyz);
				vec3 ldir = (ldirLen > 1e-5) ? (u_lightDirs[li].xyz / ldirLen) : vec3(0.0, 0.0, 1.0);
				float atten = 1.0;
				if (u_lightParams[li].z > 0.5)
				{
					vec3 toLight = u_lightPositions[li].xyz - v_worldPos;
					float dist = length(toLight);
					ldir = (dist > 0.0001) ? (toLight / dist) : vec3(0.0, 0.0, 1.0);
					float inner = u_lightParams[li].x;
					float outer = max(u_lightParams[li].y, inner + 0.0001);
					atten = 1.0 - clamp((dist - inner) / (outer - inner), 0.0, 1.0);
				}
				float nDotL = max(0.0, dot(nrm, ldir));
				litColor += (u_lightAmbients[li].rgb * matAmbient + u_lightColors[li].rgb * nDotL * matDiffuse) * atten;
				if (nDotL > 0.0)
				{
					vec3 halfV = normalize(ldir + viewDir);
					float nDotH = max(0.0, dot(nrm, halfV));
					float shininess = max(u_matSpecular.w, 1.0);
					specAccum += u_lightColors[li].rgb * pow(nDotH, shininess) * atten;
				}
			}
		}
		vec4 litDiffuse = vec4(min(vec3_splat(1.0), litColor), u_matDiffuse.a);
		float litAlpha = current.a * u_matDiffuse.a;
		if (priColorOp > 0.5 && priColorOp < 1.5)
		{
			current = tex0;
		}
		else if (priColorOp > 1.5 && priColorOp < 2.5)
		{
			current = litDiffuse;
		}
		else if (priColorOp > 2.5 && priColorOp < 3.5)
		{
			// Keep the fixed-function alpha combiner result. Some lit decals
			// use stage 0 for recolored RGB and stage 1 only as an alpha mask;
			// recomputing alpha from tex0 draws their black padding.
			current = vec4(tex0.rgb * litDiffuse.rgb, litAlpha);
		}
		else if (priColorOp > 3.5 && priColorOp < 4.5)
		{
			current = vec4(min(tex0.rgb * litDiffuse.rgb * 2.0, vec3_splat(1.0)),
			               litAlpha);
		}
		else if (priColorOp > 4.5 && priColorOp < 5.5)
		{
			current = vec4(min(tex0.rgb + litDiffuse.rgb, vec3_splat(1.0)),
			               litAlpha);
		}
		else
		{
			float priAlphaArg1Src = u_tssOps1.y;
			vec4 priArg1 = (priArg1Src < 0.5) ? tex0 : litDiffuse;
			vec4 priArg2 = (priArg1Src < 0.5) ? litDiffuse : tex0;
			vec4 priAlphaA1 = (priAlphaArg1Src < 0.5) ? tex0 : litDiffuse;
			vec4 priAlphaA2 = (priAlphaArg1Src < 0.5) ? litDiffuse : tex0;
			current = vec4(applyColorOp(priColorOp, priArg1.rgb, priArg2.rgb),
			               applyAlphaOp(priAlphaOp, priAlphaA1.a, priAlphaA2.a));
		}

		if (secColorOp > 0.5)
		{
			if (secColorOp > 7.5 && secColorOp < 8.5)
			{
				current.rgb = mix(current.rgb, tex1.rgb, tex1.a);
			}
			else if (secColorOp > 8.5 && secColorOp < 9.5)
			{
				current.rgb = mix(current.rgb, tex1.rgb, current.a);
			}
			else
			{
				current.rgb = applyColorOp(secColorOp, tex1.rgb, current.rgb);
			}
			if (u_texcoordSelect.y > 0.5)
			{
				current *= tex2 * tex3;
			}
		}
		if (secAlphaOp > 0.5)
		{
			current.a = applyAlphaOp(secAlphaOp, tex1.a, current.a);
		}

		// TheSuperHackers @feature bobtista 15/06/2026 Add Blinn-Phong specular and a
		// fresnel rim term on top of the lit/textured result. Both are gated by u_matFx
		// strengths (0 = off) so the retail look is unchanged unless explicitly enabled.
		// Fade the additive FX on transparent and alpha-tested surfaces: billboard
		// foliage, infantry cards, and blended effect meshes have flat or grazing
		// normals that otherwise wash out to white. Solid meshes keep the full effect.
		float fxMask = (u_atestParams.y > 0.5) ? 0.0 : current.a;
		// Headroom guard: fade the additive FX as the surface approaches white, so bright
		// sun-facing roofs and glossy domes (which already sit near 1.0) cannot be pushed
		// to a flat white. Specular/rim stay visible on darker, mid-tone surfaces.
		float fxHeadroom = max(0.0, 1.0 - max(max(current.r, current.g), current.b));
		fxMask *= fxHeadroom;
		float rim = pow(1.0 - max(0.0, dot(nrm, viewDir)), u_matFx.z) * u_matFx.y;
		vec3 fxAdd = (specAccum * u_matSpecular.rgb * u_matFx.x + rim * litDiffuse.rgb) * fxMask;
		// Exposure-style soft add: brightens toward white but can never overshoot it, so
		// strong settings roll grazing-angle surfaces (tunnel walls, vehicle bodies) into
		// a smooth highlight instead of hard-clamping them to a flat white blob.
		current.rgb = 1.0 - (1.0 - current.rgb) * exp(-fxAdd);
	}
	else
	{
		// Pre-lit or unlit: vertex color contains baked lighting.
		current *= u_matDiffuse;
	}

	// Additive black texels are mathematical no-ops in the D3D8 fixed-function
	// path. Discard them before output so large additive effect meshes with
	// black matte pixels cannot contribute visible fragments on bgfx backends.
	if (u_texcoordSelect2.w > 0.5
		&& max(max(current.r, current.g), current.b) <= ADDITIVE_MATTE_EPSILON)
	{
		discard;
	}

	// --- Alpha test ---
	if (!alphaTestPass(current.a, u_atestParams.x, u_atestParams.y))
	{
		discard;
	}

	// Soft particles. Compare sorted alpha-blended fragments against the
	// readable opaque scene-depth target and fade only where particles
	// intersect world geometry.
	if (u_softParticleParams.x > 0.5)
	{
		vec2 sceneUV = gl_FragCoord.xy * u_softParticleParams.zw;
		float sceneDepth = texture2D(s_sceneDepth, sceneUV).x;
		float particleDepth = gl_FragCoord.z;
		float softFade = clamp((sceneDepth - particleDepth) * u_softParticleParams.y, 0.0, 1.0);
		current.a *= softFade;
		if (current.a <= 0.003)
		{
			discard;
		}
	}

	// TheSuperHackers @bugfix bobtista 14/06/2026 Cloud-shadow modulation,
	// gated on u_cloudParams.w. The terrain pixel-shader branch above samples
	// cloud itself and returns early, so this only runs for generic-path
	// ground draws inside the terrain pass (roads and other map decals) where
	// the cloud state is enabled. Units, buildings and effects render after
	// the terrain pass with w == 0, so they are unaffected. Matches the DX8
	// ST_ROAD_BASE_NOISE multipass that modulated the scrolling cloud texture
	// into the road colour, which the single-pass bgfx road path had dropped.
	if (u_cloudParams.w > 0.5)
	{
		current.rgb *= sampleCloudShadow(v_cloudUV);
	}

	// Grayscale output for disabled button state. Matches the D3D8 path
	// (render2d.cpp) which used D3DTOP_DOTPRODUCT3 with TFACTOR=0x80A5CA8E
	// to dot-product RGB with luminance weights (0.299, 0.587, 0.114).
	if (u_grayscaleEnable.x > 0.5)
	{
		float luma = dot(current.rgb, LUMA_WEIGHTS);
		current.rgb = vec3(luma, luma, luma);
	}

	// TheSuperHackers @bugfix bobtista 16/06/2026 Objects (units, structures) CAST into the
	// sun shadow map but do NOT receive it on themselves. A small solid caster like an
	// infantryman occludes its own near side in the shadow map, so receiving the map back
	// onto its body paints a dark self-shadow blob across the model (the "blob shadow" that
	// is not a real ground shadow). Only the terrain receives the sun shadow (handled in the
	// terrain pixel-shader branch above), so cast silhouettes still land on the ground.

	gl_FragColor = current;
}
