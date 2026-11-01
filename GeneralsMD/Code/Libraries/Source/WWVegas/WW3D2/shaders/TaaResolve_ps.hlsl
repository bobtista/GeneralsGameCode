// Ronin @feature 20/09/2026 DX9: TAA resolve — reproject the history, clamp it, blend it.
//
// STEP 2b-ii: full resolve. The history is reprojected with the previous view-projection so a moving camera still
// lines up, then clamped to the current pixel's neighbourhood so movers do not trail, then blended with an EMA.
//
// THE REPROJECTION IS ANCHORED ON THIS PIXEL AND ADDS ONLY THE DELTA. Using the absolute reprojected UV jitters past
// half a pixel and bleeds — the shadow work paid for that lesson once already, docs/ShadowMap_Temporal_Work.md §8.7.
// Computing the current UV through the SAME matrix path means the float error is common to both and cancels in the
// subtraction, so a still camera reprojects to exactly this pixel instead of almost this pixel.
//
// Compile with: fxc /T ps_3_0 /Fo TaaResolve.pso TaaResolve_ps.hlsl   (pairs with ScreenQuad.vso)

sampler2D g_Scene   : register(s0);      // this frame, as rendered (W3DShaderManager's render-to-texture target)
sampler2D g_History : register(s1);      // last frame's resolved output
sampler2D g_Depth   : register(s2);      // INTZ scene depth, this frame
sampler2D g_PrevDepth: register(s4);     // last frame's LINEAR depth, packed RGBA8 by TaaDepthStore
sampler2D g_VelPrev  : register(s6);     // LAST frame's mesh velocity; its alpha = a mover covered this pixel
sampler2D g_Opaque   : register(s7);     // this frame BEFORE particles (R3DTaa::noteOpaqueDone) - auto-reactive
sampler2D g_Velocity : register(s5);     // mesh velocity, 12-bit packed by TaaVelMesh_ps; alpha = kind (R3DTaa.cpp)
// Ronin @feature 19/10/2026 DX9: the change mask. g = where a local light changed this pixel since last frame (LocalLight_ps
// COLOR1), r = where a screen-space reflection did (ReflectiveRigid_ps), b = and a unit moved there, a = lamp light.
sampler2D g_LightMask: register(s8);
// Ronin @bugfix 09/10/2026 DX9: the history's LOW BITS (COLOR1 of the pass that wrote it), bound when c6.w = 1. The history is 8-bit;
// at weight 0.9 it never shed an error under 5/255 on a still pixel, so every faint ghost stayed until the camera moved.
sampler2D g_HistoryLo: register(s9);
// Ronin @bugfix 09/10/2026 DX9: the selection flash: rgb = its colour at this pixel, a = 1 where a mesh flashes (TaaVelMesh_ps COLOR1).
// Added in the screen copy only - c8.w is its strength there and 0 in the history pass.
sampler2D g_Flash    : register(s10);
float4   g_FlashPrm  : register(c8);     // rgb = 1 / the map's light (frame -> surface colour), w = strength

float4   g_TaaParams : register(c0);     // x = history weight (0 = ignore history), yz = 1/texture size, w = reproject on
float4x4 g_Reproject : register(c1);     // c1..c4: current clip -> previous clip. Transposed, like every matrix here.
float4   g_Viewport  : register(c5);     // xy = 3D viewport min, zw = its size, both 0..1 of the texture
float4   g_TaaDebug  : register(c6);     // x = debug view, yz = clip planes (the depth views must linearise)
float4   g_TaaSharpen: register(c7);     // x = CAS (screen copy only), y = this frame's velocity is bound, z = `taa reflmark`, w = clamp
float4   g_TaaExtra  : register(c10);    // x = last frame's velocity bound, y = `taa disocc` mode, z = `taa reactive`, w = light mask bound
float4   g_TaaVel    : register(c11);    // x = 1: an effect that just left resets the pixel, y = 1 / (`autofull` - `autoreact`), z = `taa disoccv` px, w = `taa autoreact` (0 = off or no snapshot)
// Ronin @bugfix 27/09/2026 DX9: the moving-shadow mask (R3DTaa renderMoverMask) - sun depth of every mesh that moved this
// frame, 16-bit packed: RG where it is now, BA where it was. c12..c15 take a screen clip position to the sun's.
sampler2D g_MoverMap : register(s3);
float4x4 g_ToSun     : register(c12);
float4   g_MoverPrm  : register(c16);    // x = drawn this frame (2 = and `taa maskcap`), y = half texel, z = dilation (uv),
                                         // w = depth bias
// Ronin @bugfix 01/11/2026 DX9: the fog of war's change mask (R3DTaa updateFogMask): a = 1 at shroud texels that changed this
// frame or last. c25..c28 take a screen clip position to the fog texture's uv; c29.x = 1 when it is bound.
sampler2D g_FogChange : register(s12);
float4x4  g_ToFog     : register(c25);
float4    g_FogPrm    : register(c29);

// Ronin @feature 12/10/2026 DX9: HDR stage 1b - the TONE CURVE. A float scene is bent as it is read: as it is up to the knee
// (2 - white), then a parabola that meets it there with the same slope and reaches 1 at `white`. White 1 = no bend.
float4   g_Tone      : register(c9);     // x = `hdr white` (1..2), y = 1: stage 0 and g_Opaque hold the float scene
float3 toneCurve(float3 c)
{
    float  knee = 2.0f - g_Tone.x;
    // Ronin @bugfix 23/10/2026 DX9: the brightest channel bent and the others scaled with it, so a colour keeps its hue and saturation;
    // bent one by one, a sunlit orange went yellow (hue 28 -> 45 deg) and paler.
    float  m    = max(c.r, max(c.g, c.b));
    float  t    = saturate((m - knee) / max(g_Tone.x - knee, 0.0001f));
    float  bent = knee + (1.0f - knee) * (1.0f - (1.0f - t) * (1.0f - t));
    return (g_Tone.y > 0.5f && m > knee) ? c * (bent / m) : c;
}

// Ronin @feature 15/10/2026 DX9: THE NIGHT LOOK. Night vision has no colour in the dim: a pixel keeps its own colour as far as
// it is bright (lo..hi, never under the floor); the rest is its luminance in one tint, a share of the map light's cast.
float4   g_Night     : register(c17);    // rgb = the dim's tint (luminance 1), w = the look's strength (0 = off): the map light's colour
float4   g_NightKeep : register(c18);    // x = lo, y = hi (the luminance where a pixel starts / ends keeping its colour), z = floor,
                                         // Ronin @feature 16/10/2026 DX9: w = 1: the light mask (s8) is bound - its alpha = the lamp light a pixel took

// Ronin @feature 15/10/2026 DX9: THE DARK LIFT - at night the eye adapts to the dark: luminance l -> l^g, the colour kept. The dark
// comes up most, the bright (lamps) hardly moves. c19.x = g (1 = none), y = 1: on.
float4   g_NightLift : register(c19);
float3 darkLift(float3 c)
{
    float l = dot(c, float3(0.299f, 0.587f, 0.114f));
    return (l > 0.0005f) ? saturate(c * pow(max(l, 0.0005f), g_NightLift.x - 1.0f)) : c;
}
float3 nightLook(float3 c, float2 uv)
{
    float l    = dot(c, float3(0.299f, 0.587f, 0.114f));
    float keep = max(g_NightKeep.z, smoothstep(g_NightKeep.x, g_NightKeep.y, l));
    if (g_NightKeep.w > 0.5f)
        keep = max(keep, tex2Dlod(g_LightMask, float4(uv, 0.0f, 0.0f)).a);     // lit by a lamp: its own colour
    keep = lerp(1.0f, keep, g_Night.w);
    return saturate(lerp(l * g_Night.rgb, c, keep));
}

