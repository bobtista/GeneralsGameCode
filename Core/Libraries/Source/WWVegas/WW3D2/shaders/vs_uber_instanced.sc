$input  a_position, a_normal, a_color0, a_texcoord0, a_texcoord1, i_data0, i_data1, i_data2, i_data3
$output v_color0, v_texcoord0, v_texcoord1, v_normal, v_cloudUV, v_stage0UV, v_stage1UV, v_stage2UV, v_sceneDepth, v_worldPos

#include <bgfx_shader.sh>

uniform vec4 u_texcoordSelect;
uniform vec4 u_shroudParams;

// TheSuperHackers @performance bobtista 15/06/2026 Packed per-draw material uniforms.
// Index order MUST match MaterialUniformSlot in BgfxBackend.cpp.
uniform vec4 u_material[25];
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

void main()
{
	// TheSuperHackers @bugfix bobtista 02/07/2026 i_data0-3 must use bgfx's canonical descending
	// TEXCOORD7-4 semantics in varying.def.sc (which cannot hold comments). The D3D11 backend
	// hardcodes TEXCOORD7=i_data0 down to TEXCOORD4=i_data3 in its input layout; the previous
	// ascending TEXCOORD6-9 mapping swapped matrix columns 0/1 and left columns 2/3 unbound,
	// garbling instanced world transforms on DX11. Metal binds by attribute name, unaffected.
	mat4 worldMtx = mtxFromCols(i_data0, i_data1, i_data2, i_data3);

	vec3 position = a_position;
	if (u_zBias.y != 0.0)
	{
		position += a_normal * u_zBias.y;
	}
	vec4 worldPos = mul(worldMtx, vec4(position, 1.0));
	gl_Position = mul(u_viewProj, worldPos);
	gl_Position.z -= u_zBias.x * gl_Position.w;
	// TheSuperHackers @bugfix bobtista 05/06/2026 Near-plane guard removed; DX11
	// near-plane clipping is restored by disabling depth-clamp (see vs_uber.sc).

	v_color0    = (u_vertexColorFlags.x > 0.5) ? a_color0.bgra : vec4_splat(1.0);
	v_texcoord0 = a_texcoord0;
	v_texcoord1 = a_texcoord1;
	v_stage0UV  = (u_texcoordSelect.x > 0.5) ? a_texcoord1 : a_texcoord0;
	v_stage1UV  = (u_texcoordSelect2.x > 0.5) ? a_texcoord1 : a_texcoord0;
	v_stage2UV  = a_texcoord0;
	v_sceneDepth = vec4(1.0, 1.0, 0.0, 0.0);
	v_normal    = mul(worldMtx, vec4(a_normal, 0.0)).xyz;
	v_worldPos = worldPos.xyz;

	if (u_texcoordSelect.w > 0.5)
	{
		vec2 sourceUV = (u_texcoordSelect.x > 0.5) ? a_texcoord1 : a_texcoord0;
		vec4 source = vec4(sourceUV, 1.0, 1.0);
		if (u_texcoordSource.x > 0.5 && u_texcoordSource.x < 1.5)
		{
			source = vec4(normalize(mul(u_view, vec4(v_normal, 0.0)).xyz), 1.0);
		}
		else if (u_texcoordSource.x > 1.5)
		{
			vec3 cameraPos = mul(u_view, worldPos).xyz;
			vec3 cameraNormal = normalize(mul(u_view, vec4(v_normal, 0.0)).xyz);
			if (u_texcoordSource.x < 2.5)
			{
				source = vec4(reflect(normalize(cameraPos), cameraNormal), 1.0);
			}
			else
			{
				source = vec4(cameraPos, 1.0);
			}
		}
		float u0 = dot(u_texTransform0, source);
		float v0 = dot(u_texTransform1, source);
		if (u_texProjected.x > 0.5)
		{
			v_sceneDepth.x = dot(u_texTransform0Z, source);
		}
		v_stage0UV = vec2(u0, v0);
	}
	if (u_texcoordSelect2.y > 0.5)
	{
		vec2 sourceUV = (u_texcoordSelect2.x > 0.5) ? a_texcoord1 : a_texcoord0;
		vec4 source = vec4(sourceUV, 1.0, 1.0);
		if (u_texcoordSource.y > 0.5 && u_texcoordSource.y < 1.5)
		{
			source = vec4(normalize(mul(u_view, vec4(v_normal, 0.0)).xyz), 1.0);
		}
		else if (u_texcoordSource.y > 1.5)
		{
			vec3 cameraPos = mul(u_view, worldPos).xyz;
			vec3 cameraNormal = normalize(mul(u_view, vec4(v_normal, 0.0)).xyz);
			if (u_texcoordSource.y < 2.5)
			{
				source = vec4(reflect(normalize(cameraPos), cameraNormal), 1.0);
			}
			else
			{
				source = vec4(cameraPos, 1.0);
			}
		}
		float u1 = dot(u_tex1Transform0, source);
		float v1 = dot(u_tex1Transform1, source);
		if (u_texProjected.y > 0.5)
		{
			v_sceneDepth.y = dot(u_tex1TransformZ, source);
		}
		v_stage1UV = vec2(u1, v1);
	}
	if (u_texcoordSource.z > 0.5 || u_tex2Transform0.x != 1.0 || u_tex2Transform1.y != 1.0
		|| u_tex2Transform0.y != 0.0 || u_tex2Transform1.x != 0.0
		|| u_tex2Transform0.z != 0.0 || u_tex2Transform1.z != 0.0
		|| u_tex2Transform0.w != 0.0 || u_tex2Transform1.w != 0.0)
	{
		vec4 source = vec4(a_texcoord0, 1.0, 1.0);
		if (u_texcoordSource.z > 0.5 && u_texcoordSource.z < 1.5)
		{
			source = vec4(normalize(mul(u_view, vec4(v_normal, 0.0)).xyz), 1.0);
		}
		else if (u_texcoordSource.z > 1.5)
		{
			vec3 cameraPos = mul(u_view, worldPos).xyz;
			vec3 cameraNormal = normalize(mul(u_view, vec4(v_normal, 0.0)).xyz);
			if (u_texcoordSource.z < 2.5)
			{
				source = vec4(reflect(normalize(cameraPos), cameraNormal), 1.0);
			}
			else
			{
				source = vec4(cameraPos, 1.0);
			}
		}
		float u2 = dot(u_tex2Transform0, source);
		float v2 = dot(u_tex2Transform1, source);
		v_stage2UV = vec2(u2, v2);
	}

	if (u_texcoordSelect.z > 0.5)
	{
		vec2 shroudUV = (worldPos.xy + u_shroudParams.xy) * u_shroudParams.zw;
		v_stage0UV = shroudUV;
		v_stage1UV = shroudUV;
	}

	v_cloudUV = worldPos.xy * u_cloudParams.z + u_cloudParams.xy;
}
