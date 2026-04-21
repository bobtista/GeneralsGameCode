$input  a_position, a_normal, a_color0, a_texcoord0, a_texcoord1
$output v_color0, v_texcoord0, v_texcoord1, v_normal, v_lightspace, v_cloudUV

#include <bgfx_shader.sh>

// Phase 4I.2 CSM: per-draw model*lightView*lightProj, pre-composed
// by the backend with the CORRECT model matrix (raw world for sorted
// draws, regular world for opaque). Avoids u_model contamination
// where sorted draws bake camera view into u_model.
uniform mat4 u_shadowLightViewProj;
uniform vec4 u_texcoordSelect;
uniform vec4 u_shroudParams; // xy = offset, zw = scale
uniform vec4 u_cloudParams;  // xy = scroll offset, z = stretch factor, w = enable flag

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));

	v_color0    = a_color0.bgra;
	v_texcoord0 = a_texcoord0;
	v_texcoord1 = a_texcoord1;
	v_normal    = mul(u_model[0], vec4(a_normal, 0.0)).xyz;

	vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));

	// Shroud pass: compute UV from world position using offset+scale.
	// The D3D8 path uses TCI_CAMERASPACEPOSITION with a texture matrix
	// inv(view)*offset*scale, but view cancels with camera-space input.
	// The net result is UV = (worldPos.xy + offset) * scale.
	if (u_texcoordSelect.z > 0.5)
	{
		v_texcoord0 = (worldPos.xy + u_shroudParams.xy) * u_shroudParams.zw;
	}

	// Cloud shadow UV: terrain-scrolling cloud texture applied over the
	// base color. Matches the DX8 2-stage path in W3DShaderManager where
	// D3DTS_TEXTURE0 is inv(view) * scale + translate(xOffset, yOffset)
	// with TCI_CAMERASPACEPOSITION — equivalent to world-space XY times
	// STRETCH_FACTOR plus an animating translation.
	v_cloudUV = worldPos.xy * u_cloudParams.z + u_cloudParams.xy;

	// Phase 4I.2: light-space position using the pre-composed matrix
	// that already includes the correct model-to-world transform.
	v_lightspace = mul(u_shadowLightViewProj, vec4(a_position, 1.0));
}
