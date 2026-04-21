$input v_color0, v_texcoord0

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 lit textured
// fragment shader. Samples stage 0 and multiplies by the interpolated
// vertex diffuse. Equivalent to D3DTOP_MODULATE with
// (D3DTA_TEXTURE, D3DTA_DIFFUSE) which is the most common preset in the
// game (see PHASE4_INVENTORY.md).

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);

void main()
{
	vec4 texColor = texture2D(s_tex0, v_texcoord0);
	gl_FragColor  = texColor * v_color0;
}
