$input v_texcoord0

// TheSuperHackers @feature bobtista 27/04/2026 Identity scene
// composite fragment shader. This is intentionally a no-op visual pass;
// future post effects can build on this without touching world submits.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);
SAMPLER2D(s_bloom, 2);

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

// TheSuperHackers @tweak bobtista 05/06/2026 BT.601 luma weights (matches fs_uber.sc).
#define LUMA_WEIGHTS vec3(0.299, 0.587, 0.114)

vec3 acesTonemap(vec3 x)
{
	return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
	vec4 color = texture2D(s_tex0, v_texcoord0);
	vec3 rawColor = color.rgb;

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
		if (edgeRange > 0.06)
		{
			vec2 dir;
			dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
			dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
			float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.0078125, 0.001953125);
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
		graded.r += u_colorGradeParams.z * 0.10;
		graded.b -= u_colorGradeParams.z * 0.10;
		graded.g += u_colorGradeParams.w * 0.10;
		graded = graded * (graded * 0.20 + 0.85);
		graded = clamp(graded, 0.0, 1.0);
		color.rgb = mix(color.rgb, graded, u_colorGradeParams.y);
	}

	if (u_bloomParams.y > 0.001)
	{
		color.rgb += texture2D(s_bloom, v_texcoord0).rgb * u_bloomParams.y;
	}

	// Map the fully-processed scene to display range: ACES tonemap (HDR) or hard
	// clamp (LDR).
	vec3 processedOut;
	if (u_hdrParams.x > 0.5)
	{
		processedOut = acesTonemap(color.rgb);
	}
	else
	{
		processedOut = clamp(color.rgb, 0.0, 1.0);
	}

	vec3 outRgb = processedOut;
	if (u_wipeParams.y > 0.5)
	{
		// Left of the split shows the raw scene with every enhancement off (no
		// post, color grade, bloom, or HDR tonemap) for a true before/after.
		vec3 beforeOut = clamp(rawColor, 0.0, 1.0);
		float side = step(u_wipeParams.x, v_texcoord0.x);
		outRgb = mix(beforeOut, processedOut, side);
		if (abs(v_texcoord0.x - u_wipeParams.x) < u_postTexelSize.x * 1.5)
		{
			outRgb = vec3(1.0, 1.0, 1.0);
		}
	}

	gl_FragColor = vec4(outRgb, color.a);
}
