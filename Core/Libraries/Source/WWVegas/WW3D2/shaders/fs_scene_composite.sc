$input v_texcoord0

// TheSuperHackers @feature bobtista 27/04/2026 Identity scene
// composite fragment shader. This is intentionally a no-op visual pass;
// future post effects can build on this without touching world submits.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);

uniform vec4 u_postParams;
uniform vec4 u_postTexelSize;

void main()
{
	vec4 color = texture2D(s_tex0, v_texcoord0);

	// u_postParams.x = sharpen amount, y = saturation, z = contrast,
	// w = edge-aware FXAA-style smoothing amount. Defaults are identity:
	// (0, 1, 1, 0).
	if (u_postParams.w > 0.001)
	{
		vec3 nw = texture2D(s_tex0, v_texcoord0 + vec2(-u_postTexelSize.x, -u_postTexelSize.y)).rgb;
		vec3 ne = texture2D(s_tex0, v_texcoord0 + vec2( u_postTexelSize.x, -u_postTexelSize.y)).rgb;
		vec3 sw = texture2D(s_tex0, v_texcoord0 + vec2(-u_postTexelSize.x,  u_postTexelSize.y)).rgb;
		vec3 se = texture2D(s_tex0, v_texcoord0 + vec2( u_postTexelSize.x,  u_postTexelSize.y)).rgb;
		vec3 lumaVec = vec3(0.299, 0.587, 0.114);
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

	float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));
	color.rgb = mix(vec3(luma, luma, luma), color.rgb, u_postParams.y);
	color.rgb = (color.rgb - vec3(0.5, 0.5, 0.5)) * u_postParams.z + vec3(0.5, 0.5, 0.5);
	gl_FragColor = vec4(clamp(color.rgb, 0.0, 1.0), color.a);
}
