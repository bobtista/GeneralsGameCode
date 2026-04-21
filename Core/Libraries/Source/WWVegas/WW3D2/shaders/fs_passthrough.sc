$input v_color0

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4B.2 trivial
// passthrough fragment shader. Writes vertex color straight to the frame
// buffer.

#include <bgfx_shader.sh>

void main()
{
	gl_FragColor = v_color0;
}
