// TheSuperHackers @feature bobtista 01/06/2026
// Per-frame draw-call dump for backend diagnostic. GGC_DRAWLOG_AFTER=<N>
// + GGC_DRAWLOG_PATH=<basepath> opens one CSV per logged frame at
// <basepath>.<frameIndex>.csv with one row per DX8Wrapper::Draw call.
// Optional GGC_DRAWLOG_INTERVAL=<K> repeats every K frames after the first.

#pragma once

bool DrawCallLog_Is_Active();

void DrawCallLog_Record(
    unsigned primitive_type,
    unsigned polygon_count,
    unsigned vertex_count,
    unsigned vb_type,
    unsigned ib_type,
    unsigned shader_bits,
    unsigned sorted_draw_flags,
    const char * texture0_name);

void DrawCallLog_End_Frame();
