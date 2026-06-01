$input v_sceneDepth

// TheSuperHackers @feature bobtista 27/04/2026 Write normalized scene
// depth into an R32F render target for future post effects and particles.

#include <bgfx_shader.sh>

void main()
{
	gl_FragColor = vec4(v_sceneDepth.x, v_sceneDepth.x, v_sceneDepth.x, 1.0);
}
