// Ronin @feature 09/10/2026 DX9: local lights in screen space. One draw per light over the screen rectangle its sphere
// covers; position and facing come from the scene depth, the falloff is the engine's own (doTheDynamicLight).

// Ronin @build 09/10/2026 DX9: fxc /T ps_3_0 /Fo LocalLight.pso LocalLight_ps.hlsl   (pairs with ScreenQuad.vso)

sampler2D g_SceneDepth : register(s0);      // INTZ, POINT, CLAMP
sampler2D g_Frame      : register(s1);      // the frame as it stood before any light, POINT, CLAMP
sampler2D g_SunShade   : register(s2);      // rgb = what the sun's shadow left of each mesh pixel (RigidInstance_ps COLOR1); a = 1 where one wrote
sampler2D g_LampDepth  : register(s4);      // the lamps' shadow maps: D24S8, LINEAR, CLAMP - the sampler compares and returns lit or not
sampler2D g_LampDepthWas : register(s6);    // ... and last frame's, for the change mask
sampler2D g_LampMap    : register(s5);      // `lights view 4` only: the same maps as colour, depth over two bytes, POINT, CLAMP
sampler2D g_GroundShade : register(s3);     // the same for the terrain, as its own shadow pass composited it; white = none taken
// Ronin @feature 20/10/2026 DX9: view 8 - the sun's shade from its shadow map, every mode: both cascades (the near one twice when one).
sampler2D g_SunDepth    : register(s8);      // depth, LINEAR, CLAMP - the sampler compares, as RigidInstance_ps's
sampler2D g_SunDepthFar : register(s9);
samplerCUBE g_SkyCube  : register(s7);      // `lights view 5` only: the map's sky cube (R3DSkyCube), looked up by a world direction

// Ronin @feature 20/10/2026 DX9: view 8's constants, c84..c95 - no other shader reads them.
float4x4 g_SunNear     : register(c84);   // view space -> the near cascade's clip (c84..c87)
float4x4 g_SunFar      : register(c88);   // view space -> the far cascade's clip (c88..c91)
float4   g_SunMap      : register(c92);   // x = near depth bias, y = half a texel in uv, z = a near texel's world size, w = cascades
float4   g_SunMapFar   : register(c93);   // x = far depth bias, y = a far texel's world size
float4   g_SunToLight  : register(c94);   // xyz = towards the sun, in view space
float4   g_SunShadeCol : register(c95);   // rgb = what a sun shadow leaves on this map, w = how far to follow it (0 = 0.45)
float4 g_Camera     : register(c0);         // x = camera near, y = camera far, z = view mode, w = overbright
float4 g_ViewPlane  : register(c1);         // x = view-plane min X, y = view-plane max Y, z = width, w = height, at depth 1
float4 g_Viewport   : register(c2);         // xy = 3D viewport min, zw = its size - both 0..1 of the render target
float4 g_TexelSize  : register(c3);         // xy = 1 / target size, z = how much of the sun's shade the lights undo (0 = none known), w = how fast a lamp restores it
float4 g_SceneLight : register(c4);         // rgb = 1 / the map's own light on open ground: frame * this ~ surface colour
float4 g_FrameAmb      : register(c44);     // Ronin @feature 25/10/2026 DX9: the map's ambient, summed; w = 1: the frame's light by each pixel's facing
float4 g_FrameLight[6] : register(c45);     // ... per light: the way to it (view space), then its diffuse
float4 g_LightPos   : register(c5);         // xyz = the light in view space, w = 1 / (outer - inner)
float4 g_LightDiff  : register(c6);         // rgb = diffuse * intensity, w = inner radius
float4 g_LightAmb   : register(c7);         // rgb = ambient * intensity, w = the cone's edge: 1 / (cos inner - cos outer)
// Ronin @bugfix 09/10/2026 DX9: the same light as it was LAST frame (zero colour = it was not there), for TAA's change mask.
float4 g_PrevPos    : register(c8);         // xyz in THIS frame's view space, w = 1 / (outer - inner)
float4 g_PrevDiff   : register(c9);         // rgb = diffuse * intensity, w = inner radius
float4 g_PrevAmb    : register(c10);        // rgb = ambient * intensity, w = the cone's edge
// Ronin @feature 09/10/2026 DX9: a spot light (the beams, Lighting_Work.md 4.2): its axis in view space and the cosine of its outer half-angle.
// A point light uploads cosine -2 and edge 1, so its cone term is 1 everywhere.
float4 g_LightAxis  : register(c11);        // xyz = the axis, away from the lamp; w = cos outer
float4 g_PrevAxis   : register(c12);        // the same, last frame, in THIS frame's view space
float4 g_Shaft      : register(c13);        // the beam in the air: x = its strength now, y = last frame's (0 = none); z = this lamp's own turn of its rays, 0..1; w = how wide the beam leaves the lamp
float4 g_Jitter     : register(c15);        // xy = how far TAA shifted this frame on screen, in viewport widths and heights (0 without TAA); z = `lights rays`
                                            // Ronin @feature 16/10/2026 DX9: w = a local light's strength in the sun, by the time of day (`daysun`)
float4 g_ShaftSun   : register(c34);        // Ronin @feature 22/10/2026 DX9: sun shafts - rgb = the map's sun on objects, w = strength per 100 units of lit air
float4 g_ShaftGap   : register(c35);        // x = how bright the cloud shadow must be to let the sun through, y = 1 clouds on, z = the cloud map's mip
float4 g_ShaftCloud : register(c36);        // x = the cloud map's scale (per world unit), yz = its drift - the ground's own (W3DShaderManager::getCloudMapState)
float4 g_CamX       : register(c37);        // view space -> world: x, y, z rows with the camera's position in w
float4 g_CamY       : register(c38);
float4 g_CamZ       : register(c39);
float4 g_ShaftTravel : register(c40);       // xyz = the way the sun's light travels, in the world
sampler2D g_CloudMap : register(s10);       // the ground's cloud shadow texture (TSCloudMed), WRAP, LINEAR
float4 g_ShaftTexel : register(c41);        // Ronin @feature 22/10/2026 DX9: xy = 1 / the beams' half-size buffer, zw = 1 / the frame
sampler2D g_ShaftBuf : register(s11);       // ... that buffer: r = the lit air in hundreds of units, g = the depth; POINT, CLAMP
// Ronin @feature 23/10/2026 DX9: the shafts' dust - c35.w = its strength (`shaftdust`), c36.w = the clock in seconds (its glints).

