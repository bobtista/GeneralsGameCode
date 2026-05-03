$input v_color0, v_texcoord0, v_texcoord1, v_normal, v_lightspace, v_cloudUV, v_stage0UV, v_stage1UV, v_sceneDepth, v_worldPos

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);
SAMPLER2D(s_tex1, 1);
SAMPLER2D(s_tex2, 2);
SAMPLER2D(s_tex3, 3);
// Phase 4I.2 CSM: D16 shadow map with hardware PCF comparison.
SAMPLER2DSHADOW(s_shadowMap, 4);
// Terrain cloud-shadow scroll texture (BASE_NOISE1/NOISE12 paths on DX8).
SAMPLER2D(s_cloudMap, 5);
SAMPLER2D(s_sceneDepth, 6);
uniform vec4 u_matDiffuse;
uniform vec4 u_matAmbient; // material ambient color
uniform vec4 u_matEmissive; // material emissive / self-illumination color
uniform vec4 u_atestParams;
uniform vec4 u_tssOps0; // (priColorOp, priAlphaOp, secColorOp, secAlphaOp)
uniform vec4 u_tssOps1; // (priColorArg1Src, priAlphaArg1Src, secColorArg1Src, secAlphaArg1Src)
uniform vec4 u_lightDirs[4];    // per-light direction (xyz=toward light, w=enabled)
uniform vec4 u_lightColors[4]; // per-light diffuse color (rgb)
uniform vec4 u_lightAmbients[4]; // per-light ambient color (rgb)
uniform vec4 u_lightPositions[4]; // per-light world position (xyz)
uniform vec4 u_lightParams[4]; // x inner range, y outer/range, z > 0.5 point, w enabled
uniform vec4 u_sceneAmbient;   // scene ambient color (rgb)
uniform vec4 u_lightingEnabled; // .x > 0.5 = apply N.L lighting; else vertex is pre-lit
uniform vec4 u_texcoordSelect; // .x > 0.5 = use v_texcoord1 for stage 0 sampling
uniform vec4 u_texProjected; // .x > 0.5 = stage 0 projected, .y > 0.5 = stage 1 projected
uniform vec4 u_vertexColorFlags; // .y/.z/.w: diffuse/ambient/emissive source is COLOR1
uniform vec4 u_grayscaleEnable; // .x > 0.5 = convert final color to luminance (disabled button state)
uniform vec4 u_cloudParams; // xy = scroll, z = stretch, w > 0.5 = modulate cloud into output
uniform vec4 u_shadowParams; // .x > 0.5 = receive CSM shadows
uniform vec4 u_softParticleParams; // .x enable, .y fade scale, zw inverse scene size
uniform vec4 u_zBias; // .x = clip-z offset applied in the vertex shader

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

// Shadow map inverse resolution, passed from C++ via u_shadowParams.y
// to avoid hardcoding the texture size in both shader and backend.
// Shadow map depth bias. Terrain uses a larger value to kill self-shadow acne on near-flat slopes; general meshes need a tighter value so curved / thin geometry keeps its contact shadow.
#define SHADOW_BIAS_TERRAIN 0.005
#define SHADOW_BIAS_GENERAL 0.002
#define CLOUD_SHADOW_MIN 0.72
// BT.601 luminance weights (matches the BGRA bytes of the D3D8 TFACTOR=0x80A5CA8E cascade used by the disabled-button grayscale path).
#define LUMA_WEIGHTS vec3(0.299, 0.587, 0.114)
// Multiplier applied to shadowed pixels. 1.0 = unshadowed, 0.0 = fully black; we darken to 60% for visible but not crushed shadows.
#define SHADOW_DARKNESS 0.6

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

