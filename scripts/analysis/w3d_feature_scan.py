#!/usr/bin/env python3
# TheSuperHackers @feature bobtista 16/07/2026 W3D content-coverage scanner.
# Walks .big archives, parses every .w3d mesh's materials/shaders/textures, and
# cross-references the features shipped art actually uses against the bgfx
# backend's support matrix. Output: usage histograms plus a hit list of
# used-but-unsupported (or recently-fixed, needs-visual-check) features with
# example assets. Companion to the dx8wrapper parity audits.
#
# Usage:
#   python3 scripts/analysis/w3d_feature_scan.py --big-dir ~/TheSuperHackers/GeneralsZH \
#       [--out report.md] [--json report.json]

import argparse
import json
import os
import struct
import sys
from collections import Counter, defaultdict

# --- W3D chunk IDs (Core/Tools/WW3D/max2w3d/w3d_file.h) ----------------------
W3D_CHUNK_MESH = 0x00000000
W3D_CHUNK_MATERIAL_INFO = 0x00000028
W3D_CHUNK_SHADERS = 0x00000029
W3D_CHUNK_VERTEX_MATERIALS = 0x0000002A
W3D_CHUNK_VERTEX_MATERIAL = 0x0000002B
W3D_CHUNK_VERTEX_MATERIAL_NAME = 0x0000002C
W3D_CHUNK_VERTEX_MATERIAL_INFO = 0x0000002D
W3D_CHUNK_VERTEX_MAPPER_ARGS0 = 0x0000002E
W3D_CHUNK_VERTEX_MAPPER_ARGS1 = 0x0000002F
W3D_CHUNK_TEXTURES = 0x00000030
W3D_CHUNK_TEXTURE = 0x00000031
W3D_CHUNK_TEXTURE_NAME = 0x00000032
W3D_CHUNK_TEXTURE_INFO = 0x00000033
W3D_CHUNK_MATERIAL_PASS = 0x00000038
W3D_CHUNK_TEXTURE_STAGE = 0x00000048
W3D_CHUNK_STAGE_TEXCOORD_IDS = 0x0000004A
W3D_CHUNK_MESH_HEADER3 = 0x0000001F

# Chunks whose subchunks we recurse into. Wrapper chunks have the MSB set on
# their size field in most W3D files, but several containers ship without it,
# so recurse by ID.
CONTAINER_CHUNKS = {
    W3D_CHUNK_MESH,
    W3D_CHUNK_VERTEX_MATERIALS,
    W3D_CHUNK_VERTEX_MATERIAL,
    W3D_CHUNK_TEXTURES,
    W3D_CHUNK_TEXTURE,
    W3D_CHUNK_MATERIAL_PASS,
    W3D_CHUNK_TEXTURE_STAGE,
}

# --- Enum names (w3d_file.h / shader.h) --------------------------------------
DETAIL_COLOR = [
    "DISABLE", "DETAIL", "SCALE", "INVSCALE", "ADD", "SUB", "SUBR", "BLEND",
    "DETAILBLEND", "ADDSIGNED", "ADDSIGNED2X", "SCALE2X", "MODALPHAADDCOLOR",
]
DETAIL_ALPHA = ["DISABLE", "DETAIL", "SCALE", "INVSCALE"]
PRI_GRADIENT = ["DISABLE", "MODULATE", "ADD", "BUMPENVMAP", "BUMPENVMAPLUMINANCE", "MODULATE2X"]
SEC_GRADIENT = ["DISABLE", "ENABLE"]
SRC_BLEND = ["ZERO", "ONE", "SRC_ALPHA", "ONE_MINUS_SRC_ALPHA"]
DST_BLEND = ["ZERO", "ONE", "SRC_COLOR", "ONE_MINUS_SRC_COLOR", "SRC_ALPHA",
             "ONE_MINUS_SRC_ALPHA", "SRC_COLOR_PREFOG"]
