$input v_color0, v_texcoord0, v_texcoord1, v_normal, v_lightspace, v_cloudUV

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);
SAMPLER2D(s_tex1, 1);
SAMPLER2D(s_tex2, 2);
SAMPLER2D(s_tex3, 3);
// Phase 4I.2 CSM: D16 shadow map with hardware PCF comparison.
SAMPLER2DSHADOW(s_shadowMap, 4);
// Terrain cloud-shadow scroll texture (BASE_NOISE1/NOISE12 paths on DX8).
SAMPLER2D(s_cloudMap, 5);
uniform vec4 u_matDiffuse;
uniform vec4 u_matEmissive; // material emissive / self-illumination color
uniform vec4 u_atestParams;
uniform vec4 u_tssOps0; // (priColorOp, priAlphaOp, secColorOp, secAlphaOp)
uniform vec4 u_tssOps1; // (priColorArg1Src, priAlphaArg1Src, secColorArg1Src, secAlphaArg1Src)
uniform vec4 u_lightDirs[4];    // per-light direction (xyz=toward light, w=enabled)
uniform vec4 u_lightColors[4]; // per-light diffuse color (rgb)
uniform vec4 u_sceneAmbient;   // scene ambient color (rgb)
uniform vec4 u_lightingEnabled; // .x > 0.5 = apply N.L lighting; else vertex is pre-lit
uniform vec4 u_texcoordSelect; // .x > 0.5 = use v_texcoord1 for stage 0 sampling
uniform vec4 u_grayscaleEnable; // .x > 0.5 = convert final color to luminance (disabled button state)
uniform vec4 u_cloudParams; // xy = scroll, z = stretch, w > 0.5 = modulate cloud into output

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

// Shadow map size — must match kShadowMapResolution in BgfxBackend.cpp.
#define SHADOW_MAP_RESOLUTION 4096.0
// Shadow map depth bias. Terrain uses a larger value to kill self-shadow acne on near-flat slopes; general meshes need a tighter value so curved / thin geometry keeps its contact shadow.
#define SHADOW_BIAS_TERRAIN 0.005
#define SHADOW_BIAS_GENERAL 0.002
// BT.601 luminance weights (matches the BGRA bytes of the D3D8 TFACTOR=0x80A5CA8E cascade used by the disabled-button grayscale path).
#define LUMA_WEIGHTS vec3(0.299, 0.587, 0.114)
// Multiplier applied to shadowed pixels. 1.0 = unshadowed, 0.0 = fully black; we darken to 60% for visible but not crushed shadows.
#define SHADOW_DARKNESS 0.6

vec3 applyColorOp(float op, vec3 arg1, vec3 arg2)
{
	// SELECTARG1
	if (op < 1.5) return arg1;
	// SELECTARG2
	if (op < 2.5) return arg2;
	// MODULATE
	if (op < 3.5) return arg1 * arg2;
	// MODULATE2X
	if (op < 4.5) return min(arg1 * arg2 * 2.0, vec3_splat(1.0));
	// ADD
	if (op < 5.5) return arg1 + arg2;
	// ADDSIGNED
	if (op < 6.5) return arg1 + arg2 - vec3_splat(0.5);
	// SUBTRACT
	if (op < 7.5) return arg1 - arg2;
	// BLENDTEXALPHA — handled specially by caller
	// BLENDCURALPHA — handled specially by caller
	// ADDSMOOTH
	if (op > 9.5 && op < 10.5) return arg1 + arg2 - arg1 * arg2;
	return arg1;
}

float applyAlphaOp(float op, float arg1, float arg2)
{
	if (op < 1.5) return arg1;
	if (op < 2.5) return arg2;
	if (op < 3.5) return arg1 * arg2;
	if (op < 4.5) return min(arg1 * arg2 * 2.0, 1.0);
	if (op < 5.5) return arg1 + arg2;
	if (op < 6.5) return arg1 + arg2 - 0.5;
	if (op < 7.5) return arg1 - arg2;
	// BLENDTEXALPHA / BLENDCURALPHA (8, 9) — handled specially by caller.
	// ADDSMOOTH — emitted by secAlphaOp under DETAILALPHA_INVSCALE; keep in sync with applyColorOp.
	if (op > 9.5 && op < 10.5) return arg1 + arg2 - arg1 * arg2;
	return arg1;
}