// Ronin @feature 23/10/2026 DX9: a hash without sine (Hoskins): cells stay distinct thousands of units out.
float2 hash22(float2 p)
{
    float3 p3 = frac(p.xyx * float3(0.1031f, 0.1030f, 0.0973f));
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.xx + p3.yz) * p3.zy);
}
float4 g_DayShade   : register(c33);        // x = its strength in a sun shadow, by the time of day (`dayshade`)
float4x4 g_LampClip : register(c16);        // a beam's shadow map: view space to its tile - atlas uv x w, depth, w
float4 g_LampTile   : register(c20);        // ... the tile in atlas uv: xy = min, zw = max; all 0 = this light has no map
float4 g_LampBias   : register(c21);        // ... x = a texel's width at depth 1; the bias: y = along the normal, z = to the lamp (texels), w = to the lamp (units)
float4 g_LampTaps   : register(c22);        // ... x = half a texel in atlas uv, y = how much the shadow takes
float4x4 g_LampClipWas : register(c23);     // the same light's map LAST frame: THIS frame's view space to its tile then
float4 g_LampTileWas : register(c27);       // ... that tile; all 0 = it had none
float4 g_LampWas    : register(c28);        // ... x = a texel's width at depth 1 in that map
float4 g_LampSides  : register(c29);        // a blast's four level maps: x = the first one's tile (+X, -X, +Y, -Y follow; below 0 = none), depth = y - z / distance, w = a texel's width at depth 1
float4 g_WorldX     : register(c30);        // the world's axes in view space, the same for every light; w = the spill's strength
float4 g_WorldY     : register(c31);        // w = 1 / the spill's width
float4 g_WorldZ     : register(c32);        // w = `lights rayturn`
float4 g_Corona     : register(c14);        // a lamp's own glow: x = radius, y = strength; z, w = last frame's (0 = none). A beam: x, z = where its shaft ends

struct PS_OUTPUT
{
    float4 color : COLOR0;                  // added to the frame
    float4 mask  : COLOR1;                  // R3DTaa::beginLightMask - how much this light changed the pixel since last frame
};

// Ronin @feature 09/10/2026 DX9: W3D view space: camera looks down -Z, +Y up. Same reconstruction as SsaoRaw_ps.
float3 viewPosition(float2 uv)
{
    float  d = tex2Dlod(g_SceneDepth, float4(uv, 0.0f, 0.0f)).r;
    float  n = g_Camera.x;
    float  f = g_Camera.y;
    float  e = (n * f) / max(f - d * (f - n), 0.0001f);
    // Ronin @bugfix 11/10/2026 DX9: the depth here belongs to the point TAA's jitter moved to this pixel: its ray is the unshifted one.
    float2 s = (uv - g_Viewport.xy) / g_Viewport.zw - g_Jitter.xy;
    float  px = g_ViewPlane.x + s.x * g_ViewPlane.z;
    float  py = g_ViewPlane.y - s.y * g_ViewPlane.w;
    return float3(px * e, py * e, -e);
}

static const int SHAFT_STEPS  = 6;
static const int SUN_SHAFT_STEPS = 16;		// Ronin @diagnostic 22/10/2026 DX9: view 9's march through the sun's far map

