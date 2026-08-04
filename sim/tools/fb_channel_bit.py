#!/usr/bin/env python3
"""FlightBox — WHY THE CHANNEL BIT WINS. doc/doctrine-evolution.md `E-34`, round `E31`.

    tools/fb_channel_bit.py --out build/e31 --cells tools/duels-e30.txt \
        --blue-levers tools/levers-campaign-g5.txt

`E30` measured that blue's search wins generation 0 with `dl=off` and read that as a property of the
landscape. A landscape is not a mechanism. This file holds EVERY other gene fixed and flies the channel
bit alone over the whole declared lever population, on both sides of the same run, and asks three
questions the co-evolution's scalar cannot:

  * WHICH LEVEL of the key decides — V, M or C, and inside C which currency. A shift that lives
    entirely in one column is not the doctrine it looks like (`E18`'s `C_aim`).
  * WHAT the channel BUYS AND COSTS on the wire — the flight channels (`flt_*`), the engagement
    channels (`eng_*`) and the EMISSION trace, which is this file's own column set.
  * WHETHER the emission trace is honest. `X-20`: an F-16 whose radar stops radiating keeps its scan
    grid frozen, so the catch-up guard in `FBRadarSystem::Run` replays every missed frame in the tick
    the set comes back and hands the jet a firm track for free. That defect is reachable ONLY through
    EMCON, EMCON is reachable ONLY with a picture, and on these cells a picture is reachable only over
    the datalink — so it is a subsidy paid to `dl=on` alone, and it has to be priced before the bit's
    verdict means anything.

The pairing is `fb_fitness.pair_points` between the two channel states of the SAME lever on the SAME
cell against the SAME opponent: like against like, one variable.

Stdlib only, no build target, one dependency: build/fb-gym.
"""

import argparse
import collections
import concurrent.futures
import csv
import hashlib
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fb_campaign_arena as arena
import fb_campaign_coevolve as coevo
import fb_evolve as evo
import fb_fitness as fit

SIM_DIR = arena.SIM_DIR
COMMITTED = coevo.COMMITTED

# The emission trace, per blue unit and then MAXed over the flight the same way `arena.channels` maxes
# its own. `rq_max` is `X-20`'s own number: firm contacts present in the FIRST tick after the set
# starts radiating again. A set whose grid was resynchronised can only ever show 0 there.
EMI_COLS = ("emcon_spells", "emcon_s", "rq_max", "rq_events", "ttf_mean")


def emission_trace(path):
    """One unit's EMCON history out of `fcr_on`/`fcr_contacts`. Reads the columns the F-16 and the
    MiG-29 both publish, so the same instrument answers for either airframe."""
    with open(path, newline="") as f:
        rows = list(csv.reader(f))
    idx = {h: i for i, h in enumerate(rows[0])}
    if "fcr_on" not in idx or "fcr_contacts" not in idx:
        return None
    ti, oi, ci = idx["t"], idx["fcr_on"], idx["fcr_contacts"]
    spells, quiet_s, rq_max, rq_events, ttf = 0, 0.0, 0.0, 0, []
    prev_on, quiet_from, dt = None, None, 0.0
    pending = None
    for r in rows[1:]:
        try:
            t, on, n = float(r[ti]), float(r[oi]) > 0.5, float(r[ci])
        except (ValueError, IndexError):
            continue
        if prev_on is not None:
            dt = t - prev_t
            if not prev_on:
                quiet_s += dt
            if prev_on and not on:
                spells += 1
                quiet_from = t
            if not prev_on and on:
                pending = t              # the tick the antenna comes back
        if pending is not None and t >= pending:
            if t == pending:
                rq_max = max(rq_max, n)  # X-20: contacts in the RETURN tick itself
                if n > 0:
                    rq_events += 1
                    ttf.append(0.0)
                    pending = None
            elif n > 0:
                ttf.append(t - pending)
                pending = None
        prev_on, prev_t = on, t
    return {"emcon_spells": float(spells), "emcon_s": quiet_s if spells else 0.0, "rq_max": rq_max,
            "rq_events": float(rq_events),
            "ttf_mean": (sum(ttf) / len(ttf)) if ttf else -1.0}


