$input  a_position, a_normal, a_color0, a_texcoord0, a_texcoord1, i_data0, i_data1, i_data2, i_data3
$output v_color0, v_texcoord0, v_texcoord1, v_normal, v_cloudUV, v_stage0UV, v_stage1UV, v_stage2UV, v_sceneDepth, v_worldPos

#include <bgfx_shader.sh>

uniform vec4 u_texcoordSelect;
uniform vec4 u_texcoordSelect2;
uniform vec4 u_texcoordSource;
uniform vec4 u_vertexColorFlags;
uniform vec4 u_shroudParams;
uniform vec4 u_cloudParams;
uniform vec4 u_texTransform0;
uniform vec4 u_texTransform1;
uniform vec4 u_texTransform0Z;
uniform vec4 u_tex1Transform0;
uniform vec4 u_tex1Transform1;
uniform vec4 u_tex1TransformZ;
uniform vec4 u_tex2Transform0;
uniform vec4 u_tex2Transform1;
uniform vec4 u_texProjected;
uniform vec4 u_zBias;

void main()
{
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