DEPTH_COMPARE = ["NEVER", "LESS", "EQUAL", "LEQUAL", "GREATER", "NOTEQUAL", "GEQUAL", "ALWAYS"]
MAPPING = [
    "UV", "ENVIRONMENT", "CHEAP_ENVIRONMENT", "SCREEN", "LINEAR_OFFSET",
    "SILHOUETTE", "SCALE", "GRID", "ROTATE", "SINE_LINEAR_OFFSET",
    "STEP_LINEAR_OFFSET", "ZIGZAG_LINEAR_OFFSET", "WS_CLASSIC_ENV",
    "WS_ENVIRONMENT", "GRID_CLASSIC_ENV", "GRID_ENVIRONMENT", "RANDOM",
    "EDGE", "BUMPENV",
]

def enum_name(table, value):
    if 0 <= value < len(table):
        return table[value]
    return "UNKNOWN_%d" % value

# --- bgfx support matrix ------------------------------------------------------
# Derived from BgfxBackend.cpp BuildTssOpsForShader/Supports_Texture_Op, fs_uber.sc,
# mapper.cpp/matrixmapper.cpp, and textureloader.cpp's staging-format whitelist,
# as of the 16/07/2026 parity fix batch. "check" = recently fixed, wants a visual
# confirmation on a real asset; "hit" = used feature with no bgfx path;
# "info" = deliberate divergence worth knowing about.
VERDICTS = {
    ("post_detail_color", "ADDSIGNED"): "check(fixed 16/07: fix/bgfx-detail-color-funcs)",
    ("post_detail_color", "ADDSIGNED2X"): "check(fixed 16/07: fix/bgfx-detail-color-funcs)",
    ("post_detail_color", "SCALE2X"): "check(fixed 16/07: fix/bgfx-detail-color-funcs)",
    ("post_detail_color", "MODALPHAADDCOLOR"): "check(fixed 16/07: fix/bgfx-detail-color-funcs)",
    ("post_detail_color", "SUBR"): "check(fixed 16/07: was inverted)",
    ("pri_gradient", "BUMPENVMAP"): "hit(no bump-env: flat diffuse fallback)",
    ("pri_gradient", "BUMPENVMAPLUMINANCE"): "hit(no bump-env: flat diffuse fallback)",
    ("sec_gradient", "ENABLE"): "info(specular bit ignored; opt-in matFx Blinn-Phong instead)",
    ("mapping", "BUMPENV"): "hit(bump-env mapper feeds unimplemented bump shader)",
    ("mapping", "SILHOUETTE"): "hit(mapper unimplemented in engine, falls back to UV)",
    ("dst_blend", "SRC_COLOR_PREFOG"): "info(fog-dependent blend; fog unimplemented on bgfx)",
    ("texture_flag", "BUMPMAP_TYPE"): "hit(bump formats blocked by staging whitelist)",
}

# textureloader.cpp Is_CPU_Texture_Snapshot_Staging_Format whitelist (16/07/2026):
SUPPORTED_TEX_FORMATS = {
    "DXT1", "DXT2", "DXT3", "DXT4", "DXT5",
    "A8R8G8B8", "X8R8G8B8", "R8G8B8", "A4R4G4B4", "R5G6B5", "A1R5G5B5",
}

# --- BIG archive walker -------------------------------------------------------
def read_big_index(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] not in (b"BIGF", b"BIG4"):
        return None, None
    count = struct.unpack(">I", data[8:12])[0]
    entries = []
    off = 16
    for _ in range(count):
        o, s = struct.unpack(">II", data[off:off + 8])
        off += 8
        end = data.index(b"\x00", off)
        name = data[off:end].decode("latin1")
        off = end + 1
        entries.append((name, o, s))
    return data, entries

# --- W3D parser ----------------------------------------------------------------
class MeshRecord:
    def __init__(self, asset, mesh_name):
        self.asset = asset
        self.mesh_name = mesh_name
        self.pass_count = 1
        self.shaders = []       # list of dicts
        self.materials = []     # list of dicts
        self.textures = []      # list of dicts
        self.stage_counts = []  # textures stages per pass
        self.uv_channels = set()

