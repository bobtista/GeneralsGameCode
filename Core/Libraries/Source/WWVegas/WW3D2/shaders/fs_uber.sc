$input v_color0, v_texcoord0, v_normal

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);
SAMPLER2D(s_tex1, 1);
SAMPLER2D(s_tex2, 2);
SAMPLER2D(s_tex3, 3);
uniform vec4 u_matDiffuse;
uniform vec4 u_atestParams;
uniform vec4 u_tssOps0; // (priColorOp, priAlphaOp, secColorOp, secAlphaOp)
uniform vec4 u_tssOps1; // (priColorArg1Src, priAlphaArg1Src, secColorArg1Src, secAlphaArg1Src)

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

vec3 applyColorOp(float op, vec3 arg1, vec3 arg2)
{
	// SELECTARG1
	if (op < 1.5) return arg1;
	// SELECTARG2
	if (op < 2.5) return arg2;
	// MODULATE
	if (op < 3.5) return arg1 * arg2;
	// MODULATE2X
	if (op < 4.5) return arg1 * arg2 * 2.0;
	// ADD
	if (op < 5.5) return arg1 + arg2;
	// ADDSIGNED
	if (op < 6.5) return arg1 + arg2 - vec3_splat(0.5);
	// SUBTRACT
	if (op < 7.5) return arg1 - arg2;
	// BLENDTEXALPHA — handled specially by caller
	// BLENDCURALPHA — handled specially by caller
	// ADDSMOOTH
	if (op < 10.5) return arg1 + arg2 - arg1 * arg2;
	return arg1;
}

float applyAlphaOp(float op, float arg1, float arg2)
{
	if (op < 1.5) return arg1;
	if (op < 2.5) return arg2;
	if (op < 3.5) return arg1 * arg2;
	if (op < 4.5) return arg1 * arg2 * 2.0;
	if (op < 5.5) return arg1 + arg2;
	if (op < 6.5) return arg1 + arg2 - 0.5;
	if (op < 7.5) return arg1 - arg2;
	return arg1;
}

void main()
{
	vec4 tex0 = texture2D(s_tex0, v_texcoord0);
	vec4 tex1 = texture2D(s_tex1, v_texcoord0);
	vec4 tex2 = texture2D(s_tex2, v_texcoord0);
	vec4 tex3 = texture2D(s_tex3, v_texcoord0);
	vec4 diffuse = v_color0;

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

	// --- Stages 2-3: simple multiply (engine rarely uses independent ops here) ---
	current *= tex2 * tex3;

	// --- Lighting ---
	if (priColorOp > 2.5 && priColorOp < 5.5)
	{
		vec3  sunDir   = normalize(vec3(0.35, 0.55, 0.75));
		float ambient  = 0.45;
		float nDotL    = max(0.0, dot(normalize(v_normal), sunDir));
		float lighting = min(1.0, ambient + nDotL * 0.75);
		current.rgb *= lighting;
	}

	// --- Material diffuse tint (team colors, opacity) ---
	current *= u_matDiffuse;

	// --- Alpha test ---
	if (u_atestParams.x > 0.0 && current.a < u_atestParams.x)
	{
		discard;
	}

	gl_FragColor = current;
}
