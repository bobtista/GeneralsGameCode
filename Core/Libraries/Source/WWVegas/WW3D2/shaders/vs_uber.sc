$input  a_position, a_normal, a_color0, a_texcoord0, a_texcoord1
$output v_color0, v_texcoord0, v_texcoord1, v_normal, v_cloudUV, v_stage0UV, v_stage1UV, v_stage2UV, v_sceneDepth, v_worldPos

#include <bgfx_shader.sh>

uniform vec4 u_texcoordSelect;
uniform vec4 u_texcoordSelect2; // .x > 0.5 = use texcoord1 for stage 1, .y > 0.5 = stage 1 transform active
uniform vec4 u_texcoordSource; // .x: 0=mesh UV, 1=camera normal, 2=camera reflection, 3=camera position
uniform vec4 u_vertexColorFlags; // .x > 0.5 = FVF supplies COLOR0; else use D3D8's white default
uniform vec4 u_shroudParams; // xy = offset, zw = scale
uniform vec4 u_cloudParams;  // xy = scroll offset, z = stretch factor, w = enable flag
uniform vec4 u_texTransform0; // stage-0 texture matrix column for u': dot(source xyzw)
uniform vec4 u_texTransform1; // stage-0 texture matrix column for v': dot(source xyzw)
uniform vec4 u_texTransform0Z; // stage-0 texture matrix column for w' (projected): dot(source xyzw)
uniform vec4 u_tex1Transform0; // stage-1 texture matrix column for u': dot(source xyzw)
uniform vec4 u_tex1Transform1; // stage-1 texture matrix column for v': dot(source xyzw)
uniform vec4 u_tex1TransformZ; // stage-1 texture matrix column for w' (projected): dot(source xyzw)
uniform vec4 u_tex2Transform0; // stage-2 texture matrix column for u': dot(source xyzw)
uniform vec4 u_tex2Transform1; // stage-2 texture matrix column for v': dot(source xyzw)
uniform vec4 u_texProjected; // .x > 0.5 = stage 0 D3DTTFF_PROJECTED, .y same for stage 1
uniform vec4 u_zBias;        // .x = post-projection clip-space Z offset. .y = object-space normal offset for authored coplanar opposite faces.

void main()
{
	vec3 position = a_position;
	if (u_zBias.y != 0.0)
	{
		position += a_normal * u_zBias.y;
	}
	gl_Position = mul(u_modelViewProj, vec4(position, 1.0));
	// TheSuperHackers @bugfix bobtista 30/04/2026 Apply post-projection Z
	// bias the same way D3DRS_ZBIAS pulls geometry toward the camera in DX8.
	// gl_Position.z is in clip space ahead of the perspective divide, so
	// scaling the offset by .w keeps the NDC bias roughly constant across
	// depths. Backend leaves u_zBias.x at 0 for normal draws.
	gl_Position.z -= u_zBias.x * gl_Position.w;

	v_color0    = (u_vertexColorFlags.x > 0.5) ? a_color0.bgra : vec4_splat(1.0);
	v_texcoord0 = a_texcoord0;
	v_texcoord1 = a_texcoord1;
	v_stage0UV  = (u_texcoordSelect.x > 0.5) ? a_texcoord1 : a_texcoord0;
	v_stage1UV  = (u_texcoordSelect2.x > 0.5) ? a_texcoord1 : a_texcoord0;
	v_stage2UV  = a_texcoord0;
	v_sceneDepth = vec4(1.0, 1.0, 0.0, 0.0);
	v_normal    = mul(u_model[0], vec4(a_normal, 0.0)).xyz;
	vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
	v_worldPos = worldPos.xyz;

	if (u_texcoordSelect.w > 0.5)
	{
		vec2 sourceUV = (u_texcoordSelect.x > 0.5) ? a_texcoord1 : a_texcoord0;
		// TheSuperHackers @bugfix bobtista 27/04/2026 W3D's 2D mappers
		// encode atlas offsets in the texture matrix's third component.
		// Feed z=1 for mesh UV transforms so animated tread/wheel
		// mappers land on the same texels as the DX8 fixed-function path.
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

	// Shroud pass: compute UV from world position using offset+scale.
	// The D3D8 path uses TCI_CAMERASPACEPOSITION with a texture matrix
	// inv(view)*offset*scale, but view cancels with camera-space input.
	// The net result is UV = (worldPos.xy + offset) * scale.
	if (u_texcoordSelect.z > 0.5)
	{
		vec2 shroudUV = (worldPos.xy + u_shroudParams.xy) * u_shroudParams.zw;
		v_stage0UV = shroudUV;
		v_stage1UV = shroudUV;
	}

	// Cloud shadow UV: terrain-scrolling cloud texture applied over the
	// base color. Matches the DX8 2-stage path in W3DShaderManager where
	// D3DTS_TEXTURE0 is inv(view) * scale + translate(xOffset, yOffset)
	// with TCI_CAMERASPACEPOSITION — equivalent to world-space XY times
	// STRETCH_FACTOR plus an animating translation.
	v_cloudUV = worldPos.xy * u_cloudParams.z + u_cloudParams.xy;

}