def parse_chunks(buf, start, end, handler, depth=0):
    off = start
    while off + 8 <= end:
        cid, size = struct.unpack_from("<II", buf, off)
        payload = off + 8
        size &= 0x7FFFFFFF
        nxt = payload + size
        if nxt > end or size > (end - payload):
            return  # malformed; stop this level
        handler(cid, buf, payload, nxt, depth)
        if cid in CONTAINER_CHUNKS:
            parse_chunks(buf, payload, nxt, handler, depth + 1)
        off = nxt

def base_name(path):
    # .big entries and W3D texture references use Windows separators; normalize
    # before taking the basename so this works on POSIX hosts.
    return path.replace("\\", "/").rsplit("/", 1)[-1]

def cstr(buf, start, end):
    raw = buf[start:end]
    z = raw.find(b"\x00")
    if z >= 0:
        raw = raw[:z]
    return raw.decode("latin1", "replace")

def parse_w3d(asset_name, buf, records):
    state = {"mesh": None, "vm": None, "tex": None, "pass_stage_count": 0}

    def handler(cid, b, s, e, depth):
        if cid == W3D_CHUNK_MESH:
            if state["mesh"] is not None:
                finish_mesh()
            state["mesh"] = MeshRecord(asset_name, "?")
        m = state["mesh"]
        if m is None:
            return
        if cid == W3D_CHUNK_MESH_HEADER3 and e - s >= 48:
            m.mesh_name = cstr(b, s + 4 + 4, s + 4 + 4 + 32)
        elif cid == W3D_CHUNK_MATERIAL_INFO and e - s >= 16:
            m.pass_count = struct.unpack_from("<I", b, s)[0]
        elif cid == W3D_CHUNK_SHADERS:
            for so in range(s, e - 15, 16):
                (dcmp, dmask, _cmask, dblend, _fog, prig, secg, sblend, texing,
                 dcf, daf, _preset, atest, pdcf, pdaf, _pad) = struct.unpack_from("<16B", b, so)
                m.shaders.append({
                    "depth_compare": dcmp, "depth_mask": dmask, "dst_blend": dblend,
                    "pri_gradient": prig, "sec_gradient": secg, "src_blend": sblend,
                    "texturing": texing, "detail_color": dcf, "detail_alpha": daf,
                    "alpha_test": atest, "post_detail_color": pdcf, "post_detail_alpha": pdaf,
                })
        elif cid == W3D_CHUNK_VERTEX_MATERIAL:
            state["vm"] = {"name": "?", "stage0_mapping": 0, "stage1_mapping": 0,
                           "args0": "", "args1": ""}
            m.materials.append(state["vm"])
        elif cid == W3D_CHUNK_VERTEX_MATERIAL_NAME and state["vm"] is not None:
            state["vm"]["name"] = cstr(b, s, e)
        elif cid == W3D_CHUNK_VERTEX_MATERIAL_INFO and state["vm"] is not None and e - s >= 4:
            attrs = struct.unpack_from("<I", b, s)[0]
            state["vm"]["stage0_mapping"] = (attrs & 0x00FF0000) >> 16
            state["vm"]["stage1_mapping"] = (attrs & 0x0000FF00) >> 8
        elif cid == W3D_CHUNK_VERTEX_MAPPER_ARGS0 and state["vm"] is not None:
            state["vm"]["args0"] = cstr(b, s, e)
        elif cid == W3D_CHUNK_VERTEX_MAPPER_ARGS1 and state["vm"] is not None:
            state["vm"]["args1"] = cstr(b, s, e)
        elif cid == W3D_CHUNK_TEXTURE:
            state["tex"] = {"name": "?", "attributes": 0, "anim_frames": 1, "anim_fps": 0.0}
            m.textures.append(state["tex"])
        elif cid == W3D_CHUNK_TEXTURE_NAME and state["tex"] is not None:
            state["tex"]["name"] = cstr(b, s, e)
        elif cid == W3D_CHUNK_TEXTURE_INFO and state["tex"] is not None and e - s >= 12:
            attrs, _anim, frames, fps = struct.unpack_from("<HHIf", b, s)
            state["tex"]["attributes"] = attrs
            state["tex"]["anim_frames"] = frames
            state["tex"]["anim_fps"] = fps
        elif cid == W3D_CHUNK_MATERIAL_PASS:
            state["pass_stage_count"] = 0
        elif cid == W3D_CHUNK_TEXTURE_STAGE:
            state["pass_stage_count"] += 1
            m.stage_counts.append(state["pass_stage_count"])
        elif cid == W3D_CHUNK_STAGE_TEXCOORD_IDS:
            pass  # texcoord source arrays; channel >0 usage is caught via mappers

    def finish_mesh():
        if state["mesh"] is not None:
            records.append(state["mesh"])

    parse_chunks(buf, 0, len(buf), handler)
    finish_mesh()

