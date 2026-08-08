#!/usr/bin/env python3
"""Frame-time distribution of a MOVING camera, over several hundred frames and several speeds.

  tools/walkbench.py
  tools/walkbench.py --frames 600 --speeds 1.4,4.2,15,150 --yaw-rate 60

A stutter is the tail of a per-frame series. A mean hides it, a minimum hides it twice, and a
standing bench cannot see it at all: the cost that produces it is paid when the world under the
camera changes. This declares one motion run per speed whose product is the profile -- one CSV row per frame -- and
reports p50/p90/p95/p99/max plus the share of frames past one and two 60 Hz periods — per speed, and
per quarter of the distance so a trend over the walk is visible.

Every speed stage is a separate process with its own warm-up, so the stages are comparable to each
other and not to whatever the previous stage left resident.

EVERY STAGE IS RUN --repeats TIMES, and the reason is measured: the same binary at the same speed over
600 frames gave p99 19.23 / 14.16 / 18.39 ms on three consecutive runs — a spread of 5.1 ms, which is
31 % of the whole 16.67 ms budget. A single run's p99 is a sample from a distribution and not the
distribution, so one run cannot settle whether a 4 ms change helped. What is reported is therefore the
MEDIAN p99 over the repeats and the spread around it, and a verdict the spread cannot support says so
instead of picking a side.

Exit 0 = every stage stayed under the declared ceilings, 1 = at least one did not, 2 = the spread is
too wide for the verdict to mean anything.
"""

import argparse
import hashlib
import os
import statistics
import subprocess
import sys
import tempfile

import runscene

HZ = 60.0
FRAME_MS = 1000.0 / HZ


def read_csv(path):
    with open(path) as f:
        head = f.readline().strip().split(",")
        rows = []
        for line in f:
            line = line.strip()
            if not line:
                continue
            rows.append(dict(zip(head, line.split(","))))
    return rows


def pct(v, q):
    s = sorted(v)
    return s[min(len(s) - 1, int(q * len(s)))]


def summarise(rows, key):
    return [float(r[key]) for r in rows]


def stage(binary, args, speed_ms, out_csv):
    """One stage: `frames` frames at `speed_ms` east plus a constant yaw rate, as ONE declared
    motion run -- two keyframes per channel, LINEAR, and the frame count is the run's own."""
    w, h = args.size.split("x")
    fps = 1.0 / args.dt
    scene = {
        "id": "bench", "kind": "run",
        "lat": 52.10602, "lon": 9.43453, "eyeM": 1.70,
        "yawDeg": 280, "pitchDeg": 0, "fovDeg": 60,
        "utc": "2026-08-06T17:40:00Z", "windDeg": 250, "windMs": 6.0, "cloudCover": 0.55,
        "capture": {"width": int(w), "height": int(h), "warmCeiling": args.warm},
        "runs": [{
            "kind": "motion", "frames": args.frames, "fps": fps,
            "world": "streaming", "give": "profile", "path": os.path.basename(out_csv),
            "animation": {
                "samplers": [
                    {"input": [0, args.frames], "output": [280, 280 + args.yaw_rate * args.frames * args.dt],
                     "interpolation": "LINEAR"},
                    {"input": [0, args.frames], "output": [0, speed_ms * args.frames * args.dt],
                     "interpolation": "LINEAR"},
                    {"input": [0, args.frames], "output": [0, args.frames * args.dt],
                     "interpolation": "LINEAR"}],
                "channels": [
                    {"sampler": 0, "target": "camera.yawDeg"},
                    {"sampler": 1, "target": "camera.eastM"},
                    {"sampler": 2, "target": "wind.clockS"}]}}]}
    r = runscene.run(binary, scene, out_root=os.path.dirname(out_csv) or ".", tiles=args.base)
    if r.returncode != 0 or not os.path.exists(out_csv):
        sys.stderr.write(r.stdout[-4000:] + r.stderr[-4000:])
        raise SystemExit("stage %.1f m/s failed" % speed_ms)
    return read_csv(out_csv)


