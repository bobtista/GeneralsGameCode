$input v_texcoord0

// TheSuperHackers @feature bobtista 15/06/2026 Screen-space ambient occlusion.
// Reconstructs view-space position from the scene depth target and samples a
// camera-facing hemisphere; a sample is occluded when the stored geometry at its
// screen position is closer to the camera than the sample point. Output is a
// single AO factor (1 = unoccluded).

#include <bgfx_shader.sh>

SAMPLER2D(s_sceneDepth, 1); // R32F, stores clip.z/clip.w (homogeneous depth)

uniform mat4 u_ssaoInvProj;
uniform mat4 u_ssaoProj;
uniform vec4 u_ssaoParams;    // x = radius, y = intensity, z = bias
uniform vec4 u_postTexelSize; // .xy = 1/width, 1/height

#define SSAO_SAMPLES 12

vec3 reconstructViewPos(vec2 uv, float ndcDepth)
{
	vec2 ndcXY = uv * 2.0 - 1.0;
#if !BGFX_SHADER_LANGUAGE_GLSL
	ndcXY.y = -ndcXY.y;
#endif
	vec4 clip = vec4(ndcXY, ndcDepth, 1.0);
	vec4 view = mul(u_ssaoInvProj, clip);
	return view.xyz / view.w;
}

void main()
{
	float centerDepth = texture2D(s_sceneDepth, v_texcoord0).x;
	if (centerDepth >= 0.9999 || centerDepth <= 0.0001)
	{
		gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
		return;
	}

	vec3 P = reconstructViewPos(v_texcoord0, centerDepth);
	float rnd = fract(sin(dot(v_texcoord0, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831853;
	float radius = u_ssaoParams.x;
	float bias = u_ssaoParams.z;
	float occlusion = 0.0;

	for (int i = 0; i < SSAO_SAMPLES; i++)
	{
		float a = rnd + float(i) / float(SSAO_SAMPLES) * 6.2831853;
		float r = sqrt((float(i) + 0.5) / float(SSAO_SAMPLES));
		// Hemisphere biased toward the camera (view -Z) so samples sit in front of
		// the surface; geometry that intrudes in front of a sample occludes it.
		vec3 dir = vec3(cos(a) * r, sin(a) * r, -(0.35 + 0.65 * r));
		vec3 samplePos = P + dir * radius;

		vec4 sclip = mul(u_ssaoProj, vec4(samplePos, 1.0));
		vec3 sndc = sclip.xyz / sclip.w;
		vec2 suv = sndc.xy * 0.5 + 0.5;
#if !BGFX_SHADER_LANGUAGE_GLSL
		suv.y = 1.0 - suv.y;
#endif
		if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0)
		{
			continue;
		}

		float storedDepth = texture2D(s_sceneDepth, suv).x;
		// Smaller homogeneous depth = closer to camera. Occluded when the stored
		// geometry is meaningfully closer than the sample point.
		if (sndc.z - storedDepth > bias)
		{
			vec3 storedPos = reconstructViewPos(suv, storedDepth);
			float rangeCheck = smoothstep(0.0, 1.0, radius / max(length(storedPos - P), 0.0001));
			occlusion += rangeCheck;
		}
	}

	occlusion = occlusion / float(SSAO_SAMPLES);
	float ao = clamp(1.0 - occlusion * u_ssaoParams.y, 0.0, 1.0);
	gl_FragColor = vec4(ao, ao, ao, 1.0);
}