// Ronin @bugfix 09/10/2026 DX9: every falloff ends through this - no slope and no curvature where the light runs out. A ramp
// that stops dead reads as a dark band round the pool, and the sharpening deepens it.
float ease(float x)
{
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

// Ronin @feature 09/10/2026 DX9: a spot's cone: the art's at full strength, inside a dim, wider spill (c30.w, c31.w) - a lamp's light does not
// stop at a line (a hard disc showed on whatever a headlight touched). A point light (cosine -2) comes out as 1.
float coneTerm(float cosA, float4 axis, float edge)
{
    float core  = ease(saturate((cosA - axis.w) * edge));
    float c     = max(axis.w, 0.001f);
    float ca    = max(cosA, 0.001f);
    float u     = sqrt((max(1.0f - ca * ca, 0.0f) * c * c) / max((1.0f - c * c) * ca * ca, 0.000001f));    // tangent off the axis, in cone widths
    // Ronin @bugfix 09/10/2026 DX9: the spill is ONE curve from the axis to its end, under the core. As a second S-curve it began flat
    // exactly where the core ended: pool, shelf, edge - which read as a dark ring round the pool.
    float spill = (1.0f - ease(saturate(u * g_WorldY.w))) * step(0.0f, cosA);
    return (axis.w < -1.5f) ? 1.0f : lerp(spill * g_WorldX.w, 1.0f, core);
}

// Ronin @feature 09/10/2026 DX9: the beam in the air, with no cone texture: how much of the line of sight to this pixel lies inside the
// spot's cone before it meets the surface, weighted by the light there. Camera at the origin; entry and exit are exact.
float shaft(float3 P, float3 lamp, float end, float4 axis, float edge)
{
    float  tMax  = length(P);
    float3 v     = P / max(tMax, 0.001f);
    float  outer = end;                 // its own end, not the light's: the light on surfaces runs further
    float  inner = 0.5f * end;
    float  span  = 2.0f / max(end, 0.001f);
    float  c2    = axis.w * axis.w;
    float  tanO  = sqrt(max(1.0f - c2, 0.0f)) / max(axis.w, 0.001f);
    // Ronin @bugfix 12/10/2026 DX9: the beam leaves its lamp as wide as the art's did (c13.w), not from a point: the cone's point sits
    // that far behind the lamp, and the beam begins at the lamp's own plane.
    float  back  = g_Shaft.w / max(tanO, 0.001f);
    float3 co    = axis.xyz * back - lamp;                              // from the cone's point to the camera
    float  vd    = dot(v, axis.xyz);
    float  cod   = dot(co, axis.xyz);
    float  vco   = dot(v, co);
    float  coco  = dot(co, co);
    float  a     = vd * vd - c2;
    float  b     = 2.0f * (vd * cod - c2 * vco);
    float  c     = cod * cod - c2 * coco;
    a = (abs(a) < 0.00001f) ? ((a >= 0.0f) ? 0.00001f : -0.00001f) : a;
    float  disc  = b * b - 4.0f * a * c;
    float  sq    = sqrt(max(disc, 0.0f));
    float  r0    = (-b - sq) / (2.0f * a);
    float  r1    = (-b + sq) / (2.0f * a);
    float  t0    = min(r0, r1);
    float  t1    = max(r0, r1);
    float  ok    = (disc > 0.0f) ? 1.0f : 0.0f;
    if (a < 0.0f)
        ok *= (cod + 0.5f * (t0 + t1) * vd > 0.0f) ? 1.0f : 0.0f;     // not the other half of the double cone
    else if (vd > 0.0f)
    {
        t0 = t1;                    // looking along the beam: inside from the far root on
        t1 = 1.0e6f;
    }
    else
    {
        t1 = t0;                    // looking into the beam: inside up to the near root
        t0 = -1.0e6f;
    }
    float  vcl   = -dot(v, lamp);
    float  ds    = vcl * vcl - (dot(lamp, lamp) - outer * outer);       // and inside the light's reach, round the lamp
    ok *= (ds > 0.0f) ? 1.0f : 0.0f;
    float  sds   = sqrt(max(ds, 0.0f));
    t0 = max(t0, max(-vcl - sds, 0.0f));
    t1 = min(t1, min(-vcl + sds, tMax));
    float  tl    = dot(lamp, axis.xyz) / ((abs(vd) < 0.00001f) ? ((vd >= 0.0f) ? 0.00001f : -0.00001f) : vd);    // and past the lamp's plane
    if (vd > 0.0f)
        t0 = max(t0, tl);
    else
        t1 = min(t1, tl);
    ok *= (t1 > t0) ? 1.0f : 0.0f;
    // Ronin @feature 09/10/2026 DX9: brightest near the lamp: the light thins as 1/d^2 while the cone widens as d.
    float  s     = 0.12f * outer;
    float  sum   = 0.0f;
    for (int i = 0; i < SHAFT_STEPS; ++i)
    {
        float  t     = t0 + (t1 - t0) * ((i + 0.5f) / SHAFT_STEPS);
        float3 to    = v * t + co;                                      // from the cone's point
        float  dist  = length(to);
        float  cone  = saturate((dot(to, axis.xyz) / max(dist, 0.001f) - axis.w) * edge);
        cone = cone * cone * (3.0f - 2.0f * cone);
        float  reach = saturate(1.0f - (length(v * t - lamp) - inner) * span);
        sum += cone * reach * (s * s) / (dist * dist + s * s);
    }
    float  beam  = ok * (sum / SHAFT_STEPS) * (t1 - t0) / max(tanO * s, 0.0001f);
    // Ronin @feature 11/10/2026 DX9: `lights rays` - soft rays from the lamp: a pattern of where the line of sight crosses the plane through
    // the axis that faces the camera, in cone widths (u; a constant u is a line from the cone's point). Faint at the lamp and seen end-on.
    [branch]
    if (g_Jitter.z > 0.0f)                 // off costs nothing
    {
        float3 m     = cross(axis.xyz, co);
        float  ml    = length(m);
        m /= max(ml, 0.0001f);
        float3 n     = cross(axis.xyz, m);
        float  dn    = dot(v, n);
        float3 x     = co - v * (dot(co, n) / ((abs(dn) < 0.00001f) ? 0.00001f : dn));
        float  sa    = dot(x, axis.xyz);
        float  u     = dot(x, m) / max(sa * tanO, 0.0001f);
        float  fade  = smoothstep(0.08f, 0.35f, (sa - back) / max(end, 0.001f)) * smoothstep(0.25f, 0.6f, ml / max(sqrt(coco), 0.001f));
        // Ronin @feature 11/10/2026 DX9: each lamp turns the pattern its own way (c13.z); with `lights rayturn` it turns with the world
        // too: by the angle of the way to the camera round the axis, from a start fixed in the world.
        float3 ref   = (abs(dot(axis.xyz, g_WorldZ.xyz)) < 0.9f) ? g_WorldZ.xyz : g_WorldX.xyz;
        float3 e1    = normalize(cross(axis.xyz, ref));
        float  th    = u * 1.5708f + g_Shaft.z * 6.2832f + g_WorldZ.w * atan2(dot(co, cross(axis.xyz, e1)), dot(co, e1));
        float  rays  = 0.5f * cos(th * 3.0f + 0.7f) + 0.3f * cos(th * 5.0f + 2.1f) + 0.2f * cos(th * 8.0f + 4.4f);
        beam *= max(1.0f + g_Jitter.z * fade * rays, 0.0f);
    }
    return beam;
}

// Ronin @feature 09/10/2026 DX9: a lamp's own glow, with no texture: by how near the line of sight passes the lamp, so it is round from
// any side; gone where the surface at this pixel stands in front of the lamp.
float corona(float3 P, float3 lamp, float radius)
{
    float  tMax = length(P);
    float3 v    = P / max(tMax, 0.001f);
    float  t    = dot(lamp, v);
    float  r    = max(radius, 0.001f);
    float  g    = saturate(1.0f - length(lamp - v * t) / r);
    float  seen = saturate((tMax - t) / r + 1.0f);
    return g * g * seen * step(0.0f, t);
}

static const float LAMP_TILES_ROW = 8.0f;       // R3DLocalLights: LAMP_ATLAS / LAMP_TILE

// Ronin @feature 11/10/2026 DX9: a big point light low over the ground (a blast) has four more maps, level, a quarter turn apart. Which
// one holds a point, and where in it, is worked out by hand - one register a light, no matrix a face (scratchpad lampside_math_check.py).
struct LampSide
{
    float3 at;      // atlas uv, and the depth to compare
    float4 rect;    // that face's tile, half a texel in
    float  use;     // 1 = one of the four holds the point
};

LampSide lampSide(float3 P, float3 N, float3 lamp)
{
    LampSide o;
    float3 toL   = lamp - P;
    float  dist  = max(length(toL), 0.001f);
    toL /= dist;
    float  texel = g_LampSides.w * dist;
    float  ndl   = saturate(dot(N, toL));
    float3 dv    = P + N * (g_LampBias.y * texel * sqrt(1.0f - ndl * ndl)) + toL * (g_LampBias.z * texel + g_LampBias.w) - lamp;
    float3 d     = float3(dot(dv, g_WorldX.xyz), dot(dv, g_WorldY.xyz), dot(dv, g_WorldZ.xyz));
    bool   onX   = abs(d.x) >= abs(d.y);
    float  major = onX ? d.x : d.y;
    float  m     = max(abs(major), 0.0001f);
    float  safe  = (major < 0.0f) ? -m : m;
    float  nx    = onX ? (d.y / safe) : (-d.x / safe);
    float  ny    = d.z / m;
    float  face  = onX ? ((d.x > 0.0f) ? 0.0f : 1.0f) : ((d.y > 0.0f) ? 2.0f : 3.0f);
    float  index = g_LampSides.x + face;
    float  row   = floor((index + 0.5f) / LAMP_TILES_ROW);
    float2 origin = float2(index - row * LAMP_TILES_ROW, row) / LAMP_TILES_ROW;
    o.at   = float3(origin + float2(nx * 0.5f + 0.5f, 0.5f - ny * 0.5f) / LAMP_TILES_ROW + g_LampTaps.xx, g_LampSides.y - g_LampSides.z / m);
    o.rect = float4(origin + g_LampTaps.xx, origin + 1.0f / LAMP_TILES_ROW - g_LampTaps.xx);
    o.use  = (g_LampSides.x >= 0.0f && abs(ny) < 1.0f) ? 1.0f : 0.0f;      // within 45 degrees of level; the downward map has the rest
    return o;
}

// Ronin @feature 11/10/2026 DX9: LAMP SHADOW MAPS, the read (Lighting_Work.md 6b). 1 = the lamp sees this point. The bias is on the
// receiver: along its normal by the light's slant, and towards the lamp, in map texels at its own distance.
float lampShadow(sampler2D map, float4x4 toTile, float4 tile, float texelAt1, float3 P, float3 N, float3 lamp, LampSide side)
{
    float3 toL   = lamp - P;
    float  dist  = max(length(toL), 0.001f);
    toL /= dist;
    float  texel = texelAt1 * dist;
    float  ndl   = saturate(dot(N, toL));
    float3 Pb    = P + N * (g_LampBias.y * texel * sqrt(1.0f - ndl * ndl)) + toL * (g_LampBias.z * texel + g_LampBias.w);
    float4 c     = mul(float4(Pb, 1.0f), toTile);
    float3 s     = c.xyz / max(c.w, 0.0001f);
    // Ronin @feature 11/10/2026 DX9: outside its tile (or no tile: all zero) the lamp has no map - lit. The taps stay half a texel inside.
    float  inMap = (c.w > 0.0f && s.x > tile.x && s.x < tile.z && s.y > tile.y && s.y < tile.w) ? 1.0f : 0.0f;
    float2 lo    = tile.xy + g_LampTaps.xx;
    float2 hi    = tile.zw - g_LampTaps.xx;
    // Ronin @feature 11/10/2026 DX9: a blast's level map where one holds the point: the same four taps, in that tile.
    s     = lerp(s, side.at, side.use);
    lo    = lerp(lo, side.rect.xy, side.use);
    hi    = lerp(hi, side.rect.zw, side.use);
    inMap = max(inMap, side.use);
    float  z     = min(s.z, 1.0f);
    float  sum   = tex2Dproj(map, float4(clamp(s.xy + float2(-g_LampTaps.x, -g_LampTaps.x), lo, hi), z, 1.0f)).r
                 + tex2Dproj(map, float4(clamp(s.xy + float2( g_LampTaps.x, -g_LampTaps.x), lo, hi), z, 1.0f)).r
                 + tex2Dproj(map, float4(clamp(s.xy + float2(-g_LampTaps.x,  g_LampTaps.x), lo, hi), z, 1.0f)).r
                 + tex2Dproj(map, float4(clamp(s.xy + float2( g_LampTaps.x,  g_LampTaps.x), lo, hi), z, 1.0f)).r;
    return lerp(1.0f, sum * 0.25f, inMap * g_LampTaps.y);
}

PS_OUTPUT main(float2 uv : TEXCOORD0)
{
    PS_OUTPUT o;
    o.mask = float4(0.0f, 0.0f, 0.0f, 0.0f);
    const int view = (int)(g_Camera.z + 0.5f);
    // Ronin @feature 20/10/2026 DX9: view 8 - the sun's shade as RigidInstance_ps records it (3x3 PCF, normal offset, the N.L fade),
    // from the scene depth, for every pixel in every mode. rgb = the shade, a = 1; the sky a = 0.
    if (view == 8)
    {
        o.color = float4(1.0f, 1.0f, 1.0f, 0.0f);
        if (tex2Dlod(g_SceneDepth, float4(uv, 0.0f, 0.0f)).r > 0.99999f)
            return o;
        float3 P   = viewPosition(uv);
        float3 Px1 = viewPosition(uv + float2(g_TexelSize.x, 0.0f));
        float3 Px0 = viewPosition(uv - float2(g_TexelSize.x, 0.0f));
        float3 Py1 = viewPosition(uv + float2(0.0f, g_TexelSize.y));
        float3 Py0 = viewPosition(uv - float2(0.0f, g_TexelSize.y));
        float3 ex  = (abs(Px1.z - P.z) < abs(P.z - Px0.z)) ? (Px1 - P) : (P - Px0);     // the nearer side: no edge across
        float3 ey  = (abs(Py1.z - P.z) < abs(P.z - Py0.z)) ? (Py1 - P) : (P - Py0);
        float3 N   = normalize(cross(ex, ey));
        N = (dot(N, -P) < 0.0f) ? -N : N;
        float  ndotl  = dot(N, g_SunToLight.xyz);
        float  geo    = lerp(0.35f, 1.0f, smoothstep(0.0f, 0.35f, ndotl));
        float4 selClip = mul(float4(P, 1.0f), g_SunNear);
        float2 selUV   = selClip.xy * float2(0.5f, -0.5f) + 0.5f + g_SunMap.y;
        float  seamUV  = g_SunMap.y * 2.0f * 4.0f;
        bool   inNear  = (g_SunMap.w < 1.5f) ||
                         (selUV.x >= seamUV && selUV.x <= 1.0f - seamUV && selUV.y >= seamUV && selUV.y <= 1.0f - seamUV);
        float  texelWorld = inNear ? g_SunMap.z : g_SunMapFar.y;
        float  slope   = sqrt(saturate(1.0f - ndotl * ndotl));
        float3 S       = P + N * (texelWorld * 2.0f * slope);
        float4 clipN   = mul(float4(S, 1.0f), g_SunNear);
        float4 clipF   = mul(float4(S, 1.0f), g_SunFar);
        float2 uvN     = clipN.xy * float2(0.5f, -0.5f) + 0.5f + g_SunMap.y;
        float2 uvF     = clipF.xy * float2(0.5f, -0.5f) + 0.5f + g_SunMap.y;
        float2 sUV     = inNear ? uvN : uvF;
        o.color.a = 1.0f;
        if (sUV.x < 0.0f || sUV.x > 1.0f || sUV.y < 0.0f || sUV.y > 1.0f)
            return o;     // outside the fitted map: lit, as the meshes take it
        float texelUV = g_SunMap.y * 2.0f;
        float depthN  = clipN.z - g_SunMap.x;
        float depthF  = clipF.z - g_SunMapFar.x;
        float sumN = 0.0f, sumF = 0.0f;
        [unroll]
        for (int sy = -1; sy <= 1; ++sy)
        {
            [unroll]
            for (int sx = -1; sx <= 1; ++sx)
            {
                float2 ofs = float2(sx, sy) * texelUV;
                sumN += tex2Dproj(g_SunDepth,    float4(uvN + ofs, depthN, 1.0f)).r;
                sumF += tex2Dproj(g_SunDepthFar, float4(uvF + ofs, depthF, 1.0f)).r;
            }
        }
        float lit = min((inNear ? sumN : sumF) * (1.0f / 9.0f), geo);
        o.color.rgb = lerp(lerp(float3(0.45f, 0.45f, 0.45f), g_SunShadeCol.rgb, g_SunShadeCol.w), float3(1.0f, 1.0f, 1.0f), lit);
        return o;
    }
    // Ronin @feature 22/10/2026 DX9: views 9 and 10 - SUN SHAFTS: the air on each view ray from the surface up to g_WorldZ.w above it,
    // lit where the sun gets through a cloud gap and past the casters (the far map). 9 shows the lit share; 10 marches at half size.
    if (view == 9 || view == 10)
    {
        o.color = (view == 9) ? float4(0.03f, 0.06f, 0.14f, 0.0f) : float4(0.0f, 60000.0f, 0.0f, 1.0f);
        float2 at = uv;
        float  start;
        if (view == 10)
        {
            // Ronin @feature 22/10/2026 DX9: a half-size texel stands for the full-size pixel at its top left; its steps start by a 4x4
            // ordered pattern, all of which view 11's 4x4 blur holds - no grain. Under TAA the whole pattern moves on each frame.
            float2 idx   = floor(uv / g_ShaftTexel.xy);
            float2 lo    = fmod(idx, 2.0f);
            float2 hi    = fmod(floor(idx * 0.5f), 2.0f);
            float  order = 4.0f * (2.0f * lo.x + 3.0f * lo.y - 4.0f * lo.x * lo.y) + (2.0f * hi.x + 3.0f * hi.y - 4.0f * hi.x * hi.y);
            at    = (2.0f * idx + 0.5f) * g_ShaftTexel.zw;
            start = frac((order + 0.5f) * (1.0f / 16.0f) + g_Jitter.w * 0.618034f);
        }
        else
        {
            float2 pix = uv / g_TexelSize.xy + g_Jitter.w * 5.588238f;
            start = frac(52.9829189f * frac(dot(pix, float2(0.06711056f, 0.00583715f))));     // interleaved gradient noise
        }
        if (tex2Dlod(g_SceneDepth, float4(at, 0.0f, 0.0f)).r > 0.99999f)
            return o;
        float3 P     = viewPosition(at);
        float  rise  = dot(-P, g_WorldZ.xyz);                         // how far the camera stands above the surface
        float  tEnd  = (rise > 0.001f) ? saturate(g_WorldZ.w / rise) : 1.0f;
        float  ground = dot(g_CamZ.xyz, P) + g_CamZ.w;               // the surface's height in the world
        float  lit   = 0.0f;
        [unroll]
        for (int i = 0; i < SUN_SHAFT_STEPS; ++i)
        {
            float  t     = (i + start) * (1.0f / SUN_SHAFT_STEPS) * tEnd;
            float3 X     = P * (1.0f - t);
            float4 clip  = mul(float4(X, 1.0f), g_SunFar);
            float2 suv   = clip.xy * float2(0.5f, -0.5f) + 0.5f + g_SunMap.y;
            float  inMap = (suv.x > 0.0f && suv.x < 1.0f && suv.y > 0.0f && suv.y < 1.0f) ? 1.0f : 0.0f;
            float  sun   = lerp(1.0f, tex2Dproj(g_SunDepthFar, float4(suv, clip.z - g_SunMapFar.x, 1.0f)).r, inMap);
            // Ronin @feature 22/10/2026 DX9: the sun's ray through this air, followed down to the ground: the cloud shadow there says
            // whether a gap let it through - so a beam lands where the ground's cloud shadow is lightest.
            float3 W     = float3(dot(g_CamX.xyz, X) + g_CamX.w, dot(g_CamY.xyz, X) + g_CamY.w, dot(g_CamZ.xyz, X) + g_CamZ.w);
            float2 foot  = W.xy + g_ShaftTravel.xy * ((W.z - ground) / max(-g_ShaftTravel.z, 0.05f));
            float  cloud = dot(tex2Dlod(g_CloudMap, float4(foot * g_ShaftCloud.x + g_ShaftCloud.yz, 0.0f, g_ShaftGap.z)).rgb,
                               float3(0.299f, 0.587f, 0.114f));
            float  gap   = smoothstep(g_ShaftGap.x, g_ShaftGap.x + 0.027f, cloud);		// only above the cut, as previewed (11% open at 0.965)
            lit += sun * lerp(1.0f, gap, g_ShaftGap.y);
        }
        lit *= 1.0f / SUN_SHAFT_STEPS;
        if (view == 9)
        {
            o.color = float4(lit, lit, lit, 0.0f);
            return o;
        }
        o.color = float4(lit * tEnd * length(P) * 0.01f, -P.z, 0.0f, 1.0f);     // the lit air in hundreds of units, and the depth
        return o;
    }
    // Ronin @feature 22/10/2026 DX9: view 11 - the half-size march blurred 4x4 (every step start of the pattern), only across one depth.
    if (view == 11)
    {
        float2 c    = tex2Dlod(g_ShaftBuf, float4(uv, 0.0f, 0.0f)).rg;
        float  sum  = 0.0f;
        float  wsum = 0.0f;
        [unroll]
        for (int by = -2; by <= 1; ++by)
        {
            [unroll]
            for (int bx = -2; bx <= 1; ++bx)
            {
                float2 s = tex2Dlod(g_ShaftBuf, float4(uv + float2(bx, by) * g_ShaftTexel.xy, 0.0f, 0.0f)).rg;
                float  w = exp(-abs(s.y - c.y) / (0.02f * c.y));
                sum  += s.x * w;
                wsum += w;
            }
        }
        o.color = float4(sum / max(wsum, 0.0001f), c.y, 0.0f, 1.0f);
        return o;
    }
    // Ronin @feature 22/10/2026 DX9: view 12 - the blurred beams at full size from the four nearest texels, those at this pixel's depth
    // first, added as the sun's light by the lit air x the strength.
    if (view == 12)
    {
        o.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (tex2Dlod(g_SceneDepth, float4(uv, 0.0f, 0.0f)).r > 0.99999f)
            return o;
        float  e    = -viewPosition(uv).z;
        float2 h    = (uv / g_ShaftTexel.zw + 0.5f) * 0.5f;          // the blur centres texel j on full-size pixel 2j - 1
        float2 j0   = floor(h);
        float2 f    = h - j0;
        float  sum  = 0.0f;
        float  wsum = 0.0f;
        [unroll]
        for (int ny = 0; ny <= 1; ++ny)
        {
            [unroll]
            for (int nx = 0; nx <= 1; ++nx)
            {
                float2 s = tex2Dlod(g_ShaftBuf, float4((j0 + float2(nx, ny) + 0.5f) * g_ShaftTexel.xy, 0.0f, 0.0f)).rg;
                float  w = ((nx == 0) ? 1.0f - f.x : f.x) * ((ny == 0) ? 1.0f - f.y : f.y) * (exp(-abs(s.y - e) / (0.02f * e)) + 0.001f);
                sum  += s.x * w;
                wsum += w;
            }
        }
        o.color.rgb = g_ShaftSun.rgb * (1.0f - exp(-g_ShaftSun.w * sum / max(wsum, 0.000001f)));
        return o;
    }
    // Ronin @feature 23/10/2026 DX9: view 13 - DUST in the shafts: sheets at fixed heights in the world, a third of the lit layer apart, the
    // three in this pixel's lit air; on each a still scatter of motes (one in seven 4-unit cells), lit ones added.
    if (view == 13)
    {
        o.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (tex2Dlod(g_SceneDepth, float4(uv, 0.0f, 0.0f)).r > 0.99999f)
            return o;
        float3 P       = viewPosition(uv);
        float  ground  = dot(g_CamZ.xyz, P) + g_CamZ.w;             // the surface's height in the world
        float  drop    = min(ground - g_CamZ.w, -0.5f);             // how far the ray falls from the camera to it
        // Ronin @feature 23/10/2026 DX9: sizes are in pixels of a 900-row picture, so a mote looks the same at any resolution.
        float  pxAt1   = g_ViewPlane.z * g_TexelSize.x / max(g_Viewport.z, 0.0001f) * g_Viewport.w / (900.0f * g_TexelSize.y);
        float  spacing = max(g_WorldZ.w, 3.0f) / 3.0f;
        float  first   = ceil(ground / spacing);                    // the lowest sheet at or above the surface
        float  dustWhite = 0.0f;
        float  dustWarm  = 0.0f;
        [unroll]
        for (int k = 0; k < 3; ++k)
        {
            float  sheet = first + k;
            float  t     = (sheet * spacing - g_CamZ.w) / drop;     // where the ray crosses it: 0 = the camera, 1 = the surface
            float  ahead = (t > 0.0f && t <= 1.0f) ? 1.0f : 0.0f;
            float3 Xv    = P * clamp(t, 0.001f, 1.0f);
            float3 Xw    = float3(dot(g_CamX.xyz, Xv) + g_CamX.w, dot(g_CamY.xyz, Xv) + g_CamY.w, dot(g_CamZ.xyz, Xv) + g_CamZ.w);
            float2 c0    = floor(Xw.xy * 0.25f - 0.5f);
            float  bestWhite = 0.0f;
            float  bestWarm  = 0.0f;
            [unroll]
            for (int j = 0; j < 4; ++j)
            {
                float2 c  = c0 + float2((j == 1 || j == 3) ? 1.0f : 0.0f, (j >= 2) ? 1.0f : 0.0f);
                float2 hs = hash22(c + 131.0f * sheet);
                float2 hn = hash22(c * 1.7f + 53.0f + 7.0f * sheet);
                float2 mw = (c + 0.15f + 0.7f * float2(hs.y, saturate(hs.x / 0.14f))) * 4.0f;
                float3 dW = float3(mw - Xw.xy, 0.0f);
                float3 Mv = Xv + float3(dot(float3(g_CamX.x, g_CamY.x, g_CamZ.x), dW), dot(float3(g_CamX.y, g_CamY.y, g_CamZ.y), dW),
                                        dot(float3(g_CamX.z, g_CamY.z, g_CamZ.z), dW));
                float2 s  = (Mv.xy / -Mv.z - Xv.xy / -Xv.z) / pxAt1;    // the mote from this pixel, in pixels: round on screen
                // Ronin @feature 23/10/2026 DX9: a mote is a tiny facet, its normal random and slowly turning: it flashes where it mirrors the
                // sun into the eye - other motes as the camera moves - and is a dim speck between.
                float  nz = hn.x * 2.0f - 1.0f;
                float  ph = hn.y * 6.2832f + g_ShaftCloud.w * (0.2f + 0.4f * hn.x);
                float3 n  = float3(sqrt(saturate(1.0f - nz * nz)) * float2(cos(ph), sin(ph)), nz);
                float  gl = pow(saturate(dot(n, normalize(g_SunToLight.xyz + normalize(-Mv)))), 24.0f);
                // Ronin @feature 23/10/2026 DX9: a flash is a white-hot core, a tight gold halo and a short four-point star; between flashes
                // only a third of the motes keep a dim speck.
                float  d2    = dot(s, s);
                float  on    = (hs.x < 0.14f) ? 1.0f : 0.0f;
                float  speck = (frac(hn.x * 13.7f + hn.y * 7.1f) < 0.35f) ? 0.25f : 0.0f;
                float  ray   = exp(-abs(s.x) / 1.8f) * exp(-s.y * s.y / 0.3f) + exp(-abs(s.y) / 1.8f) * exp(-s.x * s.x / 0.3f);
                bestWhite = max(bestWhite, on * exp(-d2 * (1.0f / 1.44f)) * (speck + 2.0f * gl));
                bestWarm  = max(bestWarm, on * gl * (0.6f * exp(-d2 * 0.25f) + 0.8f * ray));
            }
            // Ronin @feature 23/10/2026 DX9: lit where the sun reaches the crossing - past the casters, through a cloud gap.
            float4 clip  = mul(float4(Xv, 1.0f), g_SunFar);
            float2 suv   = clip.xy * float2(0.5f, -0.5f) + 0.5f + g_SunMap.y;
            float  inMap = (suv.x > 0.0f && suv.x < 1.0f && suv.y > 0.0f && suv.y < 1.0f) ? 1.0f : 0.0f;
            float  sun   = lerp(1.0f, tex2Dproj(g_SunDepthFar, float4(suv, clip.z - g_SunMapFar.x, 1.0f)).r, inMap);
            float2 foot  = Xw.xy + g_ShaftTravel.xy * ((Xw.z - ground) / max(-g_ShaftTravel.z, 0.05f));
            float  cloud = dot(tex2Dlod(g_CloudMap, float4(foot * g_ShaftCloud.x + g_ShaftCloud.yz, 0.0f, g_ShaftGap.z)).rgb,
                               float3(0.299f, 0.587f, 0.114f));
            float  gap   = smoothstep(g_ShaftGap.x, g_ShaftGap.x + 0.027f, cloud);
            float  lit   = ahead * sun * lerp(1.0f, gap, g_ShaftGap.y);
            dustWhite += lit * bestWhite;
            dustWarm  += lit * bestWarm;
        }
        o.color.rgb = g_ShaftSun.rgb * (saturate(dustWhite * g_ShaftGap.w) + float3(1.0f, 0.85f, 0.55f) * saturate(dustWarm * g_ShaftGap.w));
        return o;
    }
    if (view == 3)
    {
        o.color = float4(0.10f, 0.03f, 0.0f, 0.0f);    // the rectangle each light costs
        return o;
    }
    // Ronin @diagnostic 11/10/2026 DX9: lamp shadow maps - one lamp's map as it was drawn: bright near the lamp, dark far from it,
    // blue where nothing stands in its light. uv = its tile in the atlas; g_LightPos.xy = the map's near and far.
    if (view == 4)
    {
        float2 e     = tex2Dlod(g_LampMap, float4(uv, 0.0f, 0.0f)).rg;
        float  z     = (e.x * 65280.0f + e.y * 255.0f) / 65535.0f;     // TaaMoverLight_ps: the high byte, then the low
        float  share = g_LightPos.x / max(g_LightPos.y - z * (g_LightPos.y - g_LightPos.x), 0.0001f);    // its distance, in the map's reach
        float  grey  = 1.0f - 0.8f * saturate(share);
        o.color = (z > 0.99998f) ? float4(0.03f, 0.06f, 0.14f, 0.0f) : float4(grey, grey, grey, 0.0f);
        return o;
    }
    // Ronin @diagnostic 12/10/2026 DX9: the sky cube as a panorama: all round across the quad, straight up at its top edge.
    if (view == 5)
    {
        float  turn = uv.x * 6.2832f;
        float  rise = (0.5f - uv.y) * 3.1416f;
        o.color = float4(texCUBElod(g_SkyCube, float4(cos(rise) * cos(turn), cos(rise) * sin(turn), sin(rise), 0.0f)).rgb, 0.0f);
        return o;
    }
    // Ronin @feature 15/10/2026 DX9: THE GROUND AS A LIGHT, one quad: dest x (1 + k). k = the surface's share of (what the ground
    // under it sends up - the flat ambient) x (1 - n.z) / 2.
    if (view == 7)
    {
        clip(0.99999f - tex2Dlod(g_SceneDepth, float4(uv, 0.0f, 0.0f)).r);
        float3 P  = viewPosition(uv);
        float3 Pr = viewPosition(uv + float2(g_TexelSize.x, 0.0f));
        float3 Pl = viewPosition(uv - float2(g_TexelSize.x, 0.0f));
        float3 Pd = viewPosition(uv + float2(0.0f, g_TexelSize.y));
        float3 Pu = viewPosition(uv - float2(0.0f, g_TexelSize.y));
        float3 dx = (abs(Pr.z - P.z) < abs(P.z - Pl.z)) ? (Pr - P) : (P - Pl);
        float3 dy = (abs(Pd.z - P.z) < abs(P.z - Pu.z)) ? (Pd - P) : (P - Pu);
        float3 N  = normalize(cross(dy, dx));
        if (dot(N, -P) < 0.0f)
            N = -N;
        float  down = 0.5f - 0.5f * dot(N, g_WorldZ.xyz);                     // 0 facing up, 0.5 a wall, 1 facing down
        float2 wxy  = g_LightPos.xy + float2(dot(P, g_WorldX.xyz), dot(P, g_WorldY.xyz));     // c5.xy: the camera in the world
        float3 G    = tex2Dlod(g_LampMap, float4((wxy - g_LightDiff.xy) * g_LightDiff.zw, 0.0f, 0.0f)).rgb * 2.0f;
        float4 shadeM = tex2Dlod(g_SunShade, float4(uv, 0.0f, 0.0f));
        float3 shadeG = tex2Dlod(g_GroundShade, float4(uv, 0.0f, 0.0f)).rgb;
        float3 shade  = float3(1.0f, 1.0f, 1.0f);   // no record: as if in the sun
        if (g_LightPos.w > 0.0f && max(shadeG.r, max(shadeG.g, shadeG.b)) > 0.05f)
            shade = shadeG;
        if (g_LightPos.z > 0.0f && shadeM.a > 0.5f)
            shade = shadeM.rgb;
        float3 k = g_SceneLight.rgb * (G - g_LightAmb.rgb) * down * g_LightAmb.w / clamp(shade, 0.05f, 1.0f);
        // Ronin @bugfix 02/11/2026 DX9: one pass (c0.w 2), blended dest x src x 2: the add and the darken at once.
        o.color = float4(max(0.5f + 0.5f * k, 0.0f), 0.0f);
        if (g_Camera.w == 0.0f)
            o.color = float4(saturate(0.5f + k * 2.0f), 0.0f);     // `lights view 6`: grey = nothing, lighter = added, darker = taken
        return o;
    }
    // Ronin @feature 12/10/2026 DX9: HDR stage 1b - THE SUN LIFT, one quad over the frame, blended dest x this + dest: the gain
    // times the sun's share of the pixel (1 - the shade's floor over its shade). g_LightPos = floor, g_LightDiff = gain; w = a record.
    if (view == 6)
    {
        float4 shadeM = tex2Dlod(g_SunShade, float4(uv, 0.0f, 0.0f));
        float3 shadeG = tex2Dlod(g_GroundShade, float4(uv, 0.0f, 0.0f)).rgb;
        float3 shade  = g_LightPos.rgb;         // no record of this pixel: no sun known, nothing added
        if (g_LightDiff.w > 0.0f && max(shadeG.r, max(shadeG.g, shadeG.b)) > 0.05f)
            shade = shadeG;
        if (g_LightPos.w > 0.0f && shadeM.a > 0.5f)
            shade = shadeM.rgb;
        float4 shadeT = tex2Dlod(g_LampMap, float4(uv, 0.0f, 0.0f));   // here: the trees' own record (Trees_ps COLOR1), 0.45..1
        if (g_Camera.w > 0.0f && shadeT.a > 0.5f)
            shade = lerp(g_LightPos.rgb, float3(1.0f, 1.0f, 1.0f), saturate((shadeT.r - 0.45f) / 0.55f));    // a canopy is in front
        o.color = float4(g_LightDiff.rgb * saturate(1.0f - g_LightPos.rgb / max(shade, g_LightPos.rgb)), 0.0f);
        return o;
    }

    clip(0.99999f - tex2Dlod(g_SceneDepth, float4(uv, 0.0f, 0.0f)).r);     // nothing drew here (sky, off-map)

    float3 P     = viewPosition(uv);
    float3 toL   = g_LightPos.xyz - P;
    float  dist  = length(toL);
    float  reach = ease(saturate(1.0f - (dist - g_LightDiff.w) * g_LightPos.w));
    reach *= coneTerm(dot(-toL / max(dist, 0.001f), g_LightAxis.xyz), g_LightAxis, g_LightAmb.w);
    float3 toP    = g_PrevPos.xyz - P;
    float  distP  = length(toP);
    float  reachP = ease(saturate(1.0f - (distP - g_PrevDiff.w) * g_PrevPos.w));
    reachP *= coneTerm(dot(-toP / max(distP, 0.001f), g_PrevAxis.xyz), g_PrevAxis, g_PrevAmb.w);

    float3 result  = float3(0.0f, 0.0f, 0.0f);
    float3 resultP = float3(0.0f, 0.0f, 0.0f);     // what the light added as it was last frame
    // Ronin @feature 09/10/2026 DX9: facing from the neighbouring depth texels, the smaller step on each axis so an edge does not bend it.
    float3 Pr = viewPosition(uv + float2(g_TexelSize.x, 0.0f));
    float3 Pl = viewPosition(uv - float2(g_TexelSize.x, 0.0f));
    float3 Pd = viewPosition(uv + float2(0.0f, g_TexelSize.y));
    float3 Pu = viewPosition(uv - float2(0.0f, g_TexelSize.y));
    float3 dx = (abs(Pr.z - P.z) < abs(P.z - Pl.z)) ? (Pr - P) : (P - Pl);
    float3 dy = (abs(Pd.z - P.z) < abs(P.z - Pu.z)) ? (Pd - P) : (P - Pu);
    float3 N  = normalize(cross(dy, dx));
    if (dot(N, -P) < 0.0f)
        N = -N;                                 // face the camera
    // Ronin @feature 11/10/2026 DX9: the lamp's shadow map, read out here: fxc refuses the compared read inside a branch (it needs
    // the screen's gradients). As it is now and as it was last frame: where a shadow moved, the change mask below says so.
    LampSide side = lampSide(P, N, g_LightPos.xyz);
    LampSide none = side;
    none.use = 0.0f;
    float lampLit  = lampShadow(g_LampDepth, g_LampClip, g_LampTile, g_LampBias.x, P, N, g_LightPos.xyz, side);
    float lampLitP = lampShadow(g_LampDepthWas, g_LampClipWas, g_LampTileWas, g_LampWas.x, P, N, g_PrevPos.xyz, none);
    lampLitP = lerp(lampLitP, lampLit, side.use);      // the level maps keep no last frame: there the shadow counts as unmoved
    [branch]
    if (reach + reachP > 0.0f)
    {
        float  ndl   = saturate(dot(N, toL / max(dist, 0.001f)));
        float3 light = (g_LightDiff.rgb * ndl + g_LightAmb.rgb) * (reach * lampLit);
        float  ndlP   = saturate(dot(N, toP / max(distP, 0.001f)));
        float3 lightP = (g_PrevDiff.rgb * ndlP + g_PrevAmb.rgb) * (reachP * lampLitP);

        if (view == 1)
            result = light * 0.5f;              // the light alone, on a grey world
        else if (view == 2)
            result = N * 0.5f + 0.5f;           // the facing it was given
        else
        {
            // Ronin @feature 09/10/2026 DX9: the frame is surface colour x the map's light: undo that light, light the surface, and stop at
            // the surface's own colour x overbright (the old vertex path stopped at 1).
            float3 frame   = tex2Dlod(g_Frame, float4(uv, 0.0f, 0.0f)).rgb;
            // Ronin @feature 10/10/2026 DX9: the frame is darker where the sun's shadow fell; the mesh's own draw recorded by
            // how much, so the lamp lights the surface as it is, not as the shadow left it.
            float4 shadeM  = tex2Dlod(g_SunShade, float4(uv, 0.0f, 0.0f));
            float3 shadeG  = tex2Dlod(g_GroundShade, float4(uv, 0.0f, 0.0f)).rgb;
            float3 white   = float3(1.0f, 1.0f, 1.0f);
            float3 shade   = (shadeM.a > 0.5f) ? shadeM.rgb : ((max(shadeG.r, max(shadeG.g, shadeG.b)) > 0.05f) ? shadeG : white);    // a mesh's own first, else the ground's
            shade = lerp(white, clamp(shade, 0.05f, 1.0f), g_TexelSize.z);
            // Ronin @feature 16/10/2026 DX9: by day a local light keeps c15.w where the sun shines, c33.x in a shadow (1 at night).
            // Sunlit: the shade over the map's floor (c4.w = its luminance). lightSun: as it would be in the sun, for the cap below.
            float sunLit = saturate((dot(shade, float3(0.299f, 0.587f, 0.114f)) - g_SceneLight.w) / max(1.0f - g_SceneLight.w, 0.05f));
            float3 lightSun  = light  * g_Jitter.w;
            float3 lightSunP = lightP * g_Jitter.w;
            float  dayKeep   = lerp(g_DayShade.x, g_Jitter.w, sunLit);
            light  *= dayKeep;
            lightP *= dayKeep;
            // Ronin @bugfix 25/10/2026 DX9: the frame's light by this pixel's own facing, not open ground's - a wall turned from the moon
            // got ambient only, and one lamp lit two walls of a corner differently.
            float3 frameLight = g_SceneLight.rgb;
            if (g_FrameAmb.w > 0.5f)
            {
                float3 lit = g_FrameAmb.rgb + g_FrameLight[1].rgb * saturate(dot(N, g_FrameLight[0].xyz))
                                            + g_FrameLight[3].rgb * saturate(dot(N, g_FrameLight[2].xyz))
                                            + g_FrameLight[5].rgb * saturate(dot(N, g_FrameLight[4].xyz));
                frameLight = 1.0f / clamp(lit, 0.1f, 1.0f);
            }
            float3 surface = saturate(frame * frameLight / shade);
            float3 ceiling = max(frame, surface * g_Camera.w);
            // Ronin @feature 10/10/2026 DX9: and the lamp gives back what the sun's shadow took, in step with its own strength here: the
            // shadow was still to be seen at the pool's rim, where the lamp is weak (`lights sunerase`).
            float3 lost    = frame / shade - frame;
            float  lamp    = saturate(max(light.r, max(light.g, light.b)) * g_TexelSize.w);
            float  lampP   = saturate(max(lightP.r, max(lightP.g, lightP.b)) * g_TexelSize.w);
            // Ronin @bugfix 16/10/2026 DX9: a shadowed pixel never brighter than it would be in the sun under the same light - the
            // lit shadow shapes round muzzle flashes and police cars. A sunlit pixel and the night are not changed by it.
            result  = min(min(frame + lost * lamp + surface * light, ceiling), frame + lost + surface * lightSun) - frame;
            resultP = min(min(frame + lost * lampP + surface * lightP, ceiling), frame + lost + surface * lightSunP) - frame;
        }
    }

    // Ronin @feature 09/10/2026 DX9: the beam in the air - also over ground the light itself does not reach. Not in the facing view.
    [branch]
    if (g_Shaft.x + g_Shaft.y > 0.0f && view != 2)
    {
        float3 air  = (g_LightDiff.rgb + g_LightAmb.rgb) * (shaft(P, g_LightPos.xyz, g_Corona.x, g_LightAxis, g_LightAmb.w) * g_Shaft.x);
        float3 airP = (g_PrevDiff.rgb + g_PrevAmb.rgb) * (shaft(P, g_PrevPos.xyz, g_Corona.z, g_PrevAxis, g_PrevAmb.w) * g_Shaft.y);
        result  += (view == 1) ? air * 0.5f : air;
        resultP += airP;
    }

    // Ronin @feature 09/10/2026 DX9: the lamp's own glow, in the light's colour.
    [branch]
    if (g_Corona.y + g_Corona.w > 0.0f && view != 2)
    {
        result  += (g_LightDiff.rgb + g_LightAmb.rgb) * (corona(P, g_LightPos.xyz, g_Corona.x) * g_Corona.y);
        resultP += (g_PrevDiff.rgb + g_PrevAmb.rgb) * (corona(P, g_PrevPos.xyz, g_Corona.z) * g_Corona.w);
    }

    // Ronin @bugfix 09/10/2026 DX9: what the light added now against what it added last frame: 2/255 of change marks the pixel in
    // full for TAA. A steady light marks nothing, so its pool and its beam keep their whole history.
    if (view == 0)
    {
        float3 change = abs(result - resultP);
        o.mask = saturate(max(change.r, max(change.g, change.b)) * 128.0f);
        // Ronin @feature 15/10/2026 DX9: alpha = how much lamp light this pixel took, summed over the lights: the night look keeps a
        // lamp-lit pixel's own colour by it, not by how bright the pixel is.
        o.mask.a = saturate(dot(result, float3(0.299f, 0.587f, 0.114f)) * 6.0f);
    }
    o.color = float4(result, 0.0f);
    return o;
}