def side_emission(outdir, calls):
    out = {c: 0.0 for c in EMI_COLS}
    out["ttf_mean"] = -1.0
    seen = []
    for c in calls:
        p = os.path.join(outdir, "telemetry_%s.csv" % c)
        if not os.path.exists(p):
            continue
        e = emission_trace(p)
        if e is None:
            continue
        seen.append(e)
    for k in ("emcon_spells", "emcon_s", "rq_max", "rq_events"):
        out[k] = max([e[k] for e in seen], default=0.0)
    good = [e["ttf_mean"] for e in seen if e["ttf_mean"] >= 0.0]
    out["ttf_mean"] = sum(good) / len(good) if good else -1.0
    return out


NAMED_RE = None


def named_map(text):
    """Which unit each graded aircraft's `identify` ladder NAMES. The rungs are what level M is made of
    on these cells, so the question "did the doctrine go where it was sent" has to be asked against the
    NAME and not against the nearest bandit — which is the device `sat-10`'s own head claims stops
    "fly at whatever is nearest" from answering every rung."""
    import re
    global NAMED_RE
    if NAMED_RE is None:
        NAMED_RE = re.compile(r"^\s*objective\s+identify\s+unit\s+(\S+)\s")
    out, cur = {}, None
    for raw in text.splitlines():
        line = raw.split("#", 1)[0]
        t = line.split()
        if t[:1] == ["unit"] and len(t) > 1:
            cur = t[1]
        m = NAMED_RE.match(line)
        if m and cur:
            out.setdefault(cur, set()).add(m.group(1))
    return out


def min_ranges(outdir, mine, theirs):
    """Planar min-range over the run for every (my aircraft, opposing aircraft) pair. One pass over the
    two tracks; the runs are same-length by construction on these cells (X-7 is screened)."""
    import csv as _csv
    import math as _math
    track = {}
    for c in set(mine) | set(theirs):
        p = os.path.join(outdir, "telemetry_%s.csv" % c)
        if not os.path.exists(p):
            continue
        with open(p, newline="") as f:
            rows = list(_csv.DictReader(f))
        if rows and "lat" in rows[0]:
            track[c] = [(float(r["lat"]), float(r["lon"])) for r in rows]
    out = {}
    for a in mine:
        for b in theirs:
            if a not in track or b not in track:
                continue
            n = min(len(track[a]), len(track[b]))
            best = 1e12
            for i in range(n):
                la, lo = track[a][i]
                lb, lob = track[b][i]
                dx = (lo - lob) * _math.cos(_math.radians(0.5 * (la + lb))) * 111320.0
                dy = (la - lb) * 111320.0
                d = dx * dx + dy * dy
                if d < best:
                    best = d
            out[(a, b)] = _math.sqrt(best)
    return out


OBJ_RE = None


def objective_kinds(outdir, calls):
    """M is a SUM (X-13's class), so a shift inside it is only a doctrine if one can say WHICH rungs
    moved. The judge's own `mission OBJECTIVE` lines, tallied by the objective's first word."""
    import re
    global OBJ_RE
    if OBJ_RE is None:
        OBJ_RE = re.compile(r'mission OBJECTIVE unit=(\S+) kind="([^"]+)" state=(\S+)')
    out = collections.Counter()
    mine = set(calls)
    for line in open(os.path.join(outdir, "events.log")):
        m = OBJ_RE.search(line)
        if not m or m.group(1) not in mine:
            continue
        kind = m.group(2).split()[0]
        out["obj_%s_n" % kind] += 1
        if m.group(3) == "met":
            out["obj_%s_met" % kind] += 1
    return dict(out)


