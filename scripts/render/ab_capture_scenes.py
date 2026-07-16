#!/usr/bin/env python3
# TheSuperHackers @feature bobtista 16/07/2026 A/B scene capture harness (backend parity).
# For each scene in the manifest, runs the deployed game against a save and produces:
#   - one deterministic anchor screenshot at a logic frame (GGC_*_SCREENSHOT_LOGICFRAME),
#     comparable across runs, builds, and backends;
#   - a few interval screenshots used by ab_compare.py to build the within-run
#     animation mask (pixels that move during a run: smoke, mappers, water).
# Save logic counters resume from the save point, so per-save anchor frames are
# derived once via a probe run (LOGICFRAME=1 captures at the first simulated frame,
# and the produced filename encodes it) and cached in a sidecar next to the manifest.
#
# Usage:
#   python3 scripts/render/ab_capture_scenes.py \
#       --runtime-dir ~/TheSuperHackers/GeneralsZH-<suffix> \
#       --manifest scripts/render/ab_scenes.json \
#       --out-dir .context/ab/<label> [--scene NAME]... [--env K=V]...

import argparse
import glob
import json
import os
import re
import signal
import subprocess
import sys
import time

def wait_for_global_idle(max_wait=1800):
    """The game enforces a single instance machine-wide; a concurrent client from
    ANY runtime dir makes ours exit immediately. Queue behind other sessions."""
    waited = 0
    while subprocess.run(["pgrep", "-x", "generalszh"],
                         stdout=subprocess.DEVNULL).returncode == 0:
        if waited == 0:
            print("[ab] another generalszh instance is running; waiting for it to exit ...",
                  flush=True)
        time.sleep(10)
        waited += 10
        if waited >= max_wait:
            raise RuntimeError("timed out waiting for other game instances to exit")

def run_game(runtime_dir, save, out_base, env_extra, duration, screenshot_after, interval):
    wait_for_global_idle()
    env = os.environ.copy()
    env.update(env_extra)
    env["GGC_BGFX_SCREENSHOT_INTERVAL"] = str(interval)
    args = [
        os.path.join(runtime_dir, "run.sh"),
        "-nologo", "-quickstart", "-win",
        "-loadsave", save,
        "-bgfxScreenshotAfter", str(screenshot_after),
        out_base,
    ]
    proc = subprocess.Popen(args, cwd=runtime_dir, env=env,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        proc.wait(timeout=duration)
    except subprocess.TimeoutExpired:
        proc.terminate()
        try:
            proc.wait(timeout=15)
        except subprocess.TimeoutExpired:
            proc.kill()
    # Kill only clients launched from THIS runtime dir; other sessions may be
    # running the game from their own runtime dirs concurrently.
    subprocess.run(["pkill", "-f", os.path.join(runtime_dir, "generalszh")], check=False)
    time.sleep(2)

def probe_base_frame(runtime_dir, save, scratch_dir, duration):
    """Run once with LOGICFRAME=1 to learn the save's starting logic frame."""
    base = os.path.join(scratch_dir, "probe_" + os.path.splitext(save)[0])
    for f in glob.glob(base + "*"):
        os.remove(f)
    run_game(runtime_dir, save, base,
             {"GGC_BGFX_SCREENSHOT_LOGICFRAME": "1"},
             duration=duration, screenshot_after=999999, interval=999999)
    shots = glob.glob(base + ".L*.bmp") + glob.glob(base + ".L*.png")
    if not shots:
        return None
    m = re.search(r"\.L(\d+)\.", os.path.basename(shots[0]))
    return int(m.group(1)) if m else None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runtime-dir", required=True)
    ap.add_argument("--manifest", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--scene", action="append", default=None,
                    help="limit to named scene(s)")
    ap.add_argument("--env", action="append", default=[],
                    help="extra K=V env for the game (e.g. kill switches)")
    ap.add_argument("--probe-duration", type=int, default=90)
    args = ap.parse_args()

    runtime_dir = os.path.expanduser(args.runtime_dir)
    manifest = json.load(open(args.manifest))
    sidecar_path = args.manifest + ".baseframes"
    baseframes = {}
    if os.path.exists(sidecar_path):
        baseframes = json.load(open(sidecar_path))

    extra_env = {}
    for kv in args.env:
        k, _, v = kv.partition("=")
        extra_env[k] = v

    # The game runs with cwd=runtime_dir; screenshot paths must be absolute.
    args.out_dir = os.path.abspath(args.out_dir)
    os.makedirs(args.out_dir, exist_ok=True)
    scratch = os.path.join(args.out_dir, "_probe")
    os.makedirs(scratch, exist_ok=True)

    results = {}
    for scene in manifest["scenes"]:
        name = scene["name"]
        if args.scene and name not in args.scene:
            continue
        save = scene["save"]
        base_frame = baseframes.get(save)
        if base_frame is None:
            print("[ab] probing base logic frame for %s ..." % save, flush=True)
            base_frame = probe_base_frame(runtime_dir, save, scratch, args.probe_duration)
            if base_frame is None:
                print("[ab] PROBE FAILED for %s; skipping scene %s" % (save, name))
                continue
            baseframes[save] = base_frame
            json.dump(baseframes, open(sidecar_path, "w"), indent=1)
        anchor = base_frame + scene["anchor_delta"]

        scene_dir = os.path.join(args.out_dir, name)
        os.makedirs(scene_dir, exist_ok=True)
        for f in glob.glob(os.path.join(scene_dir, "*")):
            os.remove(f)
        out_base = os.path.join(scene_dir, "shot")

        env = dict(extra_env)
        env["GGC_BGFX_SCREENSHOT_LOGICFRAME"] = str(anchor)
        print("[ab] scene %s: save=%s anchor=L%d duration=%ds" %
              (name, save, anchor, scene["duration"]), flush=True)
        run_game(runtime_dir, save, out_base, env,
                 duration=scene["duration"],
                 screenshot_after=scene.get("interval_after", 400),
                 interval=scene.get("interval", 50))

        shots = sorted(glob.glob(out_base + "*"))
        anchors = [s for s in shots if ".L" in os.path.basename(s)]
        print("[ab]   captured %d shots (%d anchor)" % (len(shots), len(anchors)), flush=True)
        results[name] = {"anchor_frame": anchor, "shots": len(shots), "anchors": len(anchors)}

    json.dump(results, open(os.path.join(args.out_dir, "capture_summary.json"), "w"), indent=1)
    failed = [n for n, r in results.items() if r["anchors"] == 0]
    if failed:
        print("[ab] FAILED scenes (no anchor shot): %s" % ", ".join(failed))
        sys.exit(1)
    print("[ab] all scenes captured")

if __name__ == "__main__":
    main()
