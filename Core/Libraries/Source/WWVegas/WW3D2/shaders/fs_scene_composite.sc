$input v_texcoord0

// TheSuperHackers @feature bobtista 27/04/2026 Identity scene
// composite fragment shader. This is intentionally a no-op visual pass;
// future post effects can build on this without touching world submits.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);
SAMPLER2D(s_bloom, 2);
SAMPLER2D(s_ssao, 3);

uniform vec4 u_postParams;
uniform vec4 u_postTexelSize;
// TheSuperHackers @feature bobtista 15/06/2026 u_wipeParams.x = split position
// 0..1, .y = enabled. Left of the split shows the unprocessed scene, right shows
// the processed result, for live before/after comparison of post effects.
uniform vec4 u_wipeParams;
// TheSuperHackers @feature bobtista 15/06/2026 u_colorGradeParams.x = enabled,
// .y = strength 0..1, .z = temperature -1..1 (cool..warm), .w = tint -1..1.
uniform vec4 u_colorGradeParams;
// TheSuperHackers @feature bobtista 15/06/2026 u_bloomParams.x = threshold (used
// by the bright pass), .y = intensity added over the scene here.
uniform vec4 u_bloomParams;
// TheSuperHackers @feature bobtista 15/06/2026 u_hdrParams.x = HDR enabled. When
// set, the scene target is RGBA16F and the final output is ACES-tonemapped instead
// of hard-clamped, giving highlight rolloff and richer bloom.
uniform vec4 u_hdrParams;
// TheSuperHackers @feature bobtista 15/06/2026 Cheap post effects. x = vignette
// strength, y = chromatic aberration amount, z = film grain strength, w = grain
// time seed. Each is 0 when its effect is off.
uniform vec4 u_postFx2Params;

// TheSuperHackers @tweak bobtista 05/06/2026 BT.601 luma weights (matches fs_uber.sc).
#define LUMA_WEIGHTS vec3(0.299, 0.587, 0.114)

// Highlight-rolloff knee: identity below, smooth compression above.
#define TONEMAP_KNEE 0.80
// FXAA-style tuning: edge-detect threshold and direction-reduce terms (1/128, 1/512,
// per the FXAA 3.11 reference values).
#define FXAA_EDGE_THRESHOLD 0.06
#define FXAA_REDUCE_MUL 0.0078125
#define FXAA_REDUCE_MIN 0.001953125
// Chromatic-aberration UV offset per unit amount, and color-grade curve terms.
#define CHROMA_OFFSET_SCALE 0.012
#define GRADE_CHANNEL_SHIFT 0.10
#define GRADE_CURVE_GAIN 0.20
#define GRADE_CURVE_BASE 0.85
// Wipe split line half-width in texels.
#define WIPE_LINE_TEXELS 1.5

// TheSuperHackers @tweak bobtista 15/06/2026 Gentle highlight rolloff. Identity in
// the SDR range (so the base scene is unchanged and never washed out), smoothly
// compressing only values above the knee toward 1.0 so genuine highlights do not
// clip and feed bloom cleanly. This game's art is display-referred, so a full
// filmic tonemap over-brightens it; this only touches the over-bright extremes.
vec3 tonemapHighlights(vec3 x)
{
	float knee = TONEMAP_KNEE;
	float headroom = 1.0 - knee;
	vec3 lo = min(x, vec3(knee, knee, knee));
	vec3 hi = max(x - vec3(knee, knee, knee), vec3(0.0, 0.0, 0.0));
	hi = hi / (1.0 + hi / headroom);
	return clamp(lo + hi, 0.0, 1.0);
}

