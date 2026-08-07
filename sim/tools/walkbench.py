#!/usr/bin/env python3
"""Frame-time distribution of a MOVING camera, over several hundred frames and several speeds.

  tools/walkbench.py
  tools/walkbench.py --frames 600 --speeds 1.4,4.2,15,150 --yaw-rate 60

A stutter is the tail of a per-frame series. A mean hides it, a minimum hides it twice, and a
standing bench cannot see it at all: the cost that produces it is paid when the world under the
camera changes. This runs `gpu_walk --seq-prof`, which writes one CSV row per sequence frame, and
reports p50/p90/p95/p99/max plus the share of frames past one and two 60 Hz periods — per speed, and
per quarter of the distance so a trend over the walk is visible.

Every speed stage is a separate process with its own warm-up, so the stages are comparable to each
other and not to whatever the previous stage left resident.

Exit 0 = every stage stayed under the declared ceilings, 1 = at least one did not.
"""

import argparse
import hashlib
import os
import statistics
import subprocess
import sys
import tempfile

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
    """One stage: `frames` sequence frames at `speed_ms`, stepping east, plus a constant yaw rate."""
    step = speed_ms * args.dt
    cmd = [binary,
           "--size", args.size,
           "--warm", str(args.warm),
           "--seq", str(args.frames),
           "--seq-dt", str(args.dt),
           "--seq-stepE", "%.6f" % step,
           "--seq-yaw", "%.6f" % (args.yaw_rate * args.dt),
           "--seq-prof", out_csv]
    if args.base:
        cmd += ["--base", args.base]
    r = subprocess.run(cmd, capture_output=True, text=True)
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
    for s in [float(x) for x in args.speeds.split(",")]:
        csv = os.path.join(work, "speed_%g.csv" % s)
        rows = stage(args.bin, args, s, csv)
        r = report("%.1f m/s" % s, rows)
        # The binary is hashed again per stage: a build swapping it mid-run would make the stages
        # measurements of two different programs, and nothing in the numbers would say so.
        if not os.path.exists(args.bin):
            raise SystemExit("the binary went away mid-run — the stages are not one measurement")
        if hashlib.md5(open(args.bin, "rb").read()).hexdigest() != binhash:
            raise SystemExit("binary changed during the run")
        if r["p99"] > args.max_p99 or r["late2"] > args.max_late2:
            bad.append("%.1f m/s: p99 %.2f ms, %.2f %% past 2.5 periods" % (s, r["p99"], r["late2"]))
        print()

    print("csv in %s" % work)
    if bad:
        print("OVER CEILING (p99 <= %.2f ms, past-2.5-periods <= %.2f %%):" % (args.max_p99, args.max_late2))
        for b in bad:
            print("  " + b)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