def telem_sha(outdir, calls):
    """The BIT level. One digest over the side's telemetry files in call order — `E19`'s reading, so
    that "changed nothing" and "changed nothing the class can see" stay two different statements."""
    h = hashlib.sha256()
    for c in sorted(calls):
        p = os.path.join(outdir, "telemetry_%s.csv" % c)
        if not os.path.exists(p):
            continue
        with open(p, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
    return h.hexdigest()[:16]


def run_pair(job):
    gym, outroot, tag, duel, text, threads, elev, bcalls, rcalls, named = job
    d = os.path.join(outroot, tag)
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, "mission.fbm")
    with open(path, "w") as f:
        f.write(text)
    cmd = [gym, "--mission", path, "--out", d, "--threads", str(threads), "--elev", elev]
    if duel.time:
        cmd += ["--campaign-time", duel.time]
    import subprocess
    r = subprocess.run(cmd, cwd=SIM_DIR, capture_output=True, text=True)
    if not os.path.exists(os.path.join(d, "events.log")):
        sys.exit("%s: fb-gym exited %d and wrote no events.log" % (tag, r.returncode))
    bk, rk, dur = coevo.read_both(d, bcalls, rcalls)
    if bk is None:
        sys.exit("%s: no side key" % tag)
    rec = {"bkey": bk, "rkey": rk, "durationS": dur, "exit": r.returncode}
    rec.update(arena.channels(d, [os.path.join(d, "telemetry_%s.csv" % c) for c in bcalls
                                  if os.path.exists(os.path.join(d, "telemetry_%s.csv" % c))],
                              bcalls))
    rec.update(side_emission(d, bcalls))
    rec.update(objective_kinds(d, bcalls))
    mr = min_ranges(d, bcalls, rcalls)
    pick = [v for (a, b), v in mr.items() if b in named.get(a, ())]
    rec["mr_named"] = sum(pick) / len(pick) if pick else -1.0
    rec["mr_all"] = sum(mr.values()) / len(mr) if mr else -1.0
    rec["sha_blue"] = telem_sha(d, bcalls)
    rec["sha_red"] = telem_sha(d, rcalls)
    import shutil
    shutil.rmtree(d, ignore_errors=True)
    return tag, rec