void main()
{
	vec4 color = texture2D(s_tex0, v_texcoord0);
	vec3 rawColor = color.rgb;

	// Chromatic aberration: split the red/blue channels along the radial direction,
	// stronger toward the screen edges. Captured after rawColor so the wipe's
	// before side stays clean.
	if (u_postFx2Params.y > 0.001)
	{
		vec2 caOffset = (v_texcoord0 - vec2(0.5, 0.5)) * (u_postFx2Params.y * CHROMA_OFFSET_SCALE);
		color.r = texture2D(s_tex0, v_texcoord0 + caOffset).r;
		color.b = texture2D(s_tex0, v_texcoord0 - caOffset).b;
	}

	// SSAO: darken the scene by the ambient-occlusion factor (u_hdrParams.y = apply).
	if (u_hdrParams.y > 0.5)
	{
		color.rgb *= texture2D(s_ssao, v_texcoord0).r;
	}

	// u_postParams.x = sharpen amount, y = saturation, z = contrast,
	// w = edge-aware FXAA-style smoothing amount. Defaults are identity:
	// (0, 1, 1, 0).
	if (u_postParams.w > 0.001)
	{
		vec3 nw = texture2D(s_tex0, v_texcoord0 + vec2(-u_postTexelSize.x, -u_postTexelSize.y)).rgb;
		vec3 ne = texture2D(s_tex0, v_texcoord0 + vec2( u_postTexelSize.x, -u_postTexelSize.y)).rgb;
		vec3 sw = texture2D(s_tex0, v_texcoord0 + vec2(-u_postTexelSize.x,  u_postTexelSize.y)).rgb;
		vec3 se = texture2D(s_tex0, v_texcoord0 + vec2( u_postTexelSize.x,  u_postTexelSize.y)).rgb;
		vec3 lumaVec = LUMA_WEIGHTS;
		float lumaNW = dot(nw, lumaVec);
		float lumaNE = dot(ne, lumaVec);
		float lumaSW = dot(sw, lumaVec);
		float lumaSE = dot(se, lumaVec);
		float lumaM = dot(color.rgb, lumaVec);
		float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
		float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
		float edgeRange = lumaMax - lumaMin;
		if (edgeRange > FXAA_EDGE_THRESHOLD)
		{
			vec2 dir;
			dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
			dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
			float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * FXAA_REDUCE_MUL, FXAA_REDUCE_MIN);
			float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
			dir = clamp(dir * rcpDirMin, vec2(-2.0, -2.0), vec2(2.0, 2.0)) * u_postTexelSize.xy;
			vec3 aa = (texture2D(s_tex0, v_texcoord0 + dir * -0.5).rgb
				+ texture2D(s_tex0, v_texcoord0 + dir * 0.5).rgb) * 0.5;
			float aaLuma = dot(aa, lumaVec);
			if (aaLuma >= lumaMin && aaLuma <= lumaMax)
			{
				color.rgb = mix(color.rgb, aa, u_postParams.w);
			}
		}
	}

	if (u_postParams.x > 0.001)
	{
		vec3 n = texture2D(s_tex0, v_texcoord0 + vec2(0.0, -u_postTexelSize.y)).rgb;
		vec3 s = texture2D(s_tex0, v_texcoord0 + vec2(0.0,  u_postTexelSize.y)).rgb;
		vec3 e = texture2D(s_tex0, v_texcoord0 + vec2( u_postTexelSize.x, 0.0)).rgb;
		vec3 w = texture2D(s_tex0, v_texcoord0 + vec2(-u_postTexelSize.x, 0.0)).rgb;
		vec3 blur = (n + s + e + w + color.rgb) * 0.2;
		color.rgb = color.rgb + (color.rgb - blur) * u_postParams.x;
	}

	float luma = dot(color.rgb, LUMA_WEIGHTS);
	color.rgb = mix(vec3(luma, luma, luma), color.rgb, u_postParams.y);
	color.rgb = (color.rgb - vec3(0.5, 0.5, 0.5)) * u_postParams.z + vec3(0.5, 0.5, 0.5);

	if (u_colorGradeParams.x > 0.5)
	{
		vec3 graded = color.rgb;
		graded.r += u_colorGradeParams.z * GRADE_CHANNEL_SHIFT;
		graded.b -= u_colorGradeParams.z * GRADE_CHANNEL_SHIFT;
		graded.g += u_colorGradeParams.w * GRADE_CHANNEL_SHIFT;
		graded = graded * (graded * GRADE_CURVE_GAIN + GRADE_CURVE_BASE);
		graded = clamp(graded, 0.0, 1.0);
		color.rgb = mix(color.rgb, graded, u_colorGradeParams.y);
	}

	// Map the scene to display range first: highlight rolloff (HDR) or hard clamp
	// (LDR).
	vec3 processedOut;
	if (u_hdrParams.x > 0.5)
	{
		processedOut = tonemapHighlights(color.rgb);
	}
	else
	{
		processedOut = clamp(color.rgb, 0.0, 1.0);
	}

	// TheSuperHackers @tweak bobtista 15/06/2026 Add bloom AFTER tonemap/clamp so the
	// HDR highlight rolloff does not compress the glow back down - bloom is additive
	// light on top of the display-range image.
	if (u_bloomParams.y > 0.001)
	{
		processedOut += texture2D(s_bloom, v_texcoord0).rgb * u_bloomParams.y;
	}
	processedOut = clamp(processedOut, 0.0, 1.0);

	// Vignette: darken toward the screen corners.
	if (u_postFx2Params.x > 0.001)
	{
		vec2 vd = v_texcoord0 - vec2(0.5, 0.5);
		float vig = 1.0 - dot(vd, vd) * (u_postFx2Params.x * 2.0);
		processedOut *= clamp(vig, 0.0, 1.0);
	}

	// Film grain: add animated per-pixel noise (time seed in .w keeps it moving).
	if (u_postFx2Params.z > 0.001)
	{
		float grain = fract(sin(dot(v_texcoord0 * (u_postFx2Params.w + 1.0), vec2(12.9898, 78.233))) * 43758.5453);
		processedOut += (grain - 0.5) * u_postFx2Params.z;
		processedOut = clamp(processedOut, 0.0, 1.0);
	}

	vec3 outRgb = processedOut;
	if (u_wipeParams.y > 0.5)
	{
		// Left of the split shows the raw scene with every enhancement off (no
		// post, color grade, bloom, or HDR tonemap) for a true before/after.
		vec3 beforeOut = clamp(rawColor, 0.0, 1.0);
		float side = step(u_wipeParams.x, v_texcoord0.x);
		outRgb = mix(beforeOut, processedOut, side);
		if (abs(v_texcoord0.x - u_wipeParams.x) < u_postTexelSize.x * WIPE_LINE_TEXELS)
		{
			outRgb = vec3(1.0, 1.0, 1.0);
		}
	}

	gl_FragColor = vec4(outRgb, color.a);
}