// 4-tap PCF sample of the shadow map. Returns a shadow factor in [SHADOW_DARKNESS, 1]. Same logic for terrain and general meshes; only the bias differs.
float sampleShadow(vec2 shadowUV, float refZ, float bias)
{
	float biasedZ = refZ - bias;
	float texelSize = 1.0 / SHADOW_MAP_RESOLUTION;
	float s0 = shadow2D(s_shadowMap, vec3(shadowUV + vec2(-0.5, -0.5) * texelSize, biasedZ));
	float s1 = shadow2D(s_shadowMap, vec3(shadowUV + vec2( 0.5, -0.5) * texelSize, biasedZ));
	float s2 = shadow2D(s_shadowMap, vec3(shadowUV + vec2(-0.5,  0.5) * texelSize, biasedZ));
	float s3 = shadow2D(s_shadowMap, vec3(shadowUV + vec2( 0.5,  0.5) * texelSize, biasedZ));
	float visibility = (s0 + s1 + s2 + s3) * 0.25;
	return mix(SHADOW_DARKNESS, 1.0, visibility);
}

void main()
{
	vec4 diffuse = v_color0;

	// --- Terrain pixel shader path ---
	// The D3D8 terrain system uses a hardware pixel shader (terrain.nvp)
	// that completely replaces the TSS pipeline:
	//   tex t0              ; sample tex0 with UV set 0
	//   tex t1              ; sample tex1 with UV set 1
	//   lrp r0, v0.a, t1, t0  ; mix(t0, t1, vertex_alpha)
	//   mul r0, r0, v0        ; multiply by diffuse (baked lighting)
	if (u_texcoordSelect.y > 0.5)
	{
		vec4 baseTex  = texture2D(s_tex0, v_texcoord0);
		vec4 blendTex = texture2D(s_tex1, v_texcoord1);
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
		if (tshadowUV.x >= 0.0 && tshadowUV.x <= 1.0
			&& tshadowUV.y >= 0.0 && tshadowUV.y <= 1.0
			&& trefZ >= 0.0 && trefZ <= 1.0)
		{
			result.rgb *= sampleShadow(tshadowUV, trefZ, SHADOW_BIAS_TERRAIN);
		}

		gl_FragColor = result;
		return;
	}

	vec4 tex0 = texture2D(s_tex0, v_texcoord0);
	vec4 tex1 = texture2D(s_tex1, v_texcoord0);
	vec4 tex2 = texture2D(s_tex2, v_texcoord0);
	vec4 tex3 = texture2D(s_tex3, v_texcoord0);

	// --- Stage 0: Primary (texture0 vs diffuse) ---
	float priColorOp = u_tssOps0.x;
	float priAlphaOp = u_tssOps0.y;
	float priArg1Src = u_tssOps1.x;
	float priAlphaArg1Src = u_tssOps1.y;

	// Determine arg1/arg2 for primary stage
	vec4 priArg1 = (priArg1Src < 0.5) ? tex0 : diffuse;
	vec4 priArg2 = (priArg1Src < 0.5) ? diffuse : tex0;
	vec4 priAlphaA1 = (priAlphaArg1Src < 0.5) ? tex0 : diffuse;
	vec4 priAlphaA2 = (priAlphaArg1Src < 0.5) ? diffuse : tex0;

	vec3 priColor;
	float priAlpha;

	if (priColorOp < 0.5)
	{
		// DISABLE — pass through diffuse (no texture)
		priColor = diffuse.rgb;
		priAlpha = diffuse.a;
	}
	else
	{
		priColor = applyColorOp(priColorOp, priArg1.rgb, priArg2.rgb);
		priAlpha = applyAlphaOp(priAlphaOp, priAlphaA1.a, priAlphaA2.a);
	}

	vec4 current = vec4(priColor, priAlpha);

	// --- Stage 1: Secondary/Detail (texture1 vs primary result) ---
	float secColorOp = u_tssOps0.z;
	float secAlphaOp = u_tssOps0.w;

	if (secColorOp > 0.5)
	{
		vec3 secArg1 = tex1.rgb;
		vec3 secArg2 = current.rgb;

		// Handle BLENDTEXTUREALPHA and BLENDCURRENTALPHA specially
		if (secColorOp > 7.5 && secColorOp < 8.5)
		{
			// BLENDTEXTUREALPHA: lerp(arg2, arg1, tex1.a)
			current.rgb = mix(secArg2, secArg1, tex1.a);
		}
		else if (secColorOp > 8.5 && secColorOp < 9.5)
		{
			// BLENDCURRENTALPHA: lerp(arg2, arg1, current.a)
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
		vec4 texOnly = tex0;
		if (secColorOp > 0.5)
		{
			texOnly *= tex1;
			// Same terrain-only gate as the non-lit branch above.
			if (u_texcoordSelect.y > 0.5)
				texOnly *= tex2 * tex3;
		}

		vec3 nrm = normalize(v_normal);
		vec3 litColor = u_sceneAmbient.rgb * u_matDiffuse.rgb;
		for (int li = 0; li < 4; ++li)
		{
			if (u_lightDirs[li].w > 0.5)
			{
				vec3  ldir = normalize(u_lightDirs[li].xyz);
				float nDotL = max(0.0, dot(nrm, ldir));
				litColor += u_lightColors[li].rgb * u_matDiffuse.rgb * nDotL;
			}
		}
		current = vec4(texOnly.rgb * min(vec3_splat(1.0), litColor),
		               texOnly.a * u_matDiffuse.a);
	}
	else
	{
		// Pre-lit or unlit: vertex color contains baked lighting.
		current *= u_matDiffuse;
	}

	// Self-illumination / emissive. D3D fixed-function adds the material
	// emissive on top of the lit output, which is how self-glowing meshes
	// (like SCMoveHint.w3d "move here" indicator) get their color when
	// no light reaches them.
	current.rgb += u_matEmissive.rgb;

	// --- Alpha test ---
	if (u_atestParams.x > 0.0 && current.a < u_atestParams.x)
	{
		discard;
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
	if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0
		&& shadowUV.y >= 0.0 && shadowUV.y <= 1.0
		&& refZ >= 0.0 && refZ <= 1.0)
	{
		current.rgb *= sampleShadow(shadowUV, refZ, SHADOW_BIAS_GENERAL);
	}

	// Cloud-shadow modulation. DX8 ST_TERRAIN_BASE_NOISE1 / _NOISE12
	// renders a second pass with the cloud texture sampled in camera-space
	// (via D3DTS_TEXTURE0 = inv(view) * scale + translate(scroll)) and
	// multiplies it into the base color. We pre-compute the UV in the
	// vertex shader (world-space XY * stretch + scroll offset) and
	// modulate here. Enabled per-draw by the backend — grass/tree draws
	// leave u_cloudParams.w = 0.
	// TheSuperHackers @bugfix bobtista 24/04/2026 Phase 5.2 — cloud shadow
	// modulate with a floor. The raw `current *= cloudSample` path wipes
	// pixels to pure black wherever the cloud texture has near-zero RGB,
	// producing the scrolling black bands on the beach and black
	// motorbike wheels. Real D3D8 cloud shadows darken by at most ~50%;
	// clamp the multiplier so the darkest shadow preserves 50% of the
	// original terrain brightness. Also guard against sample returning
	// garbage zeros (upload edge cases, sampler border reads) by taking
	// max() against a safe floor.
	if (u_cloudParams.w > 0.5)
	{
		vec3 cloudSample = texture2D(s_cloudMap, v_cloudUV).rgb;
		current.rgb *= max(cloudSample, vec3_splat(0.75));
	}

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
