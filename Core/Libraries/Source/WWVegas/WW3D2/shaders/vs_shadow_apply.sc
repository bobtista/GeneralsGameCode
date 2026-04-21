$input  a_position

// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I stencil shadow
// apply vertex shader. Draws a fullscreen quad in clip space; the engine
// feeds pre-baked clip-space XYZ verts (-1..1) so we skip MVP entirely.

#include <bgfx_shader.sh>

void main()
{
	gl_Position = vec4(a_position, 1.0);
}