// Ronin @feature 21/10/2026 DX9: THE GRADE (`grade`, by day; screen copy only): a warm tint with each pixel's luminance kept, then
// saturation, then the mid-tones' power. c20 = (on, saturation, power), c21 = the tint.
float4   g_Grade     : register(c20);
float4   g_GradeTint : register(c21);
float3 grade(float3 c)
{
    const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);
    float  l0 = dot(c, LUMA);
    float3 t  = c * g_GradeTint.rgb;
    float  l1 = dot(t, LUMA);
    c = (l1 > 0.000001f) ? t * (l0 / l1) : c;
    float  l  = dot(c, LUMA);
    c = l + (c - l) * g_Grade.y;
    l = dot(c, LUMA);
    return saturate((l > 0.0005f) ? c * pow(max(l, 0.0005f), g_Grade.z - 1.0f) : c);
}

// Ronin @feature 22/10/2026 DX9: VIBRANCE and LOCAL CONTRAST (screen copy only): the duller a colour the more it is raised, then
// each pixel against its surroundings - s11, the frame's luminance blurred at quarter size. c22 = (on, vibrance, local contrast, night).
float4    g_Vivid    : register(c22);
sampler2D g_Surround : register(s11);
float3 vivid(float3 c, float2 uv)
{
    const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    float s  = (mx > 0.000001f) ? (mx - mn) / mx : 0.0f;
    float l  = dot(c, LUMA);
    const bool night = (g_Vivid.w > 0.5f);
    // Ronin @feature 23/10/2026 DX9: at night only where the night look lets a pixel keep its colour (bright, or lamp-lit): the moonlit
    // rest stays in its one tint.
    float keep = 1.0f;
    if (night)
    {
        keep = smoothstep(g_NightKeep.x, g_NightKeep.y, l);
        if (g_NightKeep.w > 0.5f)
            keep = max(keep, tex2Dlod(g_LightMask, float4(uv, 0.0f, 0.0f)).a);
    }
    c = l + (c - l) * (1.0f + g_Vivid.y * keep * (1.0f - s));
    if (g_Vivid.z > 0.0f && l > 0.0001f)
    {
        float lb = tex2Dlod(g_Surround, float4((uv - g_Viewport.xy) / max(g_Viewport.zw, 0.0001f), 0.0f, 0.0f)).r;
        if (night)
        {
            // Ronin @feature 23/10/2026 DX9: at night by ratio, the same at any light level; darkening held to 15% (30% took the
            // darkest tenth from 10 to 7 of 255).
            c *= clamp(pow(max(l / max(lb, 0.0001f), 0.0001f), g_Vivid.z), 0.85f, 1.5f);
            return saturate(c);
        }
        // Ronin @bugfix 23/10/2026 DX9: darkening fades out below 0.3 - at full strength it took the darkest tenth from 16 to 4 of 255.
        float d  = l - lb;
        float k  = g_Vivid.z * ((d < 0.0f) ? smoothstep(0.0f, 0.3f, l) : 1.0f);
        c *= max(l + k * d, 0.0f) / l;
    }
    return saturate(c);
}

// Ronin @feature 21/09/2026 DX9: Catmull-Rom history fetch, 9 taps.
//
// WHY NOT BILINEAR: the history is re-fetched at a subpixel position every frame and fed straight back in. Bilinear
// softens a little on each fetch and that loss COMPOUNDS through the feedback loop — which is why a lower history
// weight looked sharper (less feedback) while shimmering more (less accumulation). Catmull-Rom resamples without the
// softening, so a high weight can be used for stability without paying for it in blur.
// Ronin @feature 22/09/2026 DX9: inverse of TaaDepthStore's pack.
// Ronin @feature 26/09/2026 DX9: 24 bits in RGB now - alpha carries last frame's auto-reactive flag.
float unpackDepth(float4 enc)
{
    return dot(enc.rgb, float3(1.0f, 1.0f / 255.0f, 1.0f / 65025.0f));
}
// Ronin @bugfix 27/09/2026 DX9: inverse of TaaMoverLight_ps's 16-bit pack.
float unpack16(float2 e)
{
    float2 b = floor(e * 255.0f + 0.5f);
    return (b.x * 256.0f + b.y) / 65535.0f;
}

float3 sampleHistoryCatmullRom(float2 uv, float2 texSize, float2 invTexSize)
{
    float2 samplePos = uv * texSize;
    float2 texPos1   = floor(samplePos - 0.5f) + 0.5f;
    float2 f         = samplePos - texPos1;

    // Catmull-Rom weights, expanded so fxc keeps them in registers rather than a loop.
    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    // The middle pair is folded into ONE bilinear tap placed between them — 9 fetches instead of 16.
    float2 w12      = w1 + w2;
    float2 offset12 = w2 / max(w12, 0.0001f);

    float2 texPos0  = (texPos1 - 1.0f)  * invTexSize;
    float2 texPos3  = (texPos1 + 2.0f)  * invTexSize;
    float2 texPos12 = (texPos1 + offset12) * invTexSize;

    float3 result = 0.0f;
    result += tex2D(g_History, float2(texPos0.x,  texPos0.y )).rgb * w0.x  * w0.y;
    result += tex2D(g_History, float2(texPos12.x, texPos0.y )).rgb * w12.x * w0.y;
    result += tex2D(g_History, float2(texPos3.x,  texPos0.y )).rgb * w3.x  * w0.y;

    result += tex2D(g_History, float2(texPos0.x,  texPos12.y)).rgb * w0.x  * w12.y;
    result += tex2D(g_History, float2(texPos12.x, texPos12.y)).rgb * w12.x * w12.y;
    result += tex2D(g_History, float2(texPos3.x,  texPos12.y)).rgb * w3.x  * w12.y;

    result += tex2D(g_History, float2(texPos0.x,  texPos3.y )).rgb * w0.x  * w3.y;
    result += tex2D(g_History, float2(texPos12.x, texPos3.y )).rgb * w12.x * w3.y;
    result += tex2D(g_History, float2(texPos3.x,  texPos3.y )).rgb * w3.x  * w3.y;

    return result;
}

// Ronin @feature 21/09/2026 DX9: clip toward the box CENTRE along the ray, instead of clamping each channel on its
// own. Per-channel clamping can land on a colour that exists nowhere in the neighbourhood — it mixes the red of one
// corner with the blue of another — and that invented colour is itself a source of flicker. Standard clip_aabb.
float3 clipToAABB(float3 boxMin, float3 boxMax, float3 history)
{
    float3 centre = 0.5f * (boxMax + boxMin);
    float3 extent = 0.5f * (boxMax - boxMin) + 0.0001f;
    float3 ray    = history - centre;
    float3 unit   = ray / extent;
    float3 a      = abs(unit);
    float  maxA   = max(a.x, max(a.y, a.z));
    return (maxA > 1.0f) ? (centre + ray / maxA) : history;
}

// Ronin @bugfix 26/09/2026 DX9: TaaVelMesh_ps's 12-bit packing, inverted. px, screen Y down, current minus previous.
float2 decodeVel(float4 vm)
{
    float3 b   = floor(vm.rgb * 255.0f + 0.5f);
    float  loX = floor(b.z / 16.0f);
    float  loY = b.z - loX * 16.0f;
    float2 e   = float2(b.x * 16.0f + loX, b.y * 16.0f + loY);
    return (e / 4095.0f - 0.5f) * 128.0f;
}

