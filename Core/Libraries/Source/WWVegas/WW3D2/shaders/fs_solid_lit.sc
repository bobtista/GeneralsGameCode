$input v_color0

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 lit solid
// fragment shader. Outputs the interpolated vertex diffuse tinted by
// the material diffuse so untextured meshes can still pick up team
// colors. Equivalent to D3DTOP_SELECTARG1(D3DTA_DIFFUSE) but with the
// W3D vertex material diffuse multiplied in.

#include <bgfx_shader.sh>

uniform vec4 u_matDiffuse;

void main()
{
	gl_FragColor = v_color0 * u_matDiffuse;
}