def sign_test(wins, losses):
    """Two-sided sign test, ties dropped — the same reading `E18` §4 uses, computed exactly."""
    n = wins + losses
    if n == 0:
        return 1.0
    k = min(wins, losses)
    p = sum(math.comb(n, i) for i in range(0, k + 1)) / float(2 ** n)
    return min(1.0, 2.0 * p)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", required=True)
    ap.add_argument("--gym", default=os.path.join(SIM_DIR, "build", "fb-gym"))
    ap.add_argument("--cells", default=os.path.join(SIM_DIR, "tools", "duels-e30.txt"))
    ap.add_argument("--blue-levers", default=os.path.join(SIM_DIR, "tools",
                                                          "levers-campaign-g5.txt"))
    ap.add_argument("--csv", default="")
    ap.add_argument("--elev", default="const")
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--threads", type=int, default=1)
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    if not arena.tree_clean():
        sys.exit("sim/missions or sim/assets is dirty BEFORE the run")
    duels = coevo.load_duels(args.cells)
    # The channel alleles of the lever file are DROPPED: they are the variable, not a background.
    base = [evo.Genome(v.name, v.params, "", v.sort)
            for v in arena.load_levers(args.blue_levers) if not v.dl]
    pop = [COMMITTED] + base
    print("channel-bit isolation: %d cells x %d genomes x 2 channel states = %d runs"
          % (len(duels), len(pop), 2 * len(duels) * len(pop)))
    print("simulator %s" % arena.gym_identity(args.gym))

    jobs, meta = [], []
    for d in duels:
        text = open(d.path).read()
        named = named_map(text)
        for g in pop:
            for dl in ("off", "on"):
                b = evo.Genome(g.name, g.params, dl, g.sort)
                t, bc, rc = coevo.splice_both(text, d, b, COMMITTED)
                tag = "cb-%s-%s-%s" % (d.mission, g.name, dl)
                jobs.append((args.gym, args.out, tag, d, t, args.threads, args.elev, bc, rc, named))
                meta.append((tag, d, g, dl, bc))
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        rec = dict(ex.map(run_pair, jobs))

    rows = []
    for tag, d, g, dl, bc in meta:
        r = dict(rec[tag])
        r["cell"], r["lever"], r["dl"] = d.mission, g.name, dl
        rows.append(r)
    by = {(r["cell"], r["lever"], r["dl"]): r for r in rows}

    print("\n%s\nPER CELL — dl=off against dl=on, same lever, same opponent, one variable"
          % ("=" * 118))
    tot = collections.Counter()
    levels = collections.Counter()
    for d in duels:
        w = l = t = 0
        print("\n%s" % d.mission)
        print("  %-13s %-22s %-22s %-7s %s"
              % ("lever", "dl=off  (V,M,Cair,Caim)", "dl=on   (V,M,Cair,Caim)", "pair", "level"))
        for g in pop:
            a, b = by[(d.mission, g.name, "off")], by[(d.mission, g.name, "on")]
            pa, _ = fit.pair_points(a["bkey"], b["bkey"])
            lvl = ("V" if a["bkey"][0] != b["bkey"][0] else
                   "M" if a["bkey"][1] != b["bkey"][1] else
                   "C" if a["bkey"][2] != b["bkey"][2] else "=")
            if lvl == "C":
                ca, cb = a["bkey"][2], b["bkey"][2]
                if ca == fit.GATE or cb == fit.GATE:
                    lvl = "C:gate"
                else:
                    lvl = "C:" + ("air" if abs(ca[0] - cb[0]) > 1e-9 else "") + \
                          ("aim" if abs(ca[1] - cb[1]) > 1e-9 else "")
            levels[lvl] += 1
            if pa > 0.5:
                w += 1
            elif pa < 0.5:
                l += 1
            else:
                t += 1

            def fk(k):
                c = k[2]
                return "%d %d %s" % (k[0], k[1], "GATE" if c == fit.GATE
                                    else "%.1f %.1f" % (c[0], c[1]))
            print("  %-13s %-22s %-22s %-7s %s"
                  % (g.name, fk(a["bkey"]), fk(b["bkey"]),
                     "off" if pa > 0.5 else "on" if pa < 0.5 else "tie", lvl))
        print("  -> dl=off %d, dl=on %d, tie %d" % (w, l, t))
        tot["off"] += w
        tot["on"] += l
        tot["tie"] += t

    print("\n%s\nTHE VERDICT" % ("=" * 118))
    print("dl=off %d, dl=on %d, tie %d over %d pairings; sign test p = %.4f"
          % (tot["off"], tot["on"], tot["tie"], sum(tot.values()),
             sign_test(tot["off"], tot["on"])))
    print("the level that decides: %s"
          % ", ".join("%s %d" % kv for kv in sorted(levels.items())))

    print("\n%s\nTHE WIRE — mean over cells and levers, per channel state" % ("=" * 118))
    cols = ["flt_mates", "flt_src", "flt_assign", "flt_cover_s", "eng_shots", "sort_assign",
            "emcon_spells", "emcon_s", "rq_max", "rq_events", "ttf_mean"]
    print("  %-14s %12s %12s" % ("channel", "dl=off", "dl=on"))
    for c in cols:
        m = {}
        for dl in ("off", "on"):
            v = [r[c] for r in rows if r["dl"] == dl and c in r]
            m[dl] = sum(v) / len(v) if v else float("nan")
        print("  %-14s %12.3f %12.3f" % (c, m["off"], m["on"]))

    print("\n%s\nWHICH RUNGS M IS MADE OF — the judge's own lines, summed over cells and levers"
          % ("=" * 118))
    kinds = sorted({k[4:-2] for r in rows for k in r if k.startswith("obj_") and k.endswith("_n")})
    print("  %-12s %14s %14s %10s" % ("objective", "dl=off met/n", "dl=on met/n", "delta"))
    for k in kinds:
        s = {}
        for dl in ("off", "on"):
            sel = [r for r in rows if r["dl"] == dl]
            s[dl] = (sum(r.get("obj_%s_met" % k, 0) for r in sel),
                     sum(r.get("obj_%s_n" % k, 0) for r in sel))
        print("  %-12s %8d /%4d %8d /%4d %10d"
              % (k, s["off"][0], s["off"][1], s["on"][0], s["on"][1], s["off"][0] - s["on"][0]))

    print("\n%s\nDID THE LADDER GRADE WHAT IT SAYS IT GRADES — planar min-range, metres" % ("=" * 118))
    print("  %-28s %12s %12s %12s %12s" % ("cell", "named off", "named on", "all off", "all on"))
    for d in duels:
        v = {}
        for dl in ("off", "on"):
            sel = [r for r in rows if r["dl"] == dl and r["cell"] == d.mission]
            for c in ("mr_named", "mr_all"):
                good = [r[c] for r in sel if r[c] >= 0]
                v[(dl, c)] = sum(good) / len(good) if good else float("nan")
        print("  %-28s %12.0f %12.0f %12.0f %12.0f"
              % (d.mission, v[("off", "mr_named")], v[("on", "mr_named")],
                 v[("off", "mr_all")], v[("on", "mr_all")]))
    print("  The ladder pays for the NAMED column. If the ALL column moves by the same amount, the "
          "name carries no information and the rung is a proximity meter.")

    same = sum(1 for d in duels for g in pop
               if by[(d.mission, g.name, "off")]["sha_blue"]
               == by[(d.mission, g.name, "on")]["sha_blue"])
    print("\nBIT LEVEL: blue telemetry identical across the channel bit in %d of %d pairs"
          % (same, len(duels) * len(pop)))

    rq = [r for r in rows if r["dl"] == "on" and r["rq_events"] > 0]
    print("X-20 REACH: %d of %d dl=on runs show a firm contact in the RETURN tick "
          "(max %g contacts); dl=off runs with any EMCON spell: %d of %d"
          % (len(rq), len([r for r in rows if r["dl"] == "on"]),
             max([r["rq_max"] for r in rows if r["dl"] == "on"], default=0),
             len([r for r in rows if r["dl"] == "off" and r["emcon_spells"] > 0]),
             len([r for r in rows if r["dl"] == "off"])))

    path = args.csv or os.path.join(args.out, "channel-bit.csv")
    with open(path, "w", newline="") as f:
        cw = csv.writer(f)
        obj = sorted({k for r in rows for k in r if k.startswith("obj_")})
        head = ["cell", "lever", "dl", "V", "M", "C_air", "C_aim", "sha_blue", "sha_red"] + \
               list(arena.CHAN_COLS) + ["releases", "sort_assign"] + list(EMI_COLS) + obj + \
               ["mr_named", "mr_all", "exit", "durationS"]
        cw.writerow(head)
        for r in rows:
            k = r["bkey"]
            c = (0.0, 0.0) if k[2] == fit.GATE else k[2]
            cw.writerow([r["cell"], r["lever"], r["dl"], k[0], k[1], c[0], c[1],
                         r["sha_blue"], r["sha_red"]] +
                        [r.get(x, "") for x in arena.CHAN_COLS] +
                        [r.get("releases", ""), r.get("sort_assign", "")] +
                        [r.get(x, "") for x in EMI_COLS] + [r.get(x, 0) for x in obj] +
                        [r["mr_named"], r["mr_all"], r["exit"], r["durationS"]])
    print("wrote %s" % path)
    if not arena.tree_clean():
        print("VOID: sim/missions or sim/assets moved during the run")
        return 1
    print("mission and model tree clean after the run")
    return 0


if __name__ == "__main__":
    sys.exit(main())