// Ronin @bugfix 27/09/2026 DX9: did the mesh in this velocity sample move away from the pixel? Idle infantry (alpha 0.92, a
// soldier standing still, only breathing) need `taa disoccv` px - resetting every breath popped their edges. Any other mesh
// vacates at any distance: a flag near its pole moves as little as a breath (debug 15), and kept its old edge - the outline.
float vacates(float4 v, float2 camPx)
{
    if (v.a <= 0.5f)
        return 0.0f;
    float need = (v.a > 0.88f && v.a < 0.96f) ? g_TaaVel.z : 0.0f;
    return (length(decodeVel(v) - camPx) >= need) ? 1.0f : 0.0f;
}

// Ronin @feature 26/09/2026 DX9: AMD FidelityFX CAS (contrast adaptive sharpening), the non-scaling form. Sharpens less
// where the 3x3 is already near its limits, so edges do not ring and flat areas do not grain. Clamped to the 3D viewport so
// the border does not read the command bar's texels.
float3 casSharpen(float2 uv, float sharpness)
{
    float2 px = g_TaaParams.yz;
    float2 lo = g_Viewport.xy + 0.5f * px;
    float2 hi = g_Viewport.xy + g_Viewport.zw - 0.5f * px;
    #define CAS_TAP(OX, OY) toneCurve(tex2Dlod(g_Scene, float4(clamp(uv + float2(OX, OY) * px, lo, hi), 0.0f, 0.0f)).rgb)
    float3 a = CAS_TAP(-1.0f, -1.0f);  float3 b = CAS_TAP(0.0f, -1.0f);  float3 c = CAS_TAP(1.0f, -1.0f);
    float3 d = CAS_TAP(-1.0f,  0.0f);  float3 e = CAS_TAP(0.0f,  0.0f);  float3 f = CAS_TAP(1.0f,  0.0f);
    float3 g = CAS_TAP(-1.0f,  1.0f);  float3 h = CAS_TAP(0.0f,  1.0f);  float3 i = CAS_TAP(1.0f,  1.0f);
    #undef CAS_TAP
    float3 mn = min(min(min(d, e), min(f, b)), h);
    mn += min(mn, min(min(a, c), min(g, i)));      // soft min: cross + all nine, 2x scale
    float3 mx = max(max(max(d, e), max(f, b)), h);
    mx += max(mx, max(max(a, c), max(g, i)));
    float3 amp = sqrt(saturate(min(mn, 2.0f - mx) / max(mx, 0.0001f)));
    float3 w   = amp * (-1.0f / lerp(8.0f, 5.0f, saturate(sharpness)));
    return saturate((b * w + d * w + f * w + h * w + e) / (1.0f + 4.0f * w));
}

float luminance(float3 c)
{
    return dot(c, float3(0.2127f, 0.7152f, 0.0722f));
}

// Ronin @feature 22/10/2026 DX9: LOCAL CONTRAST's surroundings (R3DTaa buildSurround; c0.x below 0): -1 = the source's luminance as shown,
// 4x4 texels each, from the 3D view in it (c24); -2 / -3 = a Gaussian across / down, sigma 5 texels x c0.w. c0.yz = a source texel.
float4 g_SurroundSrc : register(c24);
float4 surround(float2 uv)
{
    if (g_TaaParams.x > -1.5f)
    {
        float2 px = g_TaaParams.yz;
        float2 at = g_SurroundSrc.xy + uv * g_SurroundSrc.zw;
        float2 lo = g_SurroundSrc.xy + 0.5f * px;
        float2 hi = g_SurroundSrc.xy + g_SurroundSrc.zw - 0.5f * px;
        float  l  = luminance(toneCurve(tex2Dlod(g_Scene, float4(clamp(at + float2(-1.0f, -1.0f) * px, lo, hi), 0.0f, 0.0f)).rgb)) +
                    luminance(toneCurve(tex2Dlod(g_Scene, float4(clamp(at + float2( 1.0f, -1.0f) * px, lo, hi), 0.0f, 0.0f)).rgb)) +
                    luminance(toneCurve(tex2Dlod(g_Scene, float4(clamp(at + float2(-1.0f,  1.0f) * px, lo, hi), 0.0f, 0.0f)).rgb)) +
                    luminance(toneCurve(tex2Dlod(g_Scene, float4(clamp(at + float2( 1.0f,  1.0f) * px, lo, hi), 0.0f, 0.0f)).rgb));
        l *= 0.25f;
        if (g_NightLift.y > 0.5f && g_NightLift.x < 0.999f)
            l = pow(max(l, 0.0005f), g_NightLift.x);		// Ronin @feature 23/10/2026 DX9: at night as shown - after the dark lift
        return float4(l, l, l, 1.0f);
    }
    static const float OFS[8] = { 1.4850f, 3.4651f, 5.4452f, 7.4256f, 9.4061f, 11.3870f, 13.3682f, 15.3498f };
    static const float WTS[8] = { 0.152007f, 0.124701f, 0.087314f, 0.052179f, 0.026613f, 0.011585f, 0.004304f, 0.001364f };
    float2 dir = ((g_TaaParams.x > -2.5f) ? float2(g_TaaParams.y, 0.0f) : float2(0.0f, g_TaaParams.z)) * g_TaaParams.w;
    float  sum = 0.079864f * tex2Dlod(g_Scene, float4(uv, 0.0f, 0.0f)).r;
    [unroll]
    for (int k = 0; k < 8; ++k)
        sum += WTS[k] * (tex2Dlod(g_Scene, float4(uv + dir * OFS[k], 0.0f, 0.0f)).r + tex2Dlod(g_Scene, float4(uv - dir * OFS[k], 0.0f, 0.0f)).r);
    return float4(sum, sum, sum, 1.0f);
}

