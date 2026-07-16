#!/usr/bin/env python3
# TheSuperHackers @feature bobtista 16/07/2026 A/B capture comparison (backend parity).
# Compares two ab_capture_scenes.py output dirs (bgfx-vs-bgfx regression mode, or
# bgfx-vs-dx8 cross-backend mode). Per scene:
#   1. Animation mask: pixels that move between interval shots WITHIN each run
#     (smoke, UV mappers, water) are excluded from the static comparison — their
#     phase is wall-clock driven and never matches across runs.
#   2. Animation parity: a region that animates in run A must also animate in run
#     B (catches the frozen-mapper class of bug mechanically).
#   3. Anchor diff: logic-frame anchor shots compared over the static remainder,
#     tiled; worst clusters reported with a magenta overlay artifact.
# Optional baseline JSON holds per-scene accepted thresholds (cross-backend mode
# where deliberate divergences exist). Exit 0 = within thresholds.
#
# Usage:
#   python3 scripts/render/ab_compare.py A_DIR B_DIR [--baseline file.json]
#       [--out-dir artifacts/] [--anim-thresh 12] [--static-thresh 12]

import argparse
import glob
import json
import os
import sys

import numpy as np
from PIL import Image

UI_MASK_TOP = 40  # wall-clock text

def load(path):
    return np.array(Image.open(path).convert("RGB"), dtype=np.int16)

def scene_shots(scene_dir):
    shots = sorted(glob.glob(os.path.join(scene_dir, "shot*")))
    anchors = [s for s in shots if ".L" in os.path.basename(s)]
    intervals = [s for s in shots if ".L" not in os.path.basename(s)]
    return anchors, intervals

def animation_mask(intervals, thresh):
    """Pixels that change between consecutive interval shots within one run."""
    mask = None
    prev = None
    for f in intervals:
        cur = load(f)
        if prev is not None and cur.shape == prev.shape:
            d = (np.abs(cur - prev).max(axis=2) > thresh)
            mask = d if mask is None else (mask | d)
        prev = cur
    return mask

def dilate(mask, r=3):
    if mask is None:
        return None
    out = mask.copy()
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            out |= np.roll(np.roll(mask, dy, axis=0), dx, axis=1)
    return out

