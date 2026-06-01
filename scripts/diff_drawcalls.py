#!/usr/bin/env python3
"""Diff two DrawCallLog CSVs to spot backend-specific divergence.

Usage:
    python diff_drawcalls.py dx8_frame.csv bgfx_frame.csv [--detail]

Reports:
  1. Histogram by (vb_type, ib_type, shader_bits, tex0) — which buckets
     appear in one backend but not the other.
  2. Per-row diff when --detail: matched by draw_idx, prints rows that
     differ. (Note: a missing draw in one build shifts indices, so this
     is most useful when the high-level histogram already aligns.)

Frame snapshots are written by the engine when GGC_DRAWLOG_AFTER+PATH
are set. Frames are written as <basepath>.<frameIndex>.csv.
"""

import argparse
import csv
import os
import sys
from collections import Counter, defaultdict


def load_rows(path):
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        return list(reader)


def bucket_key(row):
    return (
        row["vb_type"],
        row["ib_type"],
        row["shader_bits"],
        row["tex0"],
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dx8_csv")
    ap.add_argument("bgfx_csv")
    ap.add_argument("--detail", action="store_true")
    args = ap.parse_args()

    dx8 = load_rows(args.dx8_csv)
    bgfx = load_rows(args.bgfx_csv)

    print(f"dx8:  {len(dx8)} draws ({os.path.basename(args.dx8_csv)})")
    print(f"bgfx: {len(bgfx)} draws ({os.path.basename(args.bgfx_csv)})")
    print(f"delta: {len(bgfx) - len(dx8):+d} draws (positive = bgfx has more)")
    print()

    h_dx8 = Counter(bucket_key(r) for r in dx8)
    h_bgfx = Counter(bucket_key(r) for r in bgfx)

    only_bgfx = {k: v for k, v in h_bgfx.items() if k not in h_dx8}
    only_dx8 = {k: v for k, v in h_dx8.items() if k not in h_bgfx}
    shared = {k: (h_dx8[k], h_bgfx[k]) for k in h_dx8 if k in h_bgfx}

    if only_bgfx:
        print("== Draws ONLY in BGFX (missing from DX8) ==")
        for k, n in sorted(only_bgfx.items(), key=lambda kv: -kv[1]):
            vb, ib, sh, tex = k
            print(f"  bgfx={n:4d}   vb={vb} ib={ib} shader={sh} tex={tex!r}")
        print()

    if only_dx8:
        print("== Draws ONLY in DX8 (missing from BGFX) ==")
        for k, n in sorted(only_dx8.items(), key=lambda kv: -kv[1]):
            vb, ib, sh, tex = k
            print(f"  dx8={n:4d}   vb={vb} ib={ib} shader={sh} tex={tex!r}")
        print()

    mismatched_counts = {k: v for k, v in shared.items() if v[0] != v[1]}
    if mismatched_counts:
        print("== Buckets present in both but DIFFERENT counts ==")
        for k, (a, b) in sorted(
            mismatched_counts.items(), key=lambda kv: -abs(kv[1][0] - kv[1][1])
        ):
            vb, ib, sh, tex = k
            print(f"  dx8={a:4d} bgfx={b:4d}   vb={vb} ib={ib} shader={sh} tex={tex!r}")
        print()

    print("== Summary by VB type ==")
    by_vb = defaultdict(lambda: [0, 0])
    for r in dx8:
        by_vb[r["vb_type"]][0] += 1
    for r in bgfx:
        by_vb[r["vb_type"]][1] += 1
    for vb, (a, b) in sorted(by_vb.items()):
        marker = "  " if a == b else "!!"
        print(f"  {marker} vb_type={vb}: dx8={a:5d}  bgfx={b:5d}  delta={b-a:+d}")

    if args.detail:
        print()
        print("== Per-row diff (matched by draw_idx) ==")
        n = min(len(dx8), len(bgfx))
        diffs = 0
        for i in range(n):
            a, b = dx8[i], bgfx[i]
            keys = [k for k in a.keys() if k != "draw_idx"]
            if any(a[k] != b[k] for k in keys):
                diffs += 1
                if diffs > 50:
                    print(f"  ... (>50 row diffs; showing first 50)")
                    break
                print(f"  idx={i}")
                for k in keys:
                    if a[k] != b[k]:
                        print(f"    {k}: dx8={a[k]!r}  bgfx={b[k]!r}")


if __name__ == "__main__":
    main()
