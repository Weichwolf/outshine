#!/usr/bin/env python3
"""FlightBox — THE RUNG LADDER, PLACED BY MEASUREMENT. doc/doctrine-evolution.md §4.2, round `E30`.

    tools/fb_rung_ladder.py --out build/e30/ladder --cell tools/duels-e30.txt --mission sat-10-duel-merge \
        --side red --units ma1,ma2,... --targets sw1,sw2,sw3,sw4 --per 2 --until 300 --emit new.fbm

WHY A TOOL AND NOT A HAND. The outcome class is a SUM (`X-13`), so a lever that changes WHICH objective
is met without changing HOW MANY is invisible to it. A cell therefore grades ONE continuous quantity
per unit through a MONOTONE RUNG LADDER, which turns that quantity into a count the class can see. Where
each rung goes is not a taste question — it is three measurements:

  THE SPECTRUM   the quantity's value under every lever of the graded side's own file, opponent
                 committed. Rungs go in its GAPS; a rung inside a cluster grades nothing.
  THE CHAOS      the same quantity over the +-3 m spawn grid, taken as the WORSE of the two
                 perturbations (the graded side's own and the opponent's). `E18` §1b: a rung is emitted
                 only where the half-gap is >= 10x this.
  THE OPPONENT BANDS   `E30` §3, and this is the part `E18` did not have. For every declared opponent
                 allele `o`, the interval [min_o - 10 c_o, max_o + 10 c_o] over that opponent's own S7
                 population — the graded side COMMITTED, its spawn over the same grid. A rung inside one
                 of those intervals passes the one-opponent rule and still flips S7 under that opponent:
                 [MESS, `E30`] `sat-10`'s blue class moved 4 of 8 under `red emcon-hi` on a rung with a
                 17x margin against the committed MiG.

WHAT IT MAY NOT DO. It never sees which lever WINS. Its objective is RESOLUTION — how many distinct
class sums the graded side's lever population produces — and the lever order is not an input to any
line below. Tuning a rig on the screen that judges it is what `§6` forbids; placing a rung so that a
measured quantity becomes visible at all is what a rig IS.

Stdlib plus numpy (the tracks are 10 Hz over 500 s and the spectra are min-reductions over pairs),
one dependency: build/fb-gym.
"""

import argparse
import concurrent.futures
import csv
import itertools
import json
import math
import os
import re
import shutil
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fb_campaign_arena as arena
import fb_campaign_coevolve as coevo
import fb_duel_arena as duel_arena

SIM_DIR = arena.SIM_DIR
kMargin = 10.0        # [SET] E18 §1b, unchanged: half-gap over chaos amplitude
M2LAT = 111320.0
TELEM_RE = re.compile(r'UNIT_RESULT unit=(\S+) .*telemetry=(\S+)')


def track(path):
    with open(path, newline="") as f:
        rows = list(csv.reader(f))
    i = {h: k for k, h in enumerate(rows[0])}
    return np.array([[float(r[i["t"]]), float(r[i["lat"]]), float(r[i["lon"]]), float(r[i["altM"]])]
                     for r in rows[1:]])


def run_one(job):
    gym, out, tag, text, calls, elev = job
    d = os.path.join(out, "run-" + tag)
    os.makedirs(d, exist_ok=True)
    p = os.path.join(d, "mission.fbm")
    with open(p, "w") as f:
        f.write(text)
    r = subprocess.run([gym, "--mission", p, "--out", d, "--threads", "1", "--elev", elev],
                       cwd=SIM_DIR, capture_output=True, text=True)
    log = os.path.join(d, "events.log")
    if not os.path.exists(log):
        sys.exit("%s: fb-gym exited %d and wrote no events.log\n%s" % (tag, r.returncode,
                                                                      r.stderr[-1500:]))
    paths = {}
    for line in open(log):
        m = TELEM_RE.search(line)
        if m:
            paths[m.group(1)] = m.group(2)
    td = os.path.join(out, "tracks")
    os.makedirs(td, exist_ok=True)
    n = 0
    for c in calls:
        q = paths.get(c, "")
        if q and os.path.exists(q):
            np.save(os.path.join(td, "%s__%s.npy" % (tag, c)), track(q))
            n += 1
    shutil.rmtree(d, ignore_errors=True)
    return tag, n


def aircraft_calls(text):
    lines = text.splitlines()
    starts = [i for i, l in enumerate(lines) if arena.UNIT_RE.match(l)]
    out = []
    for k, a in enumerate(starts):
        b = starts[k + 1] if k + 1 < len(starts) else len(lines)
        got = {"team": "", "module": ""}
        for l in lines[a:b]:
            t = l.split("#", 1)[0].split()
            if len(t) > 1 and t[0] in got:
                got[t[0]] = t[1]
        if got["module"] in ("f16", "mig29", "an26"):
            out.append(arena.UNIT_RE.match(lines[a]).group(1))
    return out


