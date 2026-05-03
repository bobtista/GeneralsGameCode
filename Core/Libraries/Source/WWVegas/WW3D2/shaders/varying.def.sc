vec4 v_color0    : COLOR0    = vec4(1.0, 1.0, 1.0, 1.0);
vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);
vec2 v_texcoord1 : TEXCOORD1 = vec2(0.0, 0.0);
vec3 v_normal    : NORMAL    = vec3(0.0, 0.0, 1.0);
vec4 v_lightspace: TEXCOORD2 = vec4(0.0, 0.0, 0.0, 1.0);
vec2 v_cloudUV   : TEXCOORD3 = vec2(0.0, 0.0);
vec2 v_stage0UV  : TEXCOORD4 = vec2(0.0, 0.0);
vec2 v_stage1UV  : TEXCOORD5 = vec2(0.0, 0.0);
vec4 v_sceneDepth: TEXCOORD6 = vec4(1.0, 0.0, 0.0, 0.0);
vec3 v_worldPos  : TEXCOORD7 = vec3(0.0, 0.0, 0.0);
vec2 v_stage2UV  : TEXCOORD8 = vec2(0.0, 0.0);
vec2 v_stage3UV  : TEXCOORD9 = vec2(0.0, 0.0);

vec3 a_position  : POSITION;
vec3 a_normal    : NORMAL;
vec4 a_color0    : COLOR0;
vec2 a_texcoord0 : TEXCOORD0;
vec2 a_texcoord1 : TEXCOORD1;
