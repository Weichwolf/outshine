#!/usr/bin/env python3
"""Actor-scaling bench: generate a cast of N into the mod's missions/scale/, run fb-gym, report wall time
per sim-second and the run fingerprint.

  gen   PROFILE N            write <mod>/src/missions/scale/<profile>-<n>.fbm and print its path
  run   PROFILE N [opts]     run it, print one CSV row per repetition
  sweep PROFILE N,N,... [..] the whole curve, mean/min/max per point, fingerprint per thread count

The measured number is the runner's OWN `wallS` out of the SUMMARY line (steady_clock over the tick
loop only), so JSBSim model loading and process start are outside it. `--elev const` by decision: the
baked DEM is an I/O path whose cost is not the cast's.

Determinism is checked at every point by the same fingerprint the campaign layer uses
(tools/fb_campaign_verify.mission_fingerprint): SHA-256 over all telemetry*.csv plus the normalised
events.log plus the exit code.
"""
import argparse
import csv
import os
import shutil
import statistics
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fb_campaign_verify import mission_fingerprint
import fb_mod as mod

SIM = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GYM = os.path.join(SIM, "build", "fb-gym")
SCALE_DIR = os.path.join(mod.MISSIONS, "scale")

# A lattice wide enough that no two actors of a thousand-strong cast share a spawn point, and coarse
# enough that they stay inside a plausible operating area: 0.02 deg ~ 2.2 km lat, 1.5 km lon at 47 N.
# SPACING IS A MEASUREMENT VARIABLE, not decoration: a registry walk always runs N iterations, but how
# much work each iteration does depends on whether the peer is inside a sensor's gate. --spacing shrinks
# the lattice so that every peer is inside everyone's gate (the DENSE case).
LAT0, LON0, DLAT, DLON, COLS = 46.30, 5.60, 0.02, 0.02, 32
SPACING = [DLAT, DLON]