def min_range(td, tag, a, b, until):
    """The planar minimum range INSIDE THE DECLARED SPAN. A judge closes an `until <s>` window at <s>
    and stops accumulating, so a spectrum taken over the whole track grades a quantity the verdict never
    saw — [MESS, `E30`] that alone moved one unit's rung count by three."""
    pa = os.path.join(td, "%s__%s.npy" % (tag, a))
    pb = os.path.join(td, "%s__%s.npy" % (tag, b))
    if not (os.path.exists(pa) and os.path.exists(pb)):
        return None
    x, y = np.load(pa), np.load(pb)
    n = min(len(x), len(y))
    w = x[:n, 0] <= until
    if not w.any():
        return None
    k = math.cos(math.radians(x[0, 1]))
    return float(np.min(np.hypot((x[:n, 1] - y[:n, 1])[w] * M2LAT,
                                 (x[:n, 2] - y[:n, 2])[w] * M2LAT * k)))


def ladder(values, chaos, bands):
    """Rungs in the gaps of `values`, >= kMargin x `chaos` from either side and outside every band."""
    vals = sorted(set(round(v, 3) for v in values if v is not None))
    rungs, half = [], []
    for a, b in zip(vals, vals[1:]):
        lo, hi = a + kMargin * chaos, b - kMargin * chaos
        if hi <= lo:
            continue
        best, cand = -1.0, None
        for x in np.linspace(lo, hi, 201):
            if any(l <= x <= h for l, h in bands):
                continue
            d = min(x - a, b - x, min([min(abs(x - l), abs(x - h)) for l, h in bands] or [1e18]))
            if d > best:
                best, cand = d, float(x)
        if cand is not None:
            rungs.append(cand)
            half.append(min(cand - a, b - cand))
    return rungs, half


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", required=True)
    ap.add_argument("--gym", default=os.path.join(SIM_DIR, "build", "fb-gym"))
    ap.add_argument("--cells", default=os.path.join(SIM_DIR, "tools", "duels-e30.txt"))
    ap.add_argument("--mission", required=True)
    ap.add_argument("--side", required=True, choices=("blue", "red"))
    ap.add_argument("--units", required=True, help="the graded units, comma separated")
    ap.add_argument("--targets", required=True, help="the units a ladder may name")
    ap.add_argument("--per", type=int, default=1, help="how many ladders one target may carry")
    ap.add_argument("--until", type=float, required=True, help="the declared span of the rungs")
    ap.add_argument("--own-levers", default="")
    ap.add_argument("--opp-levers", default="")
    ap.add_argument("--offsets", default="", help="JSON {lever: M} the cell already awards this side")
    ap.add_argument("--emit", default="", help="write <mission> with these ladders to this path")
    ap.add_argument("--elev", default="const")
    ap.add_argument("--jobs", type=int, default=6)
    args = ap.parse_args()

    duels = {d.mission: d for d in coevo.load_duels(args.cells)}
    duel = duels[args.mission]
    blue_file = args.own_levers if args.side == "blue" else args.opp_levers
    red_file = args.opp_levers if args.side == "blue" else args.own_levers
    own = duel_arena.genomes(blue_file or os.path.join(SIM_DIR, "tools", "levers-campaign-g5.txt")
                             if args.side == "blue" else
                             red_file or os.path.join(SIM_DIR, "tools", "levers-red-mig29.txt"))
    opp = duel_arena.genomes(os.path.join(SIM_DIR, "tools", "levers-red-mig29.txt")
                             if args.side == "blue" else
                             os.path.join(SIM_DIR, "tools", "levers-campaign-g5.txt"))
    text = open(duel.path).read()
    calls = aircraft_calls(text)
    gteam, gmod = duel.blue if args.side == "blue" else duel.red
    os.makedirs(args.out, exist_ok=True)

    def pair(g, o):
        return (g, o) if args.side == "blue" else (o, g)

    jobs = []
    for g in [coevo.COMMITTED] + own:                       # THE SPECTRUM
        t, _, _ = coevo.splice_both(text, duel, *pair(g, coevo.COMMITTED))
        jobs.append((args.gym, args.out, "lv-" + g.name, t, calls, args.elev))
    for who, team, mod in (("own", gteam, gmod),
                           ("foe", *(duel.red if args.side == "blue" else duel.blue))):
        for i, m in enumerate(arena.kChaosSteps):           # THE CHAOS, both perturbations
            t, _, _ = coevo.splice_both(coevo.perturbed(text, team, mod, m), duel,
                                        coevo.COMMITTED, coevo.COMMITTED)
            jobs.append((args.gym, args.out, "ch%s-%d" % (who, i), t, calls, args.elev))
    for o in [coevo.COMMITTED] + opp:                       # THE OPPONENT BANDS
        for i, m in enumerate([None] + arena.kChaosSteps):
            base = text if m is None else coevo.perturbed(text, gteam, gmod, m)
            t, _, _ = coevo.splice_both(base, duel, *pair(coevo.COMMITTED, o))
            jobs.append((args.gym, args.out, "op-%s-%d" % (o.name, i), t, calls, args.elev))
    print("%d runs: %d spectrum, %d chaos, %d opponent-band"
          % (len(jobs), len(own) + 1, 2 * len(arena.kChaosSteps),
             (len(opp) + 1) * (1 + len(arena.kChaosSteps))))
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for tag, n in ex.map(run_one, jobs):
            if not n:
                sys.exit("%s produced no track" % tag)

    td = os.path.join(args.out, "tracks")
    units, targets = args.units.split(","), args.targets.split(",")
    pairs = [(u, t) for u in units for t in targets if u != t]
    levs = ["lv-" + g.name for g in [coevo.COMMITTED] + own]
    lad = {}
    for p in pairs:
        sp = [min_range(td, t, p[0], p[1], args.until) for t in levs]
        if any(v is None for v in sp):
            continue
        chaos = 0.0
        for who in ("own", "foe"):
            v = [min_range(td, "ch%s-%d" % (who, i), p[0], p[1], args.until)
                 for i in range(len(arena.kChaosSteps))]
            v = [x for x in v if x is not None]
            if v:
                chaos = max(chaos, max(v) - min(v))
        bands = []
        for o in [coevo.COMMITTED] + opp:
            v = [min_range(td, "op-%s-%d" % (o.name, i), p[0], p[1], args.until)
                 for i in range(1 + len(arena.kChaosSteps))]
            v = [x for x in v if x is not None]
            if len(v) > 1:
                c = max(v) - min(v)
                bands.append((min(v) - kMargin * c, max(v) + kMargin * c))
        r, hg = ladder(sp, chaos, bands)
        if r:
            lad[p] = (r, hg, [sum(1 for x in r if v <= x) for v in sp], chaos, len(bands))

    off = json.load(open(args.offsets)) if args.offsets else {}
    offs = [off.get(g.name, 0) for g in [coevo.COMMITTED] + own]
    best = None
    for combo in itertools.product(targets, repeat=len(units)):
        if any(combo.count(t) > args.per for t in targets):
            continue
        ps = [(u, combo[i]) for i, u in enumerate(units)]
        if any(p not in lad for p in ps):
            continue
        sums = [offs[i] + sum(lad[p][2][i] for p in ps) for i in range(len(levs))]
        movers = sum(1 for i in range(1, len(levs)) if sums[i] != sums[0])
        score = (movers, len(set(sums)), min(min(lad[p][1]) / lad[p][3] for p in ps))
        if best is None or score > best[0]:
            best = (score, ps, sums)
    if best is None:
        sys.exit("no admissible assignment: every candidate ladder is empty under the opponent bands")
    score, ps, sums = best
    print("\nmovers %d of %d   distinct sums %d   worst rung margin %.1fx"
          % (score[0], len(levs) - 1, score[1], score[2]))
    for p in ps:
        r, hg, cnt, c, nb = lad[p]
        print("  %-6s -> %-6s chaos %6.2f m  min half-gap %8.1f m (%5.1fx)  %d bands  %d rungs: %s"
              % (p[0], p[1], c, min(hg), min(hg) / c, nb, len(r), " ".join("%.0f" % x for x in r)))
    out = {"assign": [list(p) for p in ps], "levers": levs, "sums": sums,
           "rungs": {"%s>%s" % p: lad[p][0] for p in ps},
           "chaos": {"%s>%s" % p: lad[p][3] for p in ps},
           "halfgap": {"%s>%s" % p: min(lad[p][1]) for p in ps}}
    with open(os.path.join(args.out, "assignment.json"), "w") as f:
        json.dump(out, f, indent=1)

    if args.emit:
        rungs = {tuple(p): out["rungs"]["%s>%s" % tuple(p)] for p in out["assign"]}
        mine = {p[0] for p in rungs}
        body, cur = [], ""
        for raw in open(duel.path):
            m = arena.UNIT_RE.match(raw)
            if m:
                cur = m.group(1)
            t = raw.split("#", 1)[0].split()
            if cur in mine and t[:2] == ["objective", "identify"]:
                continue
            if cur in mine and t[:2] == ["objective", "survive"]:
                tgt = [p for p in rungs if p[0] == cur][0]
                for r in rungs[tgt]:
                    body.append("  objective identify unit %s range %d hold 0.1 until %g\n"
                                % (tgt[1], round(r), args.until))
            body.append(raw)
        with open(args.emit, "w") as f:
            f.write("".join(body))
        print("wrote %s" % args.emit)
    return 0


if __name__ == "__main__":
    sys.exit(main())