def tile_report(diff_mask, tile=32, top=8):
    ys, xs = np.where(diff_mask)
    from collections import Counter
    c = Counter((int(x // tile), int(y // tile)) for x, y in zip(xs, ys))
    return [(cx * tile, cy * tile, n) for (cx, cy), n in c.most_common(top)]

def compare_scene(name, dir_a, dir_b, args, baseline):
    anchors_a, intervals_a = scene_shots(dir_a)
    anchors_b, intervals_b = scene_shots(dir_b)
    if not anchors_a or not anchors_b:
        return {"scene": name, "status": "ERROR", "reason": "missing anchor shot"}

    a = load(anchors_a[0])
    b = load(anchors_b[0])
    if a.shape != b.shape:
        return {"scene": name, "status": "ERROR", "reason": "resolution mismatch %s vs %s"
                % (a.shape, b.shape)}

    am_a = dilate(animation_mask(intervals_a, args.anim_thresh))
    am_b = dilate(animation_mask(intervals_b, args.anim_thresh))
    # Anchor-adjacent self-mask: pixels that differ between the anchor and the
    # temporally nearest interval shot in the SAME run are wall-clock animated
    # even if the interval sweep missed them.
    for anchors, intervals, am in ((anchors_a, intervals_a, am_a), (anchors_b, intervals_b, am_b)):
        if intervals and am is not None:
            near = load(intervals[len(intervals) // 2])
            anc = load(anchors[0])
            if near.shape == anc.shape:
                am |= dilate(np.abs(anc - near).max(axis=2) > args.anim_thresh)

    # Animation parity: tiles that animate on one side but are dead on the other.
    parity_fail_px = 0
    parity_tiles = []
    if am_a is not None and am_b is not None:
        tile = 32
        h, w = am_a.shape
        for ty in range(0, h - tile, tile):
            for tx in range(0, w - tile, tile):
                if ty < UI_MASK_TOP:
                    continue
                na = int(am_a[ty:ty + tile, tx:tx + tile].sum())
                nb = int(am_b[ty:ty + tile, tx:tx + tile].sum())
                # a tile solidly animated on one side and untouched on the other
                if (na > 96 and nb == 0) or (nb > 96 and na == 0):
                    parity_fail_px += abs(na - nb)
                    parity_tiles.append((tx, ty, na, nb))

    # Static anchor comparison over everything that never animated in either run.
    excl = np.zeros(a.shape[:2], dtype=bool)
    excl[:UI_MASK_TOP, :] = True
    if am_a is not None:
        excl |= am_a
    if am_b is not None:
        excl |= am_b
    d = np.abs(a - b).max(axis=2)
    d[excl] = 0
    static_diff = d > args.static_thresh
    static_count = int(static_diff.sum())
    static_mean = float(d[~excl].mean()) if (~excl).any() else 0.0

    limits = baseline.get(name, {})
    max_static = limits.get("max_static_px", args.max_static_px)
    max_parity = limits.get("max_parity_tiles", args.max_parity_tiles)
    status = "PASS"
    if static_count > max_static or len(parity_tiles) > max_parity:
        status = "FAIL"

    if args.out_dir and status == "FAIL":
        os.makedirs(args.out_dir, exist_ok=True)
        overlay = np.array(Image.open(anchors_a[0]).convert("RGB"))
        overlay[static_diff] = [255, 0, 255]
        for tx, ty, _, _ in parity_tiles:
            overlay[ty:ty + 32, tx:tx + 2] = [255, 255, 0]
            overlay[ty:ty + 32, tx + 30:tx + 32] = [255, 255, 0]
            overlay[ty:ty + 2, tx:tx + 32] = [255, 255, 0]
            overlay[ty + 30:ty + 32, tx:tx + 32] = [255, 255, 0]
        Image.fromarray(overlay).save(os.path.join(args.out_dir, "%s_overlay.png" % name))

    return {
        "scene": name, "status": status,
        "static_diff_px": static_count, "static_mean": round(static_mean, 3),
        "parity_fail_tiles": len(parity_tiles),
        "parity_examples": parity_tiles[:6],
        "worst_static_tiles": tile_report(static_diff),
        "limits": {"max_static_px": max_static, "max_parity_tiles": max_parity},
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir_a")
    ap.add_argument("dir_b")
    ap.add_argument("--baseline", default=None)
    ap.add_argument("--out-dir", default=None)
    ap.add_argument("--anim-thresh", type=int, default=12)
    ap.add_argument("--static-thresh", type=int, default=12)
    ap.add_argument("--max-static-px", type=int, default=500)
    # Parity is advisory by default: interval shots are render-frame timed, so a
    # unit driving through one run's sample window but not the other's animates
    # tiles asymmetrically. The frozen-surface bug class this check targets shows
    # up as MANY dead tiles, well above this allowance.
    ap.add_argument("--max-parity-tiles", type=int, default=6)
    ap.add_argument("--write-baseline", default=None,
                    help="record observed levels (x1.5 headroom) as the accepted-divergence baseline JSON")
    args = ap.parse_args()

    baseline = json.load(open(args.baseline)) if args.baseline else {}
    scenes = sorted(
        d for d in os.listdir(args.dir_a)
        if os.path.isdir(os.path.join(args.dir_a, d)) and not d.startswith("_")
    )
    results = []
    for name in scenes:
        db = os.path.join(args.dir_b, name)
        if not os.path.isdir(db):
            results.append({"scene": name, "status": "ERROR", "reason": "missing in B"})
            continue
        results.append(compare_scene(name, os.path.join(args.dir_a, name), db, args, baseline))

    fail = False
    for r in results:
        line = "%-24s %s" % (r["scene"], r["status"])
        if r["status"] == "PASS":
            line += "  static=%dpx parity=%d" % (r["static_diff_px"], r["parity_fail_tiles"])
        elif r["status"] == "FAIL":
            fail = True
            line += "  static=%dpx (limit %d) parity=%d tiles (limit %d) worst=%s" % (
                r["static_diff_px"], r["limits"]["max_static_px"],
                r["parity_fail_tiles"], r["limits"]["max_parity_tiles"],
                r["worst_static_tiles"][:3])
        else:
            fail = True
            line += "  " + r.get("reason", "")
        print(line)
    if args.out_dir:
        os.makedirs(args.out_dir, exist_ok=True)
        json.dump(results, open(os.path.join(args.out_dir, "ab_results.json"), "w"), indent=1)
    if args.write_baseline:
        bl = {}
        for r in results:
            if "static_diff_px" in r:
                bl[r["scene"]] = {
                    "max_static_px": max(args.max_static_px, int(r["static_diff_px"] * 1.5)),
                    "max_parity_tiles": max(args.max_parity_tiles, int(r["parity_fail_tiles"] * 1.5)),
                }
        json.dump(bl, open(args.write_baseline, "w"), indent=1)
        print("baseline written: %s" % args.write_baseline)
        sys.exit(0)
    sys.exit(1 if fail else 0)

if __name__ == "__main__":
    main()