// 4-tap PCF sample of the shadow map. Returns a shadow factor in [SHADOW_DARKNESS, 1]. Same logic for terrain and general meshes; only the bias differs.
float sampleShadow(vec2 shadowUV, float refZ, float bias)
{
	float biasedZ = refZ - bias;
	float texelSize = u_shadowParams.y;
	float s0 = shadow2D(s_shadowMap, vec3(shadowUV + vec2(-0.5, -0.5) * texelSize, biasedZ));
	float s1 = shadow2D(s_shadowMap, vec3(shadowUV + vec2( 0.5, -0.5) * texelSize, biasedZ));
	float s2 = shadow2D(s_shadowMap, vec3(shadowUV + vec2(-0.5,  0.5) * texelSize, biasedZ));
	float s3 = shadow2D(s_shadowMap, vec3(shadowUV + vec2( 0.5,  0.5) * texelSize, biasedZ));
	float visibility = (s0 + s1 + s2 + s3) * 0.25;
	return mix(SHADOW_DARKNESS, 1.0, visibility);
}

vec3 sampleCloudShadow(vec2 cloudUV)
{
	vec3 cloudSample = texture2D(s_cloudMap, cloudUV).rgb;
	return max(cloudSample, vec3_splat(CLOUD_SHADOW_MIN));
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
	// --- Terrain pixel shader path ---
	// The D3D8 terrain system uses a hardware pixel shader (terrain.nvp)
	// that completely replaces the TSS pipeline:
	//   tex t0              ; sample tex0 with UV set 0
	//   tex t1              ; sample tex1 with UV set 1
	//   lrp r0, v0.a, t1, t0  ; mix(t0, t1, vertex_alpha)
	//   mul r0, r0, v0        ; multiply by diffuse (baked lighting)
	if (u_texcoordSelect.y > 0.5)
	{
		vec4 baseTex  = texture2D(s_tex0, (u_texProjected.x > 0.5) ? stage0UV : v_texcoord0);
		vec4 blendTex = texture2D(s_tex1, (u_texProjected.y > 0.5) ? stage1UV : v_texcoord1);
		float blendAlpha = diffuse.a;
		vec3 blended = mix(baseTex.rgb, blendTex.rgb, blendAlpha);
		vec4 result = vec4(blended * diffuse.rgb, 1.0);

		if (u_atestParams.x > 0.0 && result.a < u_atestParams.x)
		{
			discard;
		}

		vec3 tlsNDC = v_lightspace.xyz / v_lightspace.w;
		vec2 tshadowUV = tlsNDC.xy * 0.5 + 0.5;
#if BGFX_SHADER_LANGUAGE_HLSL
		tshadowUV.y = 1.0 - tshadowUV.y;
		float trefZ = tlsNDC.z;
#else
		float trefZ = tlsNDC.z * 0.5 + 0.5;
#endif
		if (u_shadowParams.x > 0.5
			&& tshadowUV.x >= 0.0 && tshadowUV.x <= 1.0
			&& tshadowUV.y >= 0.0 && tshadowUV.y <= 1.0
			&& trefZ >= 0.0 && trefZ <= 1.0)
		{
			result.rgb *= sampleShadow(tshadowUV, trefZ, SHADOW_BIAS_TERRAIN);
		}
		// TheSuperHackers @bugfix bobtista 28/04/2026 Apply the cloudmap
		// inside the terrain pixel-shader path. Terrain returns from this
		// branch before the generic material path below, so placing cloud
		// modulation only after the branch made the effect invisible.
		if (u_cloudParams.w > 0.5)
		{
			result.rgb *= sampleCloudShadow(v_cloudUV);
		}

		gl_FragColor = result;
		return;
	}

	vec4 tex0 = texture2D(s_tex0, stage0UV);
	vec4 tex1 = texture2D(s_tex1, stage1UV);
	vec4 tex2 = texture2D(s_tex2, v_texcoord0);
	vec4 tex3 = texture2D(s_tex3, v_texcoord0);

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
	// TheSuperHackers @bugfix bobtista 23/04/2026 Phase 5.2 gate the
	// stage 2/3 multiply on the terrain texcoord select so only the
	// legacy DX8 multipass path (which never takes this branch in
	// standalone — terrain uses the pixel-shader branch above) can
	// opt back in.
	if (secColorOp > 0.5 && u_texcoordSelect.y > 0.5)
	{
		current *= tex2 * tex3;
	}

	// --- Lighting ---
	// Extended from the original (MODULATE/ADD only) to also include
	// SELECTARG2 (priColorOp=2): when a lit mesh has texturing disabled,
	// W3D translates its shader to SELECTARG2. Our shader used to ignore
	// the lit path in that case and pass through raw vertex color —
	// which is black on meshes authored expecting D3D's T&L pipeline to
	// overwrite it (e.g. the SCMoveHint.w3d "move here" indicator).
	// priColorOp=1 (SELECTARG1) is deliberately NOT included: that case
	// is texturing_enabled + gradient_disabled, which outputs tex0
	// directly as a baked-lit texture (shell map, pre-lit terrain). Lit
	// replacement breaks those.
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
		vec3 matEmissive = (u_vertexColorFlags.w > 0.5) ? diffuse.rgb : u_matEmissive.rgb;

		vec3 nrm = normalize(v_normal);
		// D3D fixed-function folds emissive into the material color before
		// texture-stage modulation. Adding it after sampling bleaches tinted
		// self-lit textures like the shellmap police roof lights to white.
		vec3 litColor = u_sceneAmbient.rgb * matAmbient + matEmissive;
		for (int li = 0; li < 4; ++li)
		{
			if (u_lightParams[li].w > 0.5 || u_lightDirs[li].w > 0.5)
			{
				vec3 ldir = normalize(u_lightDirs[li].xyz);
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
				current *= tex2 * tex3;
		}
		if (secAlphaOp > 0.5)
		{
			current.a = applyAlphaOp(secAlphaOp, tex1.a, current.a);
		}
	}
	else
	{
		// Pre-lit or unlit: vertex color contains baked lighting.
		current *= u_matDiffuse;
	}

	// --- Alpha test ---
	if (u_atestParams.x > 0.0 && current.a < u_atestParams.x)
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

	// Phase 4I.2 CSM shadow lookup. v_lightspace is the fragment's
	// position in light clip space. Perspective divide to NDC, map to
	// [0,1] texcoords, compare against shadow map depth via
	// sampler2DShadow (hardware PCF). Darken by shadow color if
	// occluded. Skip if the fragment is outside the shadow map bounds.
	vec3 lsNDC = v_lightspace.xyz / v_lightspace.w;
	vec2 shadowUV = lsNDC.xy * 0.5 + 0.5;
#if BGFX_SHADER_LANGUAGE_HLSL
	// D3D's NDC Y is flipped vs texture Y.
	shadowUV.y = 1.0 - shadowUV.y;
	float refZ = lsNDC.z;
#else
	float refZ = lsNDC.z * 0.5 + 0.5;
#endif
	// TheSuperHackers @bugfix bobtista 27/04/2026 CSM shadows are enabled
	// per draw so UI and render-target passes do not get darkened as if
	// they were world geometry.
	if (u_shadowParams.x > 0.5
		&& shadowUV.x >= 0.0 && shadowUV.x <= 1.0
		&& shadowUV.y >= 0.0 && shadowUV.y <= 1.0
		&& refZ >= 0.0 && refZ <= 1.0)
	{
		current.rgb *= sampleShadow(shadowUV, refZ, SHADOW_BIAS_GENERAL);
	}

	// TheSuperHackers @bugfix bobtista 30/04/2026 Cloud-shadow modulation
	// is terrain-only in DX8 (ST_TERRAIN_BASE_NOISE1 / _NOISE12). The
	// terrain pixel-shader branch above handles its own cloud sampling and
	// returns before reaching this point. Don't apply cloud here — the
	// generic material path renders buildings/units/effects, none of which
	// receive cloud shadows in DX8, and v_cloudUV is undefined for them so
	// sampleCloudShadow's floor constant just darkens them uniformly.

	// Grayscale output for disabled button state. Matches the D3D8 path
	// (render2d.cpp) which used D3DTOP_DOTPRODUCT3 with TFACTOR=0x80A5CA8E
	// to dot-product RGB with luminance weights (0.299, 0.587, 0.114).
	if (u_grayscaleEnable.x > 0.5)
	{
		float luma = dot(current.rgb, LUMA_WEIGHTS);
		current.rgb = vec3(luma, luma, luma);
	}

	gl_FragColor = current;
}