def report(name, rows):
    ms = summarise(rows, "frameMs")
    n = len(ms)
    late1 = sum(1 for x in ms if x > FRAME_MS * 1.5)
    late2 = sum(1 for x in ms if x > FRAME_MS * 2.5)
    print("%-12s n=%d  dist=%.0f m" % (name, n, float(rows[-1]["distM"])))
    print("  frameMs   p50 %7.2f  p90 %7.2f  p95 %7.2f  p99 %7.2f  max %8.2f"
          % (statistics.median(ms), pct(ms, .90), pct(ms, .95), pct(ms, .99), max(ms)))
    print("  late      >1.5 frame %5.2f %%   >2.5 frame %5.2f %%"
          % (100.0 * late1 / n, 100.0 * late2 / n))
    for key in ("worldMs", "meshMs", "albedoMs", "buildingMs", "fieldMs", "renderMs", "gpuMs"):
        if key not in rows[0]:
            continue
        v = summarise(rows, key)
        if max(v) < 0.05:
            continue
        print("    %-11s p50 %6.2f  p99 %6.2f  max %7.2f  sum %7.1f"
              % (key, statistics.median(v), pct(v, .99), max(v), sum(v)))
    q = n // 4
    if q:
        print("  quarter p95 " + "  ".join("%6.2f" % pct(ms[i * q:(i + 1) * q], .95) for i in range(4)))
    return {"p50": statistics.median(ms), "p95": pct(ms, .95), "p99": pct(ms, .99), "max": max(ms),
            "late1": 100.0 * late1 / n, "late2": 100.0 * late2 / n}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default="build/gpu_walk")
    ap.add_argument("--frames", type=int, default=600)
    # --warm is the CEILING on streaming passes, not a count: gpu_walk warms until the world
    # reports full residency and fails if it does not get there. A stage that started
    # half-loaded would measure the loader.
    ap.add_argument("--warm", type=int, default=4000)
    ap.add_argument("--dt", type=float, default=1.0 / HZ)
    ap.add_argument("--size", default="1280x720")
    ap.add_argument("--base", default="")
    ap.add_argument("--speeds", default="1.4,4.2,15,150",
                    help="m/s: walk, jog, vehicle, and one far past anything a body does")
    ap.add_argument("--yaw-rate", type=float, default=20.0, help="deg/s, on every stage")
    ap.add_argument("--dir", default="")
    ap.add_argument("--max-p99", type=float, default=FRAME_MS * 1.5)
    ap.add_argument("--max-late2", type=float, default=0.5, help="%% of frames past 2.5 periods")
    ap.add_argument("--repeats", type=int, default=3,
                    help="runs per speed; the reported p99 is their median and the spread is printed")
    args = ap.parse_args()

    args.bin = os.path.abspath(args.bin)
    work = args.dir or tempfile.mkdtemp(prefix="walkbench_")
    os.makedirs(work, exist_ok=True)
    binhash = hashlib.md5(open(args.bin, "rb").read()).hexdigest()
    print("binary %s  md5 %s" % (args.bin, binhash))
    print("frames %d  warm<=%d  dt %.5f s  yaw %.1f deg/s  size %s"
          % (args.frames, args.warm, args.dt, args.yaw_rate, args.size))
    print()

    bad = []
    unresolved = []
    for s in [float(x) for x in args.speeds.split(",")]:
        runs = []
        for k in range(args.repeats):
            csv = os.path.join(work, "speed_%g_run%d.csv" % (s, k))
            rows = stage(args.bin, args, s, csv)
            runs.append(report("%.1f m/s #%d" % (s, k + 1), rows))
            # The binary is hashed again per run: a build swapping it mid-bench would make the runs
            # measurements of two different programs, and nothing in the numbers would say so.
            if not os.path.exists(args.bin):
                raise SystemExit("the binary went away mid-run — the runs are not one measurement")
            if hashlib.md5(open(args.bin, "rb").read()).hexdigest() != binhash:
                raise SystemExit("binary changed during the run")
            print()

        order = [r["p99"] for r in runs]          # RUN ORDER, not sorted: a monotone climb across the
        p99s = sorted(order)                       # set is thermal throttling and not measurement noise
        med = statistics.median(p99s)
        spread = p99s[-1] - p99s[0]
        worst_late2 = max(r["late2"] for r in runs)
        trend = "climbing" if order == sorted(order) and spread > 1.0 else "no trend"
        print("%.1f m/s  OVER %d RUNS: p99 median %.2f ms  spread %.2f ms  in run order %s  [%s]"
              % (s, len(runs), med, spread, "  ".join("%.2f" % v for v in order), trend))
        # A verdict needs the runs to AGREE, not to be narrow. All of them on one side of the ceiling
        # settles it however wide the spread is; runs that straddle it settle nothing, and saying so is
        # the only honest answer — a median picked out of a straddling set invents the side.
        over = sum(1 for v in p99s if v > args.max_p99)
        if 0 < over < len(p99s):
            unresolved.append("%.1f m/s: %d of %d runs over the %.2f ms ceiling (%s) — the runs straddle it"
                              % (s, over, len(p99s), args.max_p99, "  ".join("%.2f" % v for v in p99s)))
        elif over == len(p99s) or worst_late2 > args.max_late2:
            bad.append("%.1f m/s: EVERY run over — p99 median %.2f ms, best %.2f, %.2f %% past 2.5 periods"
                       % (s, med, p99s[0], worst_late2))
        print()

    print("csv in %s" % work)
    if unresolved:
        print("UNRESOLVED — these stages' runs STRADDLE the ceiling, so they have no verdict. Raise")
        print("--repeats or quiet the machine; do not read a side into them:")
        for u in unresolved:
            print("  " + u)
        return 2
    if bad:
        print("OVER CEILING (p99 <= %.2f ms, past-2.5-periods <= %.2f %%):" % (args.max_p99, args.max_late2))
        for b in bad:
            print("  " + b)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
