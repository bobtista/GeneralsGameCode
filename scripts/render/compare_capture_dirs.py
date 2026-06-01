#!/usr/bin/env python3
"""Compare matching BMP screenshots from two capture directories."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from diff_screenshots import compare, read_bmp, write_bmp


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference_dir", type=Path)
    parser.add_argument("actual_dir", type=Path)
    parser.add_argument("--pattern", default="*.bmp")
    parser.add_argument("--threshold", type=int, default=8)
    parser.add_argument("--max-differing-percent", type=float, default=0.0)
    parser.add_argument("--diff-dir", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    reference_paths = sorted(args.reference_dir.glob(args.pattern))
    if not reference_paths:
        print(f"no reference captures matched {args.pattern!r} in {args.reference_dir}", file=sys.stderr)
        return 2

    results = []
    failures = 0
    missing = 0

    for reference_path in reference_paths:
        actual_path = args.actual_dir / reference_path.name
        if not actual_path.exists():
            missing += 1
            failures += 1
            results.append({
                "frame": reference_path.name,
                "status": "missing",
                "reference": str(reference_path),
                "actual": str(actual_path),
            })
            continue

        summary, diff = compare(read_bmp(reference_path), read_bmp(actual_path), args.threshold)
        summary["frame"] = reference_path.name
        summary["reference"] = str(reference_path)
        summary["actual"] = str(actual_path)
        summary["status"] = "ok"

        if args.diff_dir is not None and summary["differing_pixels"] > 0:
            diff_path = args.diff_dir / reference_path.name
            write_bmp(diff_path, diff)
            summary["diff_path"] = str(diff_path)

        if summary["differing_percent"] > args.max_differing_percent:
            summary["status"] = "failed"
            failures += 1

        results.append(summary)

    report = {
        "reference_dir": str(args.reference_dir),
        "actual_dir": str(args.actual_dir),
        "pattern": args.pattern,
        "threshold": args.threshold,
        "max_differing_percent": args.max_differing_percent,
        "frames": len(results),
        "missing": missing,
        "failures": failures,
        "results": results,
    }

    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(
            "frames={frames} missing={missing} failures={failures} "
            "threshold={threshold} max_differing_percent={max_differing_percent}".format(**report)
        )
        for result in results:
            if result["status"] == "missing":
                print(f"{result['frame']}: missing actual")
                continue
            print(
                "{frame}: {status} differing={differing_pixels} "
                "({differing_percent:.4f}%) max_delta={max_channel_delta}".format(**result)
            )

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