# --- Texture format resolution ---------------------------------------------------
def dds_format(data):
    if data[:4] != b"DDS " or len(data) < 128:
        return None
    fourcc = data[84:88]
    if fourcc in (b"DXT1", b"DXT2", b"DXT3", b"DXT4", b"DXT5"):
        return fourcc.decode()
    flags, rgb_bits = struct.unpack_from("<I", data, 80)[0], struct.unpack_from("<I", data, 88)[0]
    if flags & 0x40:  # uncompressed RGB
        a_mask = struct.unpack_from("<I", data, 104)[0]
        if rgb_bits == 32:
            return "A8R8G8B8" if a_mask else "X8R8G8B8"
        if rgb_bits == 24:
            return "R8G8B8"
        if rgb_bits == 16:
            return "A4R4G4B4" if a_mask == 0xF000 else ("A1R5G5B5" if a_mask else "R5G6B5")
    if flags & 0x80000:  # bump du/dv (D3DFMT_V8U8 family)
        return "BUMP_UV"
    return "OTHER"

# --- Main -------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--big-dir", required=True)
    ap.add_argument("--out", default=None)
    ap.add_argument("--json", default=None)
    args = ap.parse_args()

    records = []
    tex_files = {}  # lowercase name -> (big, format-resolver payload offset/size)
    bigs = sorted(
        f for f in os.listdir(args.big_dir)
        if f.lower().endswith(".big") and os.path.isfile(os.path.join(args.big_dir, f))
    )
    parsed_assets = 0
    for bigname in bigs:
        path = os.path.join(args.big_dir, bigname)
        data, entries = read_big_index(path)
        if data is None:
            continue
        for name, off, size in entries:
            low = name.lower()
            if low.endswith(".w3d"):
                try:
                    parse_w3d("%s:%s" % (bigname, name), data[off:off + size], records)
                    parsed_assets += 1
                except Exception as ex:
                    print("parse error %s %s: %s" % (bigname, name, ex), file=sys.stderr)
            elif low.endswith((".dds", ".tga")):
                tex_files[base_name(low)] = (path, off, size)

    # Aggregate
    hist = defaultdict(Counter)
    examples = defaultdict(dict)

    def bump(category, name, asset):
        hist[category][name] += 1
        examples[category].setdefault(name, asset)

    anim_textures = Counter()
    for m in records:
        for sh in m.shaders:
            bump("post_detail_color", enum_name(DETAIL_COLOR, sh["post_detail_color"]), m.asset)
            bump("post_detail_alpha", enum_name(DETAIL_ALPHA, sh["post_detail_alpha"]), m.asset)
            bump("pri_gradient", enum_name(PRI_GRADIENT, sh["pri_gradient"]), m.asset)
            bump("sec_gradient", enum_name(SEC_GRADIENT, sh["sec_gradient"]), m.asset)
            bump("src_blend", enum_name(SRC_BLEND, sh["src_blend"]), m.asset)
            bump("dst_blend", enum_name(DST_BLEND, sh["dst_blend"]), m.asset)
            bump("depth_compare", enum_name(DEPTH_COMPARE, sh["depth_compare"]), m.asset)
            bump("alpha_test", "ENABLE" if sh["alpha_test"] else "DISABLE", m.asset)
        for vm in m.materials:
            bump("mapping", enum_name(MAPPING, vm["stage0_mapping"]), m.asset)
            if vm["stage1_mapping"]:
                bump("mapping", enum_name(MAPPING, vm["stage1_mapping"]), m.asset)
        for tx in m.textures:
            attrs = tx["attributes"]
            if attrs & 0x0004:
                bump("texture_flag", "NO_LOD", m.asset)
            if attrs & 0x0008 or attrs & 0x0010:
                bump("texture_flag", "CLAMP", m.asset)
            if attrs & 0x1000:
                bump("texture_flag", "BUMPMAP_TYPE", m.asset)
            if tx["anim_frames"] > 1:
                bump("texture_flag", "ANIMATED_FRAMES", m.asset)
                anim_textures[tx["name"]] += 1
        if m.pass_count > 1:
            bump("multipass", "PASSES_%d" % m.pass_count, m.asset)
        for c in m.stage_counts:
            if c > 2:
                bump("multipass", "STAGES_%d" % c, m.asset)

    # Texture format sweep over shipped image files referenced by materials
    fmt_hist = Counter()
    fmt_examples = {}
    referenced = set()
    for m in records:
        for tx in m.textures:
            referenced.add(base_name(tx["name"].lower()))
    for name in sorted(referenced):
        base = os.path.splitext(name)[0]
        for ext in (".dds", ".tga"):
            key = base + ext
            if key in tex_files:
                path, off, size = tex_files[key]
                if ext == ".tga":
                    fmt = "TGA"
                else:
                    with open(path, "rb") as f:
                        f.seek(off)
                        fmt = dds_format(f.read(min(size, 160))) or "OTHER"
                fmt_hist[fmt] += 1
                fmt_examples.setdefault(fmt, key)
                break

    # Report
    lines = []
    lines.append("# W3D feature scan")
    lines.append("")
    lines.append("Scanned %d .w3d assets across %d .big archives; %d meshes." %
                 (parsed_assets, len(bigs), len(records)))
    lines.append("")
    lines.append("## Hit list (used features needing attention)")
    lines.append("")
    hits = []
    for (cat, name), verdict in VERDICTS.items():
        count = hist[cat].get(name, 0)
        if count:
            hits.append((verdict, cat, name, count, examples[cat][name]))
    if fmt_hist.get("BUMP_UV", 0) or fmt_hist.get("OTHER", 0):
        for fmt in ("BUMP_UV", "OTHER"):
            if fmt_hist.get(fmt):
                hits.append(("hit(texture format outside staging whitelist)",
                             "texture_format", fmt, fmt_hist[fmt], fmt_examples[fmt]))
    if hits:
        lines.append("| verdict | category | feature | uses | example |")
        lines.append("|---|---|---|---|---|")
        for verdict, cat, name, count, ex in sorted(hits):
            lines.append("| %s | %s | %s | %d | %s |" % (verdict, cat, name, count, ex))
    else:
        lines.append("No used-but-unsupported features found.")
    lines.append("")
    lines.append("## Usage histograms")
    for cat in sorted(hist):
        lines.append("")
        lines.append("### %s" % cat)
        for name, count in hist[cat].most_common():
            lines.append("- %s: %d  (e.g. %s)" % (name, count, examples[cat][name]))
    lines.append("")
    lines.append("### texture_format (referenced by materials, resolved on disk)")
    for fmt, count in fmt_hist.most_common():
        lines.append("- %s: %d  (e.g. %s)" % (fmt, count, fmt_examples[fmt]))
    lines.append("")
    lines.append("### animated multi-frame textures")
    for name, count in anim_textures.most_common(20):
        lines.append("- %s: %d meshes" % (name, count))

    report = "\n".join(lines)
    if args.out:
        with open(args.out, "w") as f:
            f.write(report + "\n")
    else:
        print(report)
    if args.json:
        with open(args.json, "w") as f:
            json.dump({
                "hist": {k: dict(v) for k, v in hist.items()},
                "examples": {k: dict(v) for k, v in examples.items()},
                "texture_formats": dict(fmt_hist),
            }, f, indent=1)

if __name__ == "__main__":
    main()