float4 resolve(float2 uv, out float3 lowBits)
{
    lowBits = float3(0.5f, 0.5f, 0.5f);		// no remainder, on every early return
    float3 scene = toneCurve(tex2D(g_Scene, uv).rgb);

    // No usable history: first frame after a toggle, a resize, a device reset, or a target that could not be made.
    // Degrade to a straight copy rather than blend into a buffer nobody wrote — the mistake that cost the terrain its
    // shadows under MSAA (Windowednew §31b).
    // Alpha 0, not 1: alpha is the motion memory now, and marking a fresh frame as "moving" would reject the history
    // it is about to start building.
    if (g_TaaParams.x <= 0.0f)
    {
        // DEBUG 3 belongs HERE, in the copy-to-screen pass. Returning it from the resolve pass writes the debug value
        // into the HISTORY, and the next frame computes `reject` against that corrupted history — the measurement
        // destroys the thing it measures. Alpha already carries the motion memory the real path wrote, so reading it
        // back here shows the signal without disturbing it.
        if (g_TaaDebug.x > 2.5f && g_TaaDebug.x < 3.5f)
        {
            float a = tex2D(g_Scene, uv).a;		// stage 0 IS the history in this pass
            return float4(a, a, a, 0.0f);
        }
        // Ronin @feature 26/09/2026 DX9: CAS on the way to the screen (`taa sharpen`). This pass does not feed the history,
        // so the sharpening cannot compound frame over frame. c7.x is 0 in pass 1.
        float3 shown = (g_TaaSharpen.x > 0.0f) ? casSharpen(uv, g_TaaSharpen.x) : scene;
        // Ronin @feature 15/10/2026 DX9: the night look - one saturated map light on two paints read as two scenes. Before the
        // flash, which keeps its colour.
        if (g_NightLift.y > 0.5f && g_NightLift.x < 0.999f)
            shown = darkLift(shown);
        if (g_Night.w > 0.0f)
            shown = nightLook(shown, uv);
        if (g_Vivid.x > 0.0f)
            shown = vivid(shown, uv);		// Ronin @feature 22/10/2026 DX9: by day; then the grade, if a modder turns it on
        if (g_Grade.x > 0.0f)
            shown = grade(shown);		// Ronin @feature 21/10/2026 DX9: by day; before the flash, which keeps its colour
        // Ronin @bugfix 09/10/2026 DX9: SELECTION FLASH, after the resolve. Lit like a local light: the surface colour (frame over the
        // map's light) x the flash, never past the surface's own colour. The history never sees it, so nothing lags.
        if (g_FlashPrm.w > 0.0f)
        {
            float4 fl = tex2Dlod(g_Flash, float4(uv, 0.0f, 0.0f));
            if (fl.a > 0.5f)
            {
                float3 surface = saturate(shown * g_FlashPrm.rgb);
                shown = min(shown + surface * fl.rgb * g_FlashPrm.w, max(shown, surface));
            }
        }
        return float4(shown, 0.0f);
    }



    // ---- reprojection ------------------------------------------------------------------------------------------
    float2 histUV   = uv;
    float2 camHistUV = uv;	// the CAMERA-reprojected position, kept before any velocity replaces histUV
    float  velTaken = 0.0f;
    float  speedPx  = 0.0f;		// how far this pixel moved since last frame, in pixels — drives the weight below
    float  prevW    = 1.0f;		// w of the reprojected position: <= 0 means it was BEHIND the eye last frame
    if (g_TaaParams.w > 0.0f)
    {
        // This pixel's position in the 3D view, as NDC. The viewport is a sub-rect of the texture (the command bar
        // takes the rest), so normalise through it before going to clip space.
        float2 s   = (uv - g_Viewport.xy) / g_Viewport.zw;
        float2 ndc = float2(s.x * 2.0f - 1.0f, 1.0f - s.y * 2.0f);

        // tex2DLOD, not tex2D. This sits inside dynamic flow control, where gradient-based sampling is UNDEFINED in
        // HLSL — and on an INTZ depth texture "undefined" came back as a flat 1.0, so every pixel reprojected as if it
        // were on the far plane and the whole frame smeared. SsaoRaw_ps reads the same buffer with tex2Dlod for the
        // same reason.
        float  d   = tex2Dlod(g_Depth, float4(uv, 0.0f, 0.0f)).r;
        float4 cur = float4(ndc, d, 1.0f);

        float4 prev = mul(cur, g_Reproject);
        prevW = prev.w;
        if (prev.w > 0.0001f)
        {
            float2 prevNDC = prev.xy / prev.w;

            // DELTA, not absolute — see the note at the top. Both sides come from the same ndc, so the error cancels.
            float2 deltaNDC = prevNDC - ndc;
            float2 deltaUV  = float2(deltaNDC.x * 0.5f, -deltaNDC.y * 0.5f) * g_Viewport.zw;
            histUV  = uv + deltaUV;
            camHistUV = histUV;
            speedPx = length(deltaUV / max(g_TaaParams.yz, 0.000001f));

            // Ronin @feature 24/09/2026 DX9: MOTION VECTORS, drawn from the meshes (§14). Written only where a moving mesh is
            // the visible surface. It is the full screen motion of that surface point - camera included - so it REPLACES
            // the camera reprojection above: the fetch is relocated, and the mover keeps its accumulation.
            if (g_TaaSharpen.y > 0.5f)
            {
                float4 vm = tex2Dlod(g_Velocity, float4(uv, 0.0f, 0.0f));
                // Ronin @bugfix 26/09/2026 DX9: DILATION. The velocity silhouette is the CLEAN projection and
                // the scene the JITTERED one, so up to half a pixel of a moving unit's edge had no velocity: reset, or fed
                // the old position's history unclamped - the ugly perimeter. Borrow a neighbour's velocity, but only from
                // the SAME surface (depth within 1%), so it never spreads onto the ground beside the unit.
                if (vm.a <= 0.5f)
                {
                    float zn0   = g_TaaDebug.y;
                    float zf0   = g_TaaDebug.z;
                    float zc    = (zn0 * zf0) / max(zf0 - d * (zf0 - zn0), 0.0001f);
                    float bestD = max(zc * 0.01f, 1.0f);
                    float2 offs[4] = { float2(g_TaaParams.y, 0.0f), float2(-g_TaaParams.y, 0.0f),
                                       float2(0.0f, g_TaaParams.z), float2(0.0f, -g_TaaParams.z) };
                    [unroll] for (int i = 0; i < 4; ++i)
                    {
                        float4 vn = tex2Dlod(g_Velocity, float4(uv + offs[i], 0.0f, 0.0f));
                        if (vn.a > 0.5f)
                        {
                            float rn = tex2Dlod(g_Depth, float4(uv + offs[i], 0.0f, 0.0f)).r;
                            float dz = abs((zn0 * zf0) / max(zf0 - rn * (zf0 - zn0), 0.0001f) - zc);
                            if (dz < bestD)
                            {
                                bestD = dz;
                                vm    = vn;
                            }
                        }
                    }
                }
                if (vm.a > 0.5f)
                {
                    float2 v  = decodeVel(vm);		// px, screen Y down, current minus previous
                    histUV    = uv - v * g_TaaParams.yz;			// the surface point was here last frame
                    velTaken  = 1.0f;
                }
            }

            // DEBUG 1: the delta itself, scaled so a one-pixel shift is clearly visible. Grey = no motion; a smooth
            // gradient that follows the terrain = correct parallax; a flat colour = depth is not reaching us; wild
            // banding = the matrix is wrong. This is the picture that says which of those it is.
            if (g_TaaDebug.x > 0.5f && g_TaaDebug.x < 1.5f)
                return float4(saturate(0.5f + deltaUV.x * 40.0f), saturate(0.5f + deltaUV.y * 40.0f), 0.5f, 1.0f);
        }
    }

    // DEBUG 2: depth as grey, LINEARISED. Raw D3D depth is f/(f-n)*(1-n/z), which sits above 0.99 for nearly the whole
    // scene — showing it directly is a white screen no matter how healthy the buffer is. Linearise to camera distance
    // and normalise by the far plane, and the terrain's shape appears.
    if (g_TaaDebug.x > 1.5f && g_TaaDebug.x < 2.5f)	// BOUNDED: unbounded, this ate 3, 4, 5 and 6 and returned alpha=1
    {
        float dd = tex2Dlod(g_Depth, float4(uv, 0.0f, 0.0f)).r;
        float n  = g_TaaDebug.y;
        float f  = g_TaaDebug.z;
        float linZ = (n * f) / max(f - dd * (f - n), 0.0001f);	// `linear` is a reserved HLSL interpolation modifier
        float g = saturate(linZ / f);
        return float4(g, g, g, 1.0f);
    }


    // Off screen OR behind the eye last frame = newly revealed, so there is no history to blend. The w test is the
    // half TerrainShadow_ps has that this was missing: a pixel can reproject to a valid-looking UV while actually
    // having been behind the camera, and that history is garbage. Alpha 0 — newly revealed is not a mover, and
    // marking it would reject the perfectly good history this pixel has NEXT frame.
    if (prevW <= 0.0f ||
        histUV.x < g_Viewport.x || histUV.y < g_Viewport.y ||
        histUV.x > g_Viewport.x + g_Viewport.z || histUV.y > g_Viewport.y + g_Viewport.w)
        return float4(scene, 0.0f);

    float2 texSize = float2(1.0f / g_TaaParams.y, 1.0f / g_TaaParams.z);
    float3 history = sampleHistoryCatmullRom(histUV, texSize, g_TaaParams.yz);
    // Ronin @bugfix 09/10/2026 DX9: plus its low bits: a remainder of -0.5..+0.5 of one 8-bit step (c6.w). One bilinear tap is enough.
    if (g_TaaDebug.w > 0.5f)
        history += (tex2Dlod(g_HistoryLo, float4(histUV, 0.0f, 0.0f)).rgb - 0.5f) * (1.0f / 255.0f);




    // The motion memory rides in alpha. A plain bilinear tap is enough for it — it is a decaying flag, not detail.
    float  histAlpha = tex2D(g_History, histUV).a;

    // Ronin @feature 24/09/2026 DX9: REACTIVE. A scrolling texture or a moving translucent layer keeps its history's
    // POSITION but not its CONTENT, so no motion vector can fix it: clamp the history and cap its weight (c10.z).
    // Velocity alpha: 0.4 reactive, 0.7 velocity + reactive, 1.0 velocity only.
    float reactive = 0.0f;
    float vanishBox = 0.0f;	// Ronin @bugfix 01/11/2026 DX9: inside a vanished mesh's box
    float autoR    = 0.0f;
    float effectLeft = 0.0f;	// Ronin @bugfix 09/10/2026 DX9: an effect covered this pixel last frame and covers it less now
    if (g_TaaExtra.z > 0.0f)
    {
        float ra = (g_TaaSharpen.y > 0.5f) ? tex2Dlod(g_Velocity, float4(uv, 0.0f, 0.0f)).a : 0.0f;
        // Ronin @bugfix 25/09/2026 DX9: reactive LAST frame too. A light that blinks off is not drawn, so it had no flag
        // the frame after and its history faded over ~10 frames: the trail along the runway lights.
        float rp = (g_TaaExtra.x > 0.5f) ? tex2Dlod(g_VelPrev, float4(camHistUV, 0.0f, 0.0f)).a : 0.0f;
        // Ronin @bugfix 01/11/2026 DX9: 0.45 = a VANISHED mesh's box, now or last frame: clamped, not capped, and
        // restarted only where the geometry that was there is gone (the depth block). Not reactive as a whole: that showed the box.
        vanishBox = (abs(ra - 0.45f) < 0.02f || abs(rp - 0.45f) < 0.02f) ? 1.0f : 0.0f;
        reactive = ((ra > 0.25f && ra < 0.85f && abs(ra - 0.45f) >= 0.02f) || (rp > 0.25f && rp < 0.85f && abs(rp - 0.45f) >= 0.02f)) ? 1.0f : 0.0f;
        // Ronin @feature 26/09/2026 DX9: AUTO-REACTIVE (c11.w). An effect drew here if the final frame differs from the copy
        // taken just before particles; LAST frame's flag rides in the depth history's alpha, so the pixel a flame just
        // left is clamped too. Particles had no velocity and no clamp: the beaded missile trail, the smeared fire.
        if (g_TaaVel.w > 0.0f)
        {
            float3 dd = abs(scene - toneCurve(tex2Dlod(g_Opaque, float4(uv, 0.0f, 0.0f)).rgb));
            float  pa = tex2Dlod(g_PrevDepth, float4(camHistUV, 0.0f, 0.0f)).a;
            // Ronin @bugfix 08/10/2026 DX9: PROPORTIONAL (c11.y). On/off left a hard-edged field with no AA round thin smoke; the
            // history now gives way only as far as the effect covers the pixel. Last frame's value rides in the alpha.
            float autoNow = saturate((max(dd.r, max(dd.g, dd.b)) - g_TaaVel.w) * g_TaaVel.y);
            autoR      = max(autoNow, pa);
            effectLeft = saturate(pa - autoNow) * g_TaaVel.x;
            reactive  = max(reactive, autoR);
        }
    }

    // Ronin @bugfix 09/10/2026 DX9: LOCAL LIGHTS (c10.w). A light that changed since last frame changes still pixels that carry no
    // velocity and no depth change. Clamped only, the moving-shadow mask's treatment - so its footprint no longer shows.
    float4 lightMask = (g_TaaExtra.w > 0.5f) ? tex2Dlod(g_LightMask, float4(uv, 0.0f, 0.0f)) : float4(0.0f, 0.0f, 0.0f, 0.0f);
    float  lightR    = lightMask.g;	// Ronin @feature 19/10/2026 DX9: green now - red is the reflections'
    // Ronin @feature 19/10/2026 DX9: a pane's screen-space reflection changed (ReflectiveRigid_ps): clamped as a light's change, and
    // with `taa reflmark 2` its sample count restarts where a unit moved there (blue, below).
    float  reflR     = lightMask.r;
    float  reflMover = lightMask.b;
    // Ronin @bugfix 01/11/2026 DX9: the FOG changed here (c29.x): still pixels with no other evidence kept the old fog, and a vanished
    // unit's reset box showed against them as a rectangle. Reactive: clamped and capped, at the pixel's own position.
    float fogR = 0.0f;
    if (g_FogPrm.x > 0.5f)
    {
        float2 fs = (uv - g_Viewport.xy) / g_Viewport.zw;
        float4 fp = mul(float4(fs.x * 2.0f - 1.0f, 1.0f - fs.y * 2.0f, tex2Dlod(g_Depth, float4(uv, 0.0f, 0.0f)).r, 1.0f), g_ToFog);
        fogR = (fp.w > 0.0001f) ? tex2Dlod(g_FogChange, float4(fp.xy / fp.w, 0.0f, 0.0f)).a : 0.0f;
        reactive = max(reactive, fogR);
    }

    // DEBUG 13: the mesh velocity target. Dim = nothing, GREEN = velocity, YELLOW = velocity + reactive, MAGENTA =
    // reactive only, CYAN = stopped this frame, ORANGE = auto-reactive (an effect drew over the scene, now or last frame).
    // Returned from pass 1; back to debug 0 and it recovers in a frame.
    if (g_TaaDebug.x > 12.5f && g_TaaDebug.x < 13.5f)
    {
        float da = (g_TaaSharpen.y > 0.5f) ? tex2Dlod(g_Velocity, float4(uv, 0.0f, 0.0f)).a : 0.0f;
        if (da < 0.1f)
        {
            float3 c13 = lerp(lerp(scene * 0.25f, float3(1.0f, 0.5f, 0.0f), autoR), float3(1.0f, 0.0f, 0.0f), effectLeft);	// RED: an effect just left
            c13 = lerp(c13, float3(1.0f, 1.0f, 1.0f), lightR);		// WHITE: a local light changed
            c13 = lerp(c13, float3(1.0f, 0.4f, 0.7f), reflR);		// Ronin @feature 19/10/2026 DX9: PINK: a reflection changed
            c13 = lerp(c13, float3(0.0f, 0.6f, 0.5f), fogR);		// Ronin @bugfix 01/11/2026 DX9: TEAL: the fog of war changed
            return float4(lerp(c13, float3(0.6f, 0.2f, 1.0f), reflMover), 0.0f);	// Ronin @feature 19/10/2026 DX9: PURPLE: a unit's
        }
        if (da < 0.25f)
            return float4(0.0f, 1.0f, 1.0f, 0.0f);
        if (da < 0.35f)
            return float4(0.2f, 0.4f, 1.0f, 0.0f);		// BLUE: ghost - moving preview or placement bib, no history kept
        if (da < 0.55f)
            return float4(1.0f, 0.0f, 1.0f, 0.0f);
        if (da < 0.85f)
            return float4(1.0f, 1.0f, 0.0f, 0.0f);
        if (da < 0.96f)
            return float4(0.6f, 0.6f, 1.0f, 0.0f);		// Ronin @bugfix 27/09/2026 DX9: LAVENDER - idle infantry (breathing only)
        return float4(0.0f, 1.0f, 0.0f, 0.0f);
    }

    // Ronin @diagnostic 27/09/2026 DX9: DEBUG 15 - how far each moving mesh moved THIS frame, in pixels, camera motion removed.
    // Measures whether breathing infantry and a waving flag separate by distance. Dim = no mesh velocity here.
    // < 0.05 navy, < 0.1 blue, < 0.2 cyan, < 0.35 green, < 0.5 yellow, < 1 orange, 1+ red.
    if (g_TaaDebug.x > 14.5f && g_TaaDebug.x < 15.5f)
    {
        float4 vm15 = (g_TaaSharpen.y > 0.5f) ? tex2Dlod(g_Velocity, float4(uv, 0.0f, 0.0f)) : float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (vm15.a <= 0.5f)
            return float4(scene * 0.25f, 0.0f);
        float  m15 = length(decodeVel(vm15) - (uv - camHistUV) / max(g_TaaParams.yz, 0.000001f));
        float3 c15 = (m15 < 0.05f) ? float3(0.0f, 0.0f, 0.5f) : (m15 < 0.1f) ? float3(0.0f, 0.3f, 1.0f) :
                     (m15 < 0.2f)  ? float3(0.0f, 1.0f, 1.0f) : (m15 < 0.35f) ? float3(0.0f, 1.0f, 0.0f) :
                     (m15 < 0.5f)  ? float3(1.0f, 1.0f, 0.0f) : (m15 < 1.0f)  ? float3(1.0f, 0.5f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
        return float4(c15, 0.0f);
    }

    // ---- neighbourhood clamp ------------------------------------------------------------------------------------
    // Reprojection fixes the camera; this fixes everything else. History that falls outside the colours actually
    // present around this pixel cannot be correct — a unit moved, a shadow swept past — so pull it back to the
    // nearest plausible value instead of trusting it. Same idea as the shadow EMA's history rejection.
    float2 du = float2(g_TaaParams.y, 0.0f);
    float2 dv = float2(0.0f, g_TaaParams.z);

    // VARIANCE CLIPPING (Salvi), not a min/max box. The raw min/max of a 3x3 is set by its two most extreme pixels,
    // so one bright subpixel sliver stretches the box wide enough to admit almost any history — and on the frames the
    // sliver is absent it collapses and rejects everything. Mean +/- gamma*sigma tracks what the neighbourhood is
    // actually made of, so it stays steady while a thin feature blinks in and out of it.
    float3 m1 = scene;
    float3 m2 = scene * scene;
    float3 lo = scene;
    float3 hi = scene;
    float3 s2;
    #define TAA_TAP(OFF) s2 = toneCurve(tex2D(g_Scene, OFF).rgb); m1 += s2; m2 += s2 * s2; lo = min(lo, s2); hi = max(hi, s2);
    TAA_TAP(uv - du)
    TAA_TAP(uv + du)
    TAA_TAP(uv - dv)
    TAA_TAP(uv + dv)
    TAA_TAP(uv - du - dv)
    TAA_TAP(uv + du - dv)
    TAA_TAP(uv - du + dv)
    TAA_TAP(uv + du + dv)
    #undef TAA_TAP

    // NOT intersected with the min/max. Intersecting can only ever SHRINK the bracket, and a tighter bracket rejects
    // more history — the opposite of what a thin feature needs. The whole point of the variance form is that when one
    // subpixel sliver makes the neighbourhood volatile, sigma grows and the bracket WIDENS to accommodate it.
    const float GAMMA = 1.25f;
    float3 mu    = m1 / 9.0f;
    float3 sigma = sqrt(abs(m2 / 9.0f - mu * mu));
    lo = mu - GAMMA * sigma;
    hi = mu + GAMMA * sigma;

    float3 clamped = clipToAABB(lo, hi, history);
    // Clamp only where there is EVIDENCE the history describes different geometry: camera motion, a sustained
    // disagreement, or a known mover. NOT merely because this frame disagrees - on a still pixel that disagreement
    // IS the subpixel sampling, and averaging it is the whole point. That is what destroyed the flagpole.
    // Camera motion only: depthBad is computed below, and the clamp is no longer the primary mechanism - the
    // sample counter is. This just handles disocclusions during a pan.
    // Ronin @bugfix 24/09/2026 DX9: ALSO clamp where a motion vector was applied. The velocity is per-OBJECT and rigid,
    // so a turret, wheels or a unit turning all carry residual error, and through a 0.9 history that compounds roughly
    // x9 into a smear. Motion vectors get the history approximately right and the clamp bounds the rest - the standard
    // pairing. A static pole never gets a velocity, so this does not bring its flicker back. `taa clamp 0` for an A/B.
    float clampNeed = saturate(max(max(speedPx * 0.5f, velTaken), max(max(reactive, vanishBox), max(lightR, reflR))));
    // Ronin @bugfix 27/09/2026 DX9: applied after the depth block, which adds the moving-shadow band (`taa shadowmask 1`).

    // ---- history weight: a per-pixel sample counter ---------------------------------------------------------------
    // Ronin @feature 22/09/2026 DX9: CONFIDENCE ACCUMULATION, per the literature. Two parts, and both matter:
    //
    // 1. The history is invalidated on EVIDENCE THAT IT DESCRIBES DIFFERENT GEOMETRY - here a depth discontinuity
    //    between this pixel and the reprojected previous depth - never on colour disagreement. On a still pixel a
    //    colour disagreement IS the subpixel sampling, and averaging it is the entire point; collapsing on it is
    //    what destroyed the flagpole and what every mechanism tried on 22/09 got wrong.
    // 2. The blend is 1/n off a per-pixel SAMPLE COUNTER, not a fixed weight. That is the part a lowered weight can
    //    never emulate: on invalidation n resets to 1, the pixel takes the current frame ENTIRELY in one frame - no
    //    ghost, no 2.5-frame smear - and then n climbs again at full rate so antialiasing rebuilds immediately.
    //    A fixed low weight instead starves the pixel forever, which is exactly the shimmer on movers.
    float curLinZ  = 0.0f;
    float prevLinZ = 0.0f;
    float depthBad = 0.0f;
    float vanishGone = 0.0f;	// Ronin @bugfix 01/11/2026 DX9: in a vanished mesh's box, where its geometry was
    float moverAny   = 0.0f;	// Ronin @bugfix 27/09/2026 DX9: in or near a moving shadow, now or last frame
    float moverBand  = 0.0f;	// Ronin @bugfix 27/09/2026 DX9: near where a moving shadow arrived or left this frame
    float moverIn    = 0.0f;	// in a moving shadow now (debug 14 only)
    if (g_TaaParams.w > 0.0f)
    {
        float dRaw = tex2Dlod(g_Depth, float4(uv, 0.0f, 0.0f)).r;
        float zn   = g_TaaDebug.y;
        float zf   = g_TaaDebug.z;
        curLinZ    = (zn * zf) / max(zf - dRaw * (zf - zn), 0.0001f);
        // Ronin @bugfix 22/09/2026 DX9: ask whether the geometry that WAS here is STILL here - not whether what is
        // here now was nearby before. The first version had it backwards and spared every trail: a trail pixel shows
        // ground now, and a pixel one over was ground last frame too, so it matched and never invalidated.
        //   * Static pole: last frame this pixel was pole; the pole is still a pixel away in THIS frame, so it
        //     matches and the history is kept. That is the jitter, not motion.
        //   * Tank trail: last frame this pixel was tank; there is no tank anywhere near it now, so nothing matches
        //     and the history is correctly thrown away. This is the case colour could never see.
        float2 dpx = g_TaaParams.yz;
        prevLinZ   = unpackDepth(tex2D(g_PrevDepth, histUV)) * zf;

        float4 nd;
        nd.x = tex2Dlod(g_Depth, float4(uv + float2(dpx.x, 0.0f), 0.0f, 0.0f)).r;
        nd.y = tex2Dlod(g_Depth, float4(uv - float2(dpx.x, 0.0f), 0.0f, 0.0f)).r;
        nd.z = tex2Dlod(g_Depth, float4(uv + float2(0.0f, dpx.y), 0.0f, 0.0f)).r;
        nd.w = tex2Dlod(g_Depth, float4(uv - float2(0.0f, dpx.y), 0.0f, 0.0f)).r;
        float4 nlin = (zn * zf) / max(zf - nd * (zf - zn), 0.0001f);

        // REVERSE: is the geometry that WAS here still here - prev depth against the CURRENT neighbourhood.
        float bestRev = abs(prevLinZ - curLinZ);
        bestRev = min(bestRev, min(min(abs(prevLinZ - nlin.x), abs(prevLinZ - nlin.y)),
                                   min(abs(prevLinZ - nlin.z), abs(prevLinZ - nlin.w))));

        float tol  = max(curLinZ * 0.01f, 0.5f);
        depthBad   = saturate((bestRev - tol) / max(tol, 0.0001f));
        // Ronin @bugfix 01/11/2026 DX9: in a vanished mesh's box, its own pixels: the same test, 5x stricter - a rock or a fence is
        // shallower than 1% of the view's depth. Still ground matches a neighbour within the jitter and is left alone.
        float tolV = max(curLinZ * 0.002f, 0.1f);
        vanishGone = vanishBox * saturate((bestRev - tolV) / tolV);

        // Ronin @bugfix 27/09/2026 DX9: MOVING SHADOWS (`taa shadowmask`). A still pixel in or near the shadow of a mesh that moved,
        // this frame or last, can have had its lighting changed by it - the trail carries no velocity and no depth change.
        // Evidence from the geometry, not from colour: a fence or grass that only shimmers under the jitter is never in it.
        // Still ground and buildings only - a mover's own pixels keep their velocity. Five taps widen it by c16.z.
        if (g_MoverPrm.x > 0.5f)
        {
            float isMesh = (g_TaaSharpen.y > 0.5f) ? tex2Dlod(g_Velocity, float4(uv, 0.0f, 0.0f)).a : 0.0f;
            if (isMesh < 0.1f)
            {
                float2 sn  = (uv - g_Viewport.xy) / g_Viewport.zw;
                float4 sc  = mul(float4(sn.x * 2.0f - 1.0f, 1.0f - sn.y * 2.0f, dRaw, 1.0f), g_ToSun);
                float3 sp  = sc.xyz / sc.w;
                float2 suv = sp.xy * float2(0.5f, -0.5f) + 0.5f + g_MoverPrm.y;
                if (suv.x > 0.0f && suv.x < 1.0f && suv.y > 0.0f && suv.y < 1.0f)
                {
                    float  rz  = sp.z - g_MoverPrm.w;		// in shadow = a caster nearer the sun than this, by the bias
                    float  dl  = g_MoverPrm.z;
                    float2 offs[5] = { float2(0.0f, 0.0f), float2(dl, 0.0f), float2(-dl, 0.0f), float2(0.0f, dl), float2(0.0f, -dl) };
                    [unroll] for (int k = 0; k < 5; ++k)
                    {
                        float4 e   = tex2Dlod(g_MoverMap, float4(suv + offs[k], 0.0f, 0.0f));
                        float  inN = (rz > unpack16(e.rg)) ? 1.0f : 0.0f;
                        float  inP = (rz > unpack16(e.ba)) ? 1.0f : 0.0f;
                        moverAny   = max(moverAny, max(inN, inP));
                        moverBand  = max(moverBand, abs(inN - inP));
                        if (k == 0)
                            moverIn = inN;
                    }
                }
            }
        }
    }

    // n lives in alpha, normalised by TAA_MAX_N. Camera motion counts as evidence too, the same as before.
    float motion   = saturate(speedPx / 16.0f);
    const float TAA_MAX_N = 32.0f;
    // Ronin @feature 24/09/2026 DX9: DISOCCLUSION. A mover covered this pixel last frame and no mover covers it now, so the
    // history here is the mover and the pixel is ground: reset it. This is the trail behind a moving unit - the ground
    // it just uncovered gets no velocity (the ground did not move) and the depth test misses a low vehicle, whose depth
    // differs from the ground's by less than its tolerance. Exact: last frame's velocity alpha says where a mover was.
    // Sampled at the CAMERA-reprojected position, so it follows a pan. A static pole never carries a velocity.
    float disocc = vanishGone;	// Ronin @bugfix 01/11/2026 DX9: a vanished mesh's own pixels restart, as a ghost's did
    // Ronin @bugfix 27/09/2026 DX9: GHOST (velocity alpha 0.3) - a moving build preview or the placement bib. Its pixels mix
    // preview and ground and change every frame, so no history is right under it.
    if (g_TaaExtra.y > 0.5f && g_TaaSharpen.y > 0.5f)
    {
        float gh = tex2Dlod(g_Velocity, float4(uv, 0.0f, 0.0f)).a;
        if (gh > 0.25f && gh < 0.35f)
            disocc = 1.0f;
    }
    if (g_TaaExtra.x > 0.5f && g_TaaExtra.y > 0.5f && velTaken < 0.75f)
    {
        // Exact rule (`taa disocc 1`): a mover covered THIS pixel last frame and nothing covers it now.
        // Ronin @bugfix 25/09/2026 DX9: "nothing" = no tracked mesh at all (a > 0.1), not just no mover - a unit that
        // stopped is drawn one frame as still (0.15), so its own pixels are not taken for vacated ground.
        float isHere = (g_TaaSharpen.y > 0.5f) ? tex2Dlod(g_Velocity, float4(uv, 0.0f, 0.0f)).a : 0.0f;
        // Ronin @bugfix 26/09/2026 DX9: only a mover that MOVED vacates a pixel (`taa disoccv`, px). A breathing
        // infantryman's clean silhouette wobbles by a fraction of a pixel every frame, and each edge pixel it slipped
        // off was reset: the dark outline in debug 3, the popping edges. Object motion = its velocity minus the camera's.
        float2 camPx    = (uv - camHistUV) / max(g_TaaParams.yz, 0.000001f);
        float4 wm       = tex2Dlod(g_VelPrev, float4(camHistUV, 0.0f, 0.0f));
        float  wasMover = vacates(wm, camPx);
        if (wasMover > 0.5f && isHere < 0.1f)
            disocc = 1.0f;
        // Ronin @bugfix 27/09/2026 DX9: a ghost left this pixel. It has no velocity, so the mover test above never fires.
        if (wm.a > 0.25f && wm.a < 0.35f && isHere < 0.1f)
            disocc = 1.0f;

        // Ronin @bugfix 24/09/2026 DX9: `taa disocc 2` adds the one-pixel EDGE BAND, with the right question: a mover
        // was NEAR here last frame and is NOT near here now. The velocity silhouette uses the CLEAN projection while
        // the history came from JITTERED frames, so the band just outside the old outline holds some of the object and
        // was never marked - the dotted outline a flag left behind. The first version asked only "was a mover near",
        // and a pole pixel beside its flag is near it on EVERY frame, so it was reset every frame: the flicker at the
        // flag-pole joint. Near on both frames keeps its history; near before and not now is the vacated band.
        if (g_TaaExtra.y > 1.5f && disocc < 0.5f)
        {
            float2 tx = float2(g_TaaParams.y, 0.0f);
            float2 ty = float2(0.0f, g_TaaParams.z);
            float4 n0 = tex2Dlod(g_VelPrev, float4(camHistUV + tx, 0.0f, 0.0f));
            float4 n1 = tex2Dlod(g_VelPrev, float4(camHistUV - tx, 0.0f, 0.0f));
            float4 n2 = tex2Dlod(g_VelPrev, float4(camHistUV + ty, 0.0f, 0.0f));
            float4 n3 = tex2Dlod(g_VelPrev, float4(camHistUV - ty, 0.0f, 0.0f));
            // a neighbour whose mesh moved away, by the same rule as above
            float wasNear = max(max(vacates(n0, camPx), vacates(n1, camPx)), max(vacates(n2, camPx), vacates(n3, camPx)));
            // Ronin @bugfix 27/09/2026 DX9: and a ghost's edge band, the preview's outline left in the jittered history.
            if ((n0.a > 0.25f && n0.a < 0.35f) || (n1.a > 0.25f && n1.a < 0.35f) ||
                (n2.a > 0.25f && n2.a < 0.35f) || (n3.a > 0.25f && n3.a < 0.35f)) wasNear = 1.0f;
            float isNear = 0.0f;
            if (g_TaaSharpen.y > 0.5f)		// this frame's velocity is bound
            {
                isNear = max(max(tex2Dlod(g_Velocity, float4(uv + tx, 0.0f, 0.0f)).a,
                                 tex2Dlod(g_Velocity, float4(uv - tx, 0.0f, 0.0f)).a),
                             max(tex2Dlod(g_Velocity, float4(uv + ty, 0.0f, 0.0f)).a,
                                 tex2Dlod(g_Velocity, float4(uv - ty, 0.0f, 0.0f)).a));
            }
            if (wasNear > 0.5f && isNear < 0.1f)	// a still or reactive neighbour counts as present
                disocc = 1.0f;
        }
    }
    // Ronin @bugfix 27/09/2026 DX9: the WHOLE moving shadow is clamped, now and last frame. Not only the band where it changed:
    // the mask is a separate drawing of the unit whose edge is not the real shadow's, and a pixel the band missed kept its lit
    // history - white pixels inside the shadow. Not reset: that showed the raw frame, pixelated. Tested in game: no trail, no
    // white pixels, no shimmer under moving shadows.
    history = lerp(history, clamped, g_TaaSharpen.w * saturate(max(clampNeed, moverAny)));
    // Ronin @bugfix 09/10/2026 DX9: AN EFFECT LEFT. A slow particle is still next door to the pixel it left, so the
    // clamp above keeps its ghost: the smeared snowflake. The frame BEFORE effects IS this pixel without it: take that.
    if (effectLeft > 0.0f)
        history = lerp(history, toneCurve(tex2Dlod(g_Opaque, float4(uv, 0.0f, 0.0f)).rgb), effectLeft);
    float invalid  = saturate(max(max(depthBad, motion), disocc));
    // Ronin @feature 19/10/2026 DX9: `taa reflmark 2` - where a unit moved through a reflection the pane starts over, as a
    // disocclusion does: the clamp kept its ghost while it was still next to the pixel it left. Fire, lights: clamped only.
    if (g_TaaSharpen.z > 1.5f)
        invalid = max(invalid, reflMover);
    // Ronin @bugfix 27/09/2026 DX9: DEBUG 14 - the moving-shadow mask. BLUE = in a moving mesh's shadow now, RED = clamped (in or
    // near one now or last frame), YELLOW = also weight-capped (`taa maskcap`). Returned from pass 1, like 13.
    if (g_TaaDebug.x > 13.5f && g_TaaDebug.x < 14.5f)
    {
        float3 dbg14 = lerp(scene * 0.25f + float3(0.0f, 0.0f, 0.45f) * moverIn, float3(1.0f, 0.0f, 0.0f), moverAny);
        return float4((g_MoverPrm.x > 1.5f && moverBand > 0.5f) ? float3(1.0f, 1.0f, 0.0f) : dbg14, 0.0f);
    }
    float n        = max(histAlpha * TAA_MAX_N, 1.0f);
    n              = lerp(min(n + 1.0f, TAA_MAX_N), 1.0f, invalid);
    // Ronin @bugfix 09/10/2026 DX9: where an effect left, the history above is one raw sample: count it as two, so the pixel's
    // anti-aliasing rebuilds in a few frames instead of holding that sample at full weight.
    n              = lerp(n, min(n, 2.0f), effectLeft);
    // The caller's weight caps how much history a converged pixel may keep, so `taa weight` still means something.
    float effWeight = min(1.0f - 1.0f / n, g_TaaParams.x);
    // Ronin @bugfix 08/10/2026 DX9: the cap scales with how reactive the pixel is (1 = the whole `taa reactive` cap).
    effWeight = min(effWeight, lerp(1.0f, g_TaaExtra.z, reactive));
    // Ronin @bugfix 27/09/2026 DX9: where a moving shadow arrived or left this frame (`taa maskcap`), keep at most half the
    // history - the rotor's treatment. The clamp alone left a faint trail on grass: its box holds dark blades, so a faint old
    // shadow looked plausible there (none on the road, whose box is tight). Not a reset: that showed the raw frame.
    if (g_MoverPrm.x > 1.5f && moverBand > 0.5f)
        effWeight = min(effWeight, 0.5f);


    // DEBUG 4: the history WITHOUT the current frame mixed in — effWeight forced to 1. If the pole is steady here but
    // flickers with the blend on, the accumulated history is fine and the current frame is what disturbs it. If it
    // flickers HERE too, the history itself never converges and the fault is upstream, in the reprojection or the
    // resample. Safe to return from this pass: it writes a real history value, just a fully-accumulated one.
    if (g_TaaDebug.x > 3.5f && g_TaaDebug.x < 4.5f)	// BOUNDED: unbounded, this ate 5 and 6
        return float4(history, n / TAA_MAX_N);

    // ---- tonemapped blending (Karis) -----------------------------------------------------------------------------
    // For thin geometry — a flagpole, a mast — which is narrower than a pixel, so the jitter rasterises it on some
    // frames and misses it on others and a plain lerp passes that full-amplitude blink straight through.
    //
    // The blend happens in TONEMAPPED space and is then inverted. Weighting the two sides by 1/(1+luminance) instead
    // is the form usually quoted, but it does almost nothing here: that form assumes HDR, where a highlight is 10x or
    // 100x and the attenuation bites. This engine renders LDR into A8R8G8B8, luminance sits around 0.3-0.8, and the
    // weights barely differ. Compressing the VALUES and inverting afterwards works at these magnitudes, and it costs
    // no highlight brightness because the inverse restores it.
    float3 sceneT = scene   / (1.0f + luminance(scene));
    float3 histT  = history / (1.0f + luminance(history));
    float3 mixed  = lerp(sceneT, histT, effWeight);
    float3 result = mixed / max(1.0f - luminance(mixed), 0.0001f);

    // Ronin @cleanup 26/09/2026 DX9: the unsharp mask that sat here is gone - it wrote into the history and compounded.
    // Sharpening is CAS in the screen copy now (casSharpen, `taa sharpen`).

    float storeAlpha = n / TAA_MAX_N;		// the sample counter, reprojected with the colour; debug 3 reads it back

    // Ronin @bugfix 09/10/2026 DX9: the 8-bit value the target will hold, and what rounding it drops - kept for next frame's history.
    float3 full   = saturate(result);
    float3 stored = floor(full * 255.0f + 0.5f) * (1.0f / 255.0f);
    lowBits       = (full - stored) * 255.0f + 0.5f;
    return float4(stored, storeAlpha);
}

struct PS_OUTPUT
{
    float4 color : COLOR0;
    float4 low   : COLOR1;       // the history's low bits; goes nowhere when no second target is bound
};

PS_OUTPUT main(float2 uv : TEXCOORD0)
{
    PS_OUTPUT o;
    if (g_TaaParams.x < 0.0f)		// Ronin @feature 22/10/2026 DX9: local contrast's surroundings, not a frame
    {
        o.color = surround(uv);
        o.low   = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return o;
    }
    float3 lowBits;
    o.color = resolve(uv, lowBits);
    o.low   = float4(lowBits, 0.0f);
    return o;
}
