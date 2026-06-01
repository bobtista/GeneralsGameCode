$input  a_position
$output v_texcoord0

// TheSuperHackers @feature bobtista 27/04/2026 Fullscreen scene
// composite vertex shader. The scene color target is sampled back to
// the swapchain before UI draws, creating the post-processing hook.

#include <bgfx_shader.sh>

void main()
{
	gl_Position = vec4(a_position, 1.0);
	v_texcoord0 = vec2(a_position.x * 0.5 + 0.5, 0.5 - a_position.y * 0.5);
}