def grid(i, side):
    """Actor i of one side onto the lattice; side 1 sits 1.5 deg east and flies the other way."""
    lat = LAT0 + (i // COLS) * SPACING[0]
    lon = LON0 + (i % COLS) * SPACING[1] + (1.5 if side else 0.0)
    return lat, lon


def f16(name, i, side, walkers, task, objectives, alt):
    """`walkers` is the set of registry-walking sensors switched ON: eye, fcr, dl, rwr."""
    lat, lon = grid(i, side)
    hdg = 270.0 if side else 90.0
    wlon = lon - 4.0 if side else lon + 4.0
    L = [f"unit {name}", "  team " + ("hostile" if side else "friendly"),
         "  module " + ("mig29" if side else "f16"),
         f"  spawn {lat:.5f} {lon:.5f} {alt} {hdg:.1f} 450",
         "  set gear up", "  set fuel_pct 70"]
    if "eye" not in walkers:
        L += ["  set visual off"]
    if side:   # the MiG-29's own key names for the same three boxes
        L += ["  set n019_mode rad", "  set n019_range_nm 27", "  set n019_emission illum"] \
            if "fcr" in walkers else ["  set n019_mode off"]
        L += ["  set rwr on" if "rwr" in walkers else "  set rwr off"]
    else:
        L += ["  set fcr_mode crm", "  set fcr_range_nm 40"] if "fcr" in walkers else \
             ["  set fcr_mode off"]
        L += ["  set datalink on" if "dl" in walkers else "  set datalink off"]
        L += ["  set rwr on" if "rwr" in walkers else "  set rwr off"]
    if task:
        L += ["  set brief_master_arm arm", "  set task intercept"]
        L += ["  set store 3 r27r", "  set store 4 r27r"] if side else \
             ["  set store 3 aim120", "  set store 7 aim120"]
    L += [f"  wp {lat:.5f} {wlon:.5f} {alt} 450"]
    if objectives:
        L += ["  objective kill team " + ("friendly" if side else "hostile"), "  objective survive"]
    return "\n".join(L)


GROUND_TYPES = ["sa2", "sa3", "sa6", "sa8", "zsu23", "p18"]


def ground(name, i):
    lat, lon = grid(i, 0)
    return "\n".join([f"unit {name}", "  team hostile", f"  module {GROUND_TYPES[i % len(GROUND_TYPES)]}",
                      f"  spawn {lat:.5f} {lon:.5f} ground 0.0 0", "  set emcon free"])


def missile(name, i):
    lat, lon = grid(i, 0)
    return "\n".join([f"unit {name}", "  team hostile", "  module aim120",
                      f"  spawn {lat:.5f} {lon:.5f} 9000 90.0 900"])


def target(name, i):
    """The cheapest actor in the tree: FBGroundModule::Run is empty."""
    lat, lon = grid(i, 0)
    return "\n".join([f"unit {name}", "  team hostile",
                      f"  module {'target_hard' if i % 2 else 'target_soft'}",
                      f"  spawn {lat:.5f} {lon:.5f} ground 0.0 0"])


def mover(name, i):
    """A KINEMATIC air mover: `an26` has no FBFdm and no radar/RWR row — but its EYE is ungated."""
    lat, lon = grid(i, 0)
    return "\n".join([f"unit {name}", "  team hostile", "  module an26",
                      f"  spawn {lat:.5f} {lon:.5f} 6000 90.0 240",
                      f"  wp {lat:.5f} {lon + 4.0:.5f} 6000 240"])


# The owner's bound on the expensive class: a few dozen modules online at once, so the mixed profiles
# carry a FIXED base load of 24 (12 F-16 + 12 MiG-29, every box on) and vary only the cheap class.
MIX_MODULES = 24
CHEAP = {"target": target, "site": ground, "missile": missile, "mover": mover}


WALKERS = {
    "blind":   set(),
    "quiet":   {"eye"},
    "fcr":     {"eye", "fcr"},
    "fcrdl":   {"eye", "fcr", "dl"},
    "sensors": {"eye", "fcr", "dl", "rwr"},
    "combat":  {"eye", "fcr", "dl", "rwr"},
}

PROFILES = {
    # blind < quiet < fcr < fcrdl < sensors is a chain of SINGLE additions: each delta is one walker.
    "blind":   "N f16, every registry-walking sensor incl. the eye OFF — the linear floor",
    "quiet":   "blind + the eye (FBVisualSystem; it is not health-gated, so this is the real default)",
    "fcr":     "quiet + the fire-control radar (FrameS 4 s in CRM)",
    "fcrdl":   "fcr + the datalink (1 Hz net cycle)",
    "sensors": "fcrdl + the RWR (10 Hz, one full registry walk per module sensor tick)",
    "combat":  "sensors, but N/2 f16 vs N/2 mig29, armed, task intercept, kill+survive objectives",
    "ground":  "N ground positions (6 catalogue types) + 1 blind f16 — a unit with no airframe",
    "missile": "N aim120 airframes in the air — the small-airframe actor",
    "mix_none":    "the base load alone: 24 modules (12 f16 + 12 mig29), every box on",
    "mix_target":  "24 modules + N inert ground targets (FBGroundModule::Run is EMPTY)",
    "mix_site":    "24 modules + N air-defence positions (fire control + 2 radars + ESM + optics)",
    "mix_missile": "24 modules + N aim120 in the air",
    "mix_mover":   "24 modules + N kinematic air movers (an26: no FDM, no radar, no RWR — but an eye)",
    "bare_target": "N inert ground targets and NOTHING that looks — the cheap class's own floor",
    "bare_mover":  "N kinematic air movers and nothing that looks",
    "mix_theatre": "24 modules + N cheap actors, 40 % positions / 40 % targets / 20 % movers — the shape "
                   "a campaign theatre actually has",
}


def build(profile, n, timeout, alt=9000):
    head = [f"name scale-{profile}-{n}\ntimeout {timeout:g}\n\n"
            f"# GENERATED by tools/fb_scale_bench.py — a measurement cast, not a scenario.\n"
            f"# {PROFILES[profile]}"]
    body = []
    if profile in WALKERS:
        for i in range(n):
            side = 1 if (profile == "combat" and i >= n // 2) else 0
            j = i - n // 2 if side else i
            body.append(f16(f"u{i+1}", j, side, WALKERS[profile], profile == "combat",
                            profile == "combat", alt))
    elif profile == "ground":
        body.append(f16("probe", 0, 0, set(), False, False, alt))
        for i in range(n):
            body.append(ground(f"g{i+1}", i))
    elif profile == "missile":
        for i in range(n):
            body.append(missile(f"m{i+1}", i))
    elif profile.startswith("mix"):
        for i in range(MIX_MODULES):
            side = 1 if i >= MIX_MODULES // 2 else 0
            body.append(f16(f"m{i+1}", i - (MIX_MODULES // 2 if side else 0), side,
                            WALKERS["sensors"], False, False, alt))
        kind = profile.split("_", 1)[1]
        if kind == "theatre":
            for i in range(n):
                k = "site" if i % 5 < 2 else ("target" if i % 5 < 4 else "mover")
                body.append(CHEAP[k](f"c{i+1}", i))
        elif kind != "none":
            for i in range(n):
                body.append(CHEAP[kind](f"c{i+1}", i))
    elif profile.startswith("bare_"):
        for i in range(n):
            body.append(CHEAP[profile.split("_", 1)[1]](f"c{i+1}", i))
    return "\n\n".join(head + body) + "\n"


def write_mission(profile, n, timeout, tag=""):
    os.makedirs(SCALE_DIR, exist_ok=True)
    path = os.path.join(SCALE_DIR, f"{profile}{tag}-{n}.fbm")
    with open(path, "w") as f:
        f.write(build(profile, n, timeout))
    return path


def one_run(path, out, threads, keep=False):
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)
    t0 = time.monotonic()
    with open(os.path.join(out, "stdout.txt"), "w") as log:
        rc = subprocess.call(["/usr/bin/time", "-l", GYM, "--mission", path, "--out", out,
                              "--elev", "const", "--threads", str(threads)],
                             stdout=log, stderr=subprocess.STDOUT, cwd=SIM)
    proc_s = time.monotonic() - t0
    wall_s = sim_s = float("nan")
    rss = 0
    with open(os.path.join(out, "stdout.txt")) as f:
        for line in f:
            if " SUMMARY " in line:
                for tok in line.split():
                    if tok.startswith("wallS="):
                        wall_s = float(tok[6:])
                    elif tok.startswith("durationS="):
                        sim_s = float(tok[10:])
            elif "maximum resident set size" in line:
                rss = int(line.split()[0])
    names = os.listdir(out)
    actors = sum(1 for n in names if n.startswith("telemetry") and n.endswith(".csv"))
    fp = mission_fingerprint(out, rc)
    bytes_out = sum(os.path.getsize(os.path.join(out, n)) for n in names)
    if not keep:
        shutil.rmtree(out)
    return dict(rc=rc, wall_s=wall_s, sim_s=sim_s, proc_s=proc_s, fp=fp, bytes=bytes_out,
                actors=actors, rss=rss)


def sweep(profile, counts, threads_list, reps, timeout, outroot, keep, tag=""):
    w = csv.writer(sys.stdout)
    w.writerow(["profile", "n", "actors", "threads", "reps", "simS", "wallS_mean", "wallS_min",
                "wallS_max", "wallS_sd", "us_per_simS_per_actor", "speedup_vs_realtime", "procS_mean",
                "rssMB", "outMB", "rc", "fingerprints", "det"])
    for n in counts:
        path = write_mission(profile, n, timeout, tag)
        fps_all = {}
        for th in threads_list:
            rows = []
            for r in range(reps):
                out = os.path.join(outroot, f"{profile}-{n}-t{th}-r{r}")
                rows.append(one_run(path, out, th, keep))
            walls = [x["wall_s"] for x in rows]
            fps = sorted({x["fp"] for x in rows})
            fps_all[th] = fps
            sim_s = rows[0]["sim_s"]
            actors = rows[0]["actors"]
            mean = statistics.fmean(walls)
            sd = statistics.stdev(walls) if len(walls) > 1 else 0.0
            det = "SPLIT" if len(fps) > 1 else "ok"
            w.writerow([profile, n, actors, th, reps, f"{sim_s:g}", f"{mean:.4f}", f"{min(walls):.4f}",
                        f"{max(walls):.4f}", f"{sd:.4f}",
                        f"{mean / sim_s / max(actors, 1) * 1e6:.1f}", f"{sim_s/mean:.2f}",
                        f"{statistics.fmean(x['proc_s'] for x in rows):.3f}",
                        f"{max(x['rss'] for x in rows)/1048576:.0f}",
                        f"{rows[0]['bytes']/1048576:.1f}", rows[0]["rc"], len(fps), det])
            sys.stdout.flush()
        allfp = {f for v in fps_all.values() for f in v}
        print(f"# n={n} determinism across threads {threads_list}: "
              f"{'IDENTICAL' if len(allfp) == 1 else 'DIVERGED (' + str(len(allfp)) + ' fingerprints)'}"
              f" {sorted(allfp)[0][:16]}", file=sys.stderr)


def main():
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("gen"); g.add_argument("profile"); g.add_argument("n", type=int)
    g.add_argument("--timeout", type=float, default=30.0)
    s = sub.add_parser("sweep"); s.add_argument("profile"); s.add_argument("counts")
    s.add_argument("--threads", default="1,2,4"); s.add_argument("--reps", type=int, default=3)
    s.add_argument("--timeout", type=float, default=30.0)
    s.add_argument("--out", default="/tmp/fb-scale"); s.add_argument("--keep", action="store_true")
    for q in (g, s):
        q.add_argument("--spacing", type=float, default=0.0,
                       help="lattice pitch in degrees (default 0.02); 0.002 = the dense case")
    a = p.parse_args()
    tag = ""
    if a.spacing > 0.0:
        SPACING[0] = SPACING[1] = a.spacing
        tag = f"-s{a.spacing:g}"
    if a.cmd == "gen":
        print(write_mission(a.profile, a.n, a.timeout, tag))
    else:
        sweep(a.profile, [int(x) for x in a.counts.split(",")],
              [int(x) for x in a.threads.split(",")], a.reps, a.timeout, a.out, a.keep, tag)


if __name__ == "__main__":
    main()
