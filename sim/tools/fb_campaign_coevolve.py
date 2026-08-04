#!/usr/bin/env python3
"""FlightBox — TWO-SIDED DOCTRINE CO-EVOLUTION. doc/doctrine-evolution.md §3, round `E19`.

    tools/fb_campaign_coevolve.py --out build/e19 --cells tools/duels-sat.txt --probe blue
    tools/fb_campaign_coevolve.py --out build/e19 --cells tools/duels-sat.txt --generations 4

WHY THIS FILE EXISTS BESIDE `fb_campaign_evolve.py`, AND IT IS THE WHOLE POINT. That file says so in
its own docstring: *"THE OPPONENT IS COMMITTED MISSION TEXT. A campaign rung's other side is a script;
it cannot answer. So this is not a co-evolution and the Red Queen is excluded by construction."* It
carries `--archive` and calls `evo.archive_sample`, but the archive it keeps is an archive of ONE
population measured against a WORLD THAT NEVER MOVES — §3's failure mode cannot occur there and the
three instruments of §3.6 have nothing to detect. Every result E13…E18 published is one side trimmed
against a fixed opponent.

WHAT IS DIFFERENT HERE, in one sentence: BOTH sides carry a genome, one run produces BOTH side keys,
and the archive is per side.

  * THE SIDES ARE DECLARED, NEVER DERIVED — a duel line is `<mission> <blue team> <blue module>
    <red team> <red module>`, the same rule `fb_campaign_arena` states for its one side.
  * ONE RUN, TWO KEYS. `fb_fitness.side_key` is evaluated twice off the same telemetry, each side with
    the other as its `FlightView`. Nothing is compared ACROSS the two keys — an F-16 flight and a
    MiG-29 flight declare different objectives on different airframes, so a cross-side comparison
    measures the seat and not the doctrine (D6, and `fb_fitness.match_points` says it at length).
  * THE FITNESS IS LIKE AGAINST LIKE, CONDITIONED ON THE OPPONENT. Blue genome b's score against red
    opponent r is its mean `pair_points` against the OTHER BLUE genomes on the same r and the same
    cell. That vector over r is exactly the CIAO row §3.4 B admits on and §3.6 b reads.
  * THE CHAMPION IS PICKED IN THE FROZEN WORLD (E19's rule, inherited verbatim): against the COMMITTED
    opposing side, ranked inside a FROZEN reference field, never by the co-evolutionary round robin.

Stdlib only, no build target, one dependency: build/fb-gym.
"""

import argparse
import collections
import concurrent.futures
import csv
import math
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fb_campaign_arena as arena
import fb_evolve as evo
import fb_fitness as fit
import fb_tournament as tour

SIM_DIR = arena.SIM_DIR

# RED'S WHOLE GENOME, and it is ONE gene wide. Measured in round `E19` at the TRAJECTORY level
# (`build/e19/probe-red.log`, and the bit-level table in this file's State block): of the nine genes
# of §2/§7, eight are bit-identical on the MiG-29 side on all cells, and only the sort contract
# reaches it. Its alphabet is what `FBMig29Module::Set("brief_sort")` accepts; `left` is the committed
# brief on every rig, so the empty allele IS `left` and admitting both would put a silent identity map
# in the population (X-14's class). `dl=` is omitted for the same reason: the MiG-29 has no `datalink`
# key at all and `arena.splice_mission` injects the channel bit only for `f16`.
RED_SORT_ALLELES = [("off", ""), ("off", "near"), ("off", "right"), ("off", "far"), ("off", "none")]


class Duel:
    """One arena cell with BOTH sides declared. The clock is the cell's, exactly as in
    `fb_campaign_arena.Cell` — a rig that took the campaign default on one side and not the other
    would grade two different missions."""

    def __init__(self, mission, bteam, bmod, rteam, rmod):
        self.mission = mission
        self.blue = (bteam, bmod)
        self.red = (rteam, rmod)
        self.path = os.path.join(arena.MISSION_DIR, mission + ".fbm")
        self.time = arena.campaign_time(mission.split("-")[0])

    @property
    def name(self):
        return self.mission


def load_duels(path):
    out = []
    for lineno, raw in enumerate(open(path), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        t = line.split()
        if len(t) != 5:
            sys.exit("%s:%d: want '<mission> <blue team> <blue module> <red team> <red module>'"
                     % (path, lineno))
        d = Duel(*t)
        if not os.path.exists(d.path):
            sys.exit("%s:%d: no such mission %s" % (path, lineno, d.path))
        out.append(d)
    return out


COMMITTED = evo.Genome("committed", {}, dl="", sort="")   # splices nothing: the file as it stands


def splice_both(text, duel, blue, red):
    t, bcalls = arena.splice_mission(text, duel.blue[0], duel.blue[1], blue.variant(duel.blue[1]))
    t, rcalls = arena.splice_mission(t, duel.red[0], duel.red[1], red.variant(duel.red[1]))
    if not bcalls:
        sys.exit("%s: no unit with team=%s module=%s" % (duel.mission, duel.blue[0], duel.blue[1]))
    if not rcalls:
        sys.exit("%s: no unit with team=%s module=%s" % (duel.mission, duel.red[0], duel.red[1]))
    return t, bcalls, rcalls


TELEM_RE = re.compile(r'UNIT_RESULT unit=(\S+) .*telemetry=(\S+)')


def read_both(outdir, bcalls, rcalls):
    """Both side keys off ONE run. `fb_fitness.read_events` is called ONCE with every aircraft in the
    map, because it is the only reader of the objective vector and calling it twice would append each
    unit's objectives a second time."""
    log = os.path.join(outdir, "events.log")
    paths, duration = {}, 0.0
    for line in open(log):
        m = TELEM_RE.search(line)
        if m:
            paths[m.group(1)] = m.group(2)
        if "mission SUMMARY" in line:
            d = re.search(r"durationS=([0-9.]+)", line)
            if d:
                duration = float(d.group(1))
    blue = [fit.read_side(paths[c], c) for c in bcalls if c in paths and os.path.exists(paths[c])]
    red = [fit.read_side(paths[c], c) for c in rcalls if c in paths and os.path.exists(paths[c])]
    if not blue or not red:
        return None, None, duration
    fit.read_events(log, {s.name: s for s in blue + red})
    bk, _ = fit.side_key(blue, fit.FlightView(red), duration)
    rk, _ = fit.side_key(red, fit.FlightView(blue), duration)
    return bk, rk, duration


def run_one(job):
    gym, outroot, tag, duel, text, threads, elev, bcalls, rcalls, keep = job
    d = os.path.join(outroot, tag)
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, "mission.fbm")
    with open(path, "w") as f:
        f.write(text)
    cmd = [gym, "--mission", path, "--out", d, "--threads", str(threads), "--elev", elev]
    if duel.time:
        cmd += ["--campaign-time", duel.time]
    r = subprocess.run(cmd, cwd=SIM_DIR, capture_output=True, text=True)
    # A boundary check, not a belt: a run that produced no log at all is a failure of the RUNNER, and
    # reading it as an empty result would put a phantom key into the cache that nothing could explain.
    if not os.path.exists(os.path.join(d, "events.log")):
        sys.exit("%s: fb-gym exited %d and wrote no events.log; dir holds %r\n%s"
                 % (tag, r.returncode, sorted(os.listdir(d))[:8] if os.path.isdir(d) else "GONE",
                    r.stderr[-2000:]))
    bk, rk, dur = read_both(d, bcalls, rcalls)
    if bk is None:
        named = {c for c in bcalls + rcalls
                 if any(("unit=%s " % c) in l for l in open(os.path.join(d, "events.log")))}
        sys.exit("%s: exit %d, %d of %d units named in events.log, dir holds %d file(s) — "
                 "blue %s red %s"
                 % (tag, r.returncode, len(named), len(bcalls) + len(rcalls), len(os.listdir(d)),
                    ",".join(bcalls), ",".join(rcalls)))
    chan = arena.channels(d, [], bcalls)
    chan["exit"] = r.returncode
    chan["durationS"] = dur
    if not keep:
        shutil.rmtree(d, ignore_errors=True)
    return tag, bk, rk, chan


class Pool:
    """Every (blue line, red line, cell) ever flown. The environment is deterministic and the mission
    text is a pure function of the two genomes, so a triple flown once is known forever — which is
    what makes a two-sided cross product affordable at all."""

    def __init__(self, gym, out, duels, jobs, threads, elev, keep, sink=None):
        self.gym, self.out, self.duels = gym, out, duels
        self.jobs, self.threads, self.elev, self.keep = jobs, threads, elev, keep
        self.keys, self.runs, self.sink = {}, 0, sink
        self.text = {d.mission: open(d.path).read() for d in duels}

    def _k(self, b, r, d):
        return (b.line(), r.line(), d.mission)

    def demand(self, pairs, tagpfx=""):
        """`pairs` is an iterable of (blue genome, red genome). Every duel is flown for each."""
        jobs, meta = [], []
        seen = set()
        for b, r in pairs:
            for d in self.duels:
                k = self._k(b, r, d)
                if k in self.keys or k in seen:
                    continue
                seen.add(k)
                text, bc, rc = splice_both(self.text[d.mission], d, b, r)
                tag = "%s%s-%s-%s" % (tagpfx, d.mission, b.name, r.name)
                jobs.append((self.gym, self.out, tag, d, text, self.threads, self.elev, bc, rc,
                             self.keep))
                meta.append((tag, k, b, r, d))
        if not jobs:
            return
        # Two jobs sharing a run directory would have one of them delete the other's telemetry, and the
        # symptom is a missing file rather than a wrong number. The tag is built from GENOME NAMES and
        # the cache key from genome LINES, so the two can disagree; this is where that is caught.
        tags = [m[0] for m in meta]
        if len(set(tags)) != len(tags):
            dupes = sorted({t for t in tags if tags.count(t) > 1})
            sys.exit("run tag collision — two genomes share a name: %s" % ", ".join(dupes[:5]))
        with concurrent.futures.ThreadPoolExecutor(max_workers=self.jobs) as ex:
            done = {t: (bk, rk, ch) for t, bk, rk, ch in ex.map(run_one, jobs)}
        for tag, k, b, r, d in meta:
            bk, rk, ch = done[tag]
            self.keys[k] = (bk, rk, ch)
            if self.sink:
                self.sink.note(d, b, r, bk, rk, ch)
        self.runs += len(jobs)

    def blue(self, b, r, d):
        return self.keys[self._k(b, r, d)][0]

    def red(self, b, r, d):
        return self.keys[self._k(b, r, d)][1]

    def chan(self, b, r, d):
        return self.keys[self._k(b, r, d)][2]


CSV_COLS = ["mission", "blue", "red", "blue_line", "red_line", "bV", "bM", "bC_air", "bC_aim",
            "rV", "rM", "rC_air", "rC_aim"] + list(arena.CHAN_COLS) + ["exit", "durationS"]


class Sink:
    def __init__(self, path):
        self.path = path
        if path and not os.path.exists(path):
            with open(path, "w") as f:
                f.write(",".join(CSV_COLS) + "\n")

    def note(self, duel, b, r, bk, rk, chan):
        if not self.path:
            return
        row = [duel.mission, b.name, r.name, b.line(), r.line()]
        for k in (bk, rk):
            row += [k[0], k[1], "GATE" if k[2] == fit.GATE else "%.1f" % k[2][0],
                    "GATE" if k[2] == fit.GATE else "%.1f" % k[2][1]]
        row += ["%g" % chan.get(c, 0.0) for c in arena.CHAN_COLS] + [chan["exit"],
                                                                    "%.1f" % chan["durationS"]]
        with open(self.path, "a") as f:
            f.write(",".join(str(x).replace(",", ";") for x in row) + "\n")


# ---------------------------------------------------------------------------------------------
# THE INSTRUMENT CHECK — before any generation, and it decides whether a co-evolution is possible.
# ---------------------------------------------------------------------------------------------
def probe(pool, duels, levers, side, out):
    """One side's lever set spliced in while the other stays COMMITTED, and BOTH keys read. Two
    questions, and the second is the one that decides whether this is a co-evolution at all:

      REACH-OWN   does the lever move the mover's OWN outcome class?  (S2, for that side)
      REACH-FOE   does it move the OPPOSING side's outcome class?     (the coupling)

    A side whose levers cannot move the other side's key is not an opponent that can answer, and an
    archive of it is an archive of a world that never moved."""
    base = evo.Genome("baseline", {}, dl="", sort="")
    pairs = [(base, base)] + [((g, base) if side == "blue" else (base, g)) for g in levers]
    pool.demand(pairs, "probe-%s-" % side)
    own = (lambda b, r, d: pool.blue(b, r, d)) if side == "blue" else (lambda b, r, d: pool.red(b, r, d))
    foe = (lambda b, r, d: pool.red(b, r, d)) if side == "blue" else (lambda b, r, d: pool.blue(b, r, d))

    print("\n%s" % ("=" * 104))
    print("PROBE — %s carries the lever, the other side is the COMMITTED mission text" % side.upper())
    print("=" * 104)
    print("%-22s %-26s %-26s %s" % ("lever", "own class (V,M)", "foe class (V,M)", "moves own/foe"))
    movers_own = collections.Counter()
    movers_foe = collections.Counter()
    rows = []
    for d in duels:
        b0 = fit.outcome_class(own(base, base, d))
        f0 = fit.outcome_class(foe(base, base, d))
        print("\n  %s   committed baseline: own %s   foe %s" % (d.mission, b0, f0))
        for g in levers:
            pair = (g, base) if side == "blue" else (base, g)
            o = fit.outcome_class(own(pair[0], pair[1], d))
            f = fit.outcome_class(foe(pair[0], pair[1], d))
            mo, mf = o != b0, f != f0
            if mo:
                movers_own[g.name] += 1
            if mf:
                movers_foe[g.name] += 1
            rows.append((d.mission, g.name, o, f, mo, mf))
            if mo or mf:
                print("    %-20s own %-12s foe %-12s   %s%s"
                      % (g.name, str(o), str(f), "OWN " if mo else "    ", "FOE" if mf else ""))
    print("\n%-22s %8s %8s" % ("lever", "own", "foe"))
    for g in levers:
        print("%-22s %8d %8d" % (g.name, movers_own[g.name], movers_foe[g.name]))
    ncells = len(duels)
    print("\nreach of the %s alphabet: %d of %d levers move their OWN class somewhere, "
          "%d move the FOE's" % (side, len([g for g in levers if movers_own[g.name]]), len(levers),
                                 len([g for g in levers if movers_foe[g.name]])))
    print("cells on which the %s side has ANY own-class gradient: %d of %d"
          % (side, len({r[0] for r in rows if r[4]}), ncells))
    print("cells on which the %s side can move the FOE: %d of %d"
          % (side, len({r[0] for r in rows if r[5]}), ncells))
    with open(os.path.join(out, "probe-%s.csv" % side), "w") as f:
        w = csv.writer(f)
        w.writerow(["mission", "lever", "own_V", "own_M", "foe_V", "foe_M", "moves_own", "moves_foe"])
        for m, n, o, fo, mo, mf in rows:
            w.writerow([m, n, o[0], o[1], fo[0], fo[1], int(mo), int(mf)])
    return movers_own, movers_foe


def perturbed(text, team, module, metres):
    """The cell's mission with ONE side's spawn displaced east by `metres` — `arena.chaos_flips`'
    transform, lifted out so the two-sided rig can screen a baseline that carries a RED genome. The
    perturbed side is always the one whose doctrine is under test."""
    out, tm, mod = [], "", ""
    for raw in text.splitlines(True):
        t = raw.split("#", 1)[0].split()
        if t[:1] == ["unit"]:
            tm = mod = ""
        elif len(t) > 1 and t[0] == "team":
            tm = t[1]
        elif len(t) > 1 and t[0] == "module":
            mod = t[1]
        elif len(t) > 2 and t[0] == "spawn" and tm == team and mod == module:
            lat, lon = float(t[1]), float(t[2])
            t[2] = "%.8f" % (lon + metres / (111320.0 * max(1e-6, math.cos(math.radians(lat)))))
            raw = "  " + " ".join(t) + "\n"
        out.append(raw)
    return "".join(out)


def chaos_screen(pool, duels, reds, out, gym, jobs, threads, elev):
    """S7, asked of every (cell, RED genome) pair rather than of the committed cell alone. E18 measured
    S7 on baselines whose opposing side was the committed mission text; a red genome makes a NEW
    baseline, and a cell that is chaos-clean under one opponent is not thereby clean under another.
    Screening only the committed pair would be reading a coin four times over."""
    print("\n%s\nS7 CHAOS SCREEN, per (cell, red genome) — %d spawn perturbations of the BLUE side"
          % ("=" * 104, len(arena.kChaosSteps)))
    pool.demand([(COMMITTED, r) for r in reds], "s7base-")
    jobslist, meta = [], []
    for d in duels:
        text = open(d.path).read()
        for r in reds:
            for i, m in enumerate(arena.kChaosSteps):
                t, bc, rc = splice_both(perturbed(text, d.blue[0], d.blue[1], m), d, COMMITTED, r)
                tag = "s7-%s-%s-%d" % (d.mission, r.name, i)
                jobslist.append((gym, out, tag, d, t, threads, elev, bc, rc, False))
                meta.append((tag, d, r))
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        done = {tag: bk for tag, bk, _, _ in ex.map(run_one, jobslist)}
    flips = collections.Counter()
    for tag, d, r in meta:
        if fit.outcome_class(done[tag]) != fit.outcome_class(pool.blue(COMMITTED, r, d)):
            flips[(d.mission, r.name)] += 1
    print("%-26s %s" % ("cell", "  ".join("%-12s" % r.name for r in reds)))
    clean = set()
    for d in duels:
        row = []
        for r in reds:
            n = flips[(d.mission, r.name)]
            row.append("%-12s" % ("%d of %d %s" % (n, len(arena.kChaosSteps),
                                                   "ok" if n <= arena.kChaosMaxFlips else "NO")))
            if n <= arena.kChaosMaxFlips:
                clean.add((d.mission, r.name))
        print("%-26s %s" % (d.mission, "  ".join(row)))
    print("chaos-clean (cell, red) pairs: %d of %d" % (len(clean), len(duels) * len(reds)))
    return clean, len(jobslist)


def sign_test(w, l):
    """One-sided sign test over the non-tied cells, exact: P(W >= w) under p = 1/2."""
    n = w + l
    if n == 0:
        return 1.0
    return sum(math.comb(n, k) for k in range(w, n + 1)) / float(2 ** n)


def sweep(pool, duels, blues, reds, clean, out):
    """E18 §4's instrument, run once per RED opponent: every blue lever as a ONE-GENE displacement from
    the committed blue, compared against the committed blue on each chaos-clean cell, one-sided sign
    test over the non-tied cells. Read at the OUTCOME CLASS (V, then M) — E18 §4's own warning, X-11:
    a level-C-only effect reaches p = 0.031 and looks exactly like a doctrine shift."""
    pool.demand([(b, r) for b in [COMMITTED] + blues for r in reds], "sw-")
    rows = []
    print("\n%s\nTHE SWEEP, PER RED OPPONENT — %d blue levers x %d reds, class level (V, M)"
          % ("=" * 104, len(blues), len(reds)))
    for r in reds:
        cells = [d for d in duels if (d.mission, r.name) in clean]
        print("\n  red = %-12s  %d chaos-clean cell(s): %s"
              % (r.name, len(cells), ", ".join(c.mission for c in cells)))
        print("  %-14s %-8s %4s %4s %4s %9s" % ("lever", "cells", "W", "L", "T", "p"))
        for b in blues:
            sgn, w, l, t = "", 0, 0, 0
            for d in cells:
                a = fit.outcome_class(pool.blue(b, r, d))
                z = fit.outcome_class(pool.blue(COMMITTED, r, d))
                c = fit.compare((a[0], a[1], fit.GATE), (z[0], z[1], fit.GATE))
                sgn += "+" if c > 0 else "-" if c < 0 else "."
                w += c > 0
                l += c < 0
                t += c == 0
            p = sign_test(w, l)
            rows.append((r.name, b.name, sgn, w, l, t, p))
            if w + l:
                print("  %-14s %-8s %4d %4d %4d %9.3f %s"
                      % (b.name, sgn, w, l, t, p, "BETTER" if w > l else "worse"))
    with open(os.path.join(out, "sweep.csv"), "w") as f:
        wr = csv.writer(f)
        wr.writerow(["red", "lever", "signs", "W", "L", "T", "p"])
        wr.writerows(rows)
    return rows


# ---------------------------------------------------------------------------------------------
# THE CO-EVOLUTION
# ---------------------------------------------------------------------------------------------
def ciao_row(pool, side, me, field, opponents, duels):
    """One row of the CIAO matrix: for each opponent, `me`'s mean `pair_points` against the rest of
    its OWN field, over the cells. Like against like — never against the opponent's key."""
    row = {}
    key = pool.blue if side == "blue" else pool.red
    for o in opponents:
        pts, n = 0.0, 0
        for peer in field:
            if peer.name == me.name:
                continue
            for d in duels:
                a = key(me, o, d) if side == "blue" else key(o, me, d)
                b = key(peer, o, d) if side == "blue" else key(o, peer, d)
                pts += fit.pair_points(a, b)[0]
                n += 1
        row[o.name] = pts / max(1, n)
    return row


def frozen_score(pool, side, me, frozen, duels):
    """§3.6 a, and it is EXACT here for the same reason `fb_campaign_evolve` says it is: the opposing
    side is the COMMITTED mission text, which cannot answer. `me` against a field that never changes,
    in a world that never changes."""
    pts, n = 0.0, 0
    key = pool.blue if side == "blue" else pool.red
    for ref in frozen:
        if ref.line() == me.line():
            continue
        for d in duels:
            a = key(me, COMMITTED, d) if side == "blue" else key(COMMITTED, me, d)
            b = key(ref, COMMITTED, d) if side == "blue" else key(COMMITTED, ref, d)
            pts += fit.pair_points(a, b)[0]
            n += 1
    return pts / max(1, n)


def grid_poll(champ, keys, bands, alleles, generation, points, tag):
    """`fb_campaign_evolve.grid_poll`'s operator, with the allele list passed in — red's is shorter
    because the MiG-29 has no channel bit to brief."""
    pop, seen = [champ], {champ.line()}
    for gi, key in enumerate(keys):
        lo, hi = bands[key]
        for i in range(points):
            v = round(lo + (hi - lo) * i / float(points - 1), 4)
            child = evo.Genome("%s%d_%d%d" % (tag, generation, gi, i), champ.params, champ.dl,
                               champ.sort)
            child.params[key] = v
            if child.line() in seen:
                continue
            seen.add(child.line())
            pop.append(child)
    for ai, (dl, sort) in enumerate(alleles):
        child = evo.Genome("%s%d_s%d" % (tag, generation, ai), champ.params, dl, sort)
        if child.line() in seen:
            continue
        seen.add(child.line())
        pop.append(child)
    return pop


def narrow(bands, champ, alphabet, keys):
    out = {}
    for key in keys:
        lo, hi = bands[key]
        e = alphabet[key]
        w = (hi - lo) / 4.0
        c = champ.params.get(key, 0.5 * (lo + hi))
        out[key] = (max(e["lo"], c - w), min(e["hi"], c + w))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", required=True)
    ap.add_argument("--gym", default=os.path.join(SIM_DIR, "build", "fb-gym"))
    ap.add_argument("--cells", default=os.path.join(SIM_DIR, "tools", "duels-sat.txt"))
    ap.add_argument("--levers", default=os.path.join(SIM_DIR, "tools", "levers-campaign-g5.txt"))
    ap.add_argument("--probe", default="", help="blue | red | both — the instrument check, no evolution")
    ap.add_argument("--sweep", action="store_true",
                    help="E18 §4's sweep, run once per RED opponent, preceded by the per-(cell, red) "
                         "chaos screen. The co-evolutionary question asked exhaustively rather than "
                         "searched: does blue's lever ORDER depend on which red it faces?")
    ap.add_argument("--genes", default="", help="comma-separated subset of the genome, per side "
                                                "as blue:a,b;red:c,d — declared, never derived")
    ap.add_argument("--generations", type=int, default=4)
    ap.add_argument("--points", type=int, default=3)
    ap.add_argument("--archive-sample", type=int, default=3)
    ap.add_argument("--elev", default="const")
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--threads", type=int, default=1)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--channels", default="")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    alphabet = evo.pilot_alphabet(args.gym)
    duels = load_duels(args.cells)
    if not arena.tree_clean():
        sys.exit("sim/missions or sim/assets is dirty BEFORE the run")
    print("mission and model tree clean before the run")
    print("duels: %d, from %s" % (len(duels), os.path.basename(args.cells)))
    for d in duels:
        print("   %-26s blue %s/%s   red %s/%s" % (d.mission, d.blue[0], d.blue[1],
                                                   d.red[0], d.red[1]))
    sink = Sink(args.channels or os.path.join(args.out, "coevolve-channels.csv"))
    pool = Pool(args.gym, args.out, duels, args.jobs, args.threads, args.elev, args.keep, sink)

    if args.probe:
        levers = [evo.Genome(v.name, v.params, v.dl, v.sort) for v in arena.load_levers(args.levers)]
        for side in (["blue", "red"] if args.probe == "both" else [args.probe]):
            probe(pool, duels, levers, side, args.out)
        print("\nruns: %d" % pool.runs)
        return 0 if arena.tree_clean() else 1

    # RED'S WHOLE SPACE, ENUMERATED. Measured in this round: of the nine genes of §2/§7, exactly ONE
    # reaches the MiG-29 at all, and its alphabet is the five sort contracts the parser accepts. A
    # space of five is smaller than any population that would search it, so red is enumerated rather
    # than evolved — which is strictly stronger and is stated rather than dressed up.
    reds = [evo.Genome("red-" + (s or "left"), {}, dl="", sort=("" if s == "left" else s))
            for s in ("left", "near", "right", "far", "none")]
    if args.sweep:
        blues = [evo.Genome(v.name, v.params, v.dl, v.sort) for v in arena.load_levers(args.levers)]
        clean, n = chaos_screen(pool, duels, reds, args.out, args.gym, args.jobs, args.threads,
                                args.elev)
        sweep(pool, duels, blues, reds, clean, args.out)
        print("\nruns: %d (+%d screen)" % (pool.runs, n))
        return 0 if arena.tree_clean() else 1

    want = {"blue": [], "red": []}
    for part in args.genes.split(";"):
        if ":" in part:
            s, ks = part.split(":", 1)
            want[s.strip()] = [k.strip() for k in ks.split(",") if k.strip()]
    allkeys = [k for k, blocked in evo.GENES if not blocked and k in alphabet]
    # `-` is the EMPTY alphabet, spelled out rather than left to fall through to "all": red's numeric
    # half is measured inert on the MiG-29 and a side whose declared genome is empty must be sayable.
    genes = {s: ([] if want[s] == ["-"] else [k for k in allkeys if not want[s] or k in want[s]])
             for s in ("blue", "red")}
    alleles = {"blue": evo.SORT_ALLELES, "red": RED_SORT_ALLELES}
    for s in ("blue", "red"):
        print("\n%s genome: %d gene(s) of %d pilot keys, %d sort allele(s)"
              % (s, len(genes[s]), len(alphabet), len(alleles[s])))
        for k in genes[s]:
            e = alphabet[k]
            print("   %-26s %s %g..%g x %s" % (k, e["kind"], e["lo"], e["hi"], e["hook"] or "-"))

    bands = {s: {k: (alphabet[k]["lo"], alphabet[k]["hi"]) for k in genes[s]} for s in ("blue", "red")}
    seed = {s: evo.Genome("%s0_seed" % s[0],
                          {k: round(0.5 * (bands[s][k][0] + bands[s][k][1]), 4) for k in genes[s]},
                          *alleles[s][0]) for s in ("blue", "red")}
    # THE FROZEN FIELD, fixed here for the whole run and never touched again: the seed plus the
    # extreme allele of every gene the side carries. §3.6 a needs a field that cannot drift; a field
    # drawn from the population would drift with it and the instrument would read its own motion.
    frozen = {}
    for s in ("blue", "red"):
        f = [seed[s]]
        for i, k in enumerate(genes[s]):
            for j, v in enumerate((bands[s][k][0], bands[s][k][1])):
                g = evo.Genome("%sfz%d%d" % (s[0], i, j), seed[s].params, *alleles[s][0])
                g.params[k] = round(v, 4)
                f.append(g)
        frozen[s] = f
        print("frozen field %s: %d members" % (s, len(f)))

    pop = {s: grid_poll(seed[s], genes[s], bands[s], alleles[s], 0, args.points, s[0])
           for s in ("blue", "red")}
    archive = {"blue": [], "red": []}
    champions = {"blue": [], "red": []}
    scores = {"blue": [], "red": []}

    for gen in range(args.generations):
        samp = {s: evo.archive_sample(archive[s], args.archive_sample) for s in ("blue", "red")}
        opp = {"blue": pop["red"] + samp["red"], "red": pop["blue"] + samp["blue"]}
        pairs = [(b, r) for b in pop["blue"] for r in opp["blue"]]
        pairs += [(b, r) for r in pop["red"] for b in opp["red"]]
        pairs += [(g, COMMITTED) for g in pop["blue"] + frozen["blue"]]
        pairs += [(COMMITTED, g) for g in pop["red"] + frozen["red"]]
        before = pool.runs
        pool.demand(pairs, "g%d-" % gen)

        print("\n%s\ngeneration %d — blue %d, red %d, archives %d/%d (sampled %d/%d), "
              "%d new runs, %d total" % ("=" * 104, gen, len(pop["blue"]), len(pop["red"]),
              len(archive["blue"]), len(archive["red"]), len(samp["blue"]), len(samp["red"]),
              pool.runs - before, pool.runs))
        champ = {}
        for s in ("blue", "red"):
            rows = {g.name: ciao_row(pool, s, g, pop[s], opp[s], duels) for g in pop[s]}
            fitness = {n: sum(v.values()) / max(1, len(v)) for n, v in rows.items()}
            froz = {g.name: frozen_score(pool, s, g, frozen[s], duels) for g in pop[s]}
            order = sorted(pop[s], key=lambda g: (-froz[g.name], -fitness[g.name], g.name))
            c = order[0]
            champ[s] = c
            champions[s].append(c)
            scores[s].append(froz[c.name])
            print("\n  %s — champion %s   frozen %.3f   co-evo %.3f"
                  % (s, c.name, froz[c.name], fitness[c.name]))
            for g in sorted(pop[s], key=lambda g: -fitness[g.name])[:6]:
                print("     %-10s co-evo %.3f  frozen %.3f   %s"
                      % (g.name, fitness[g.name], froz[g.name], g.line()))
            for g in order:
                if len(archive[s]) >= evo.kArchiveMax:
                    break
                key = pool.blue if s == "blue" else pool.red
                beh = tuple(key(g, COMMITTED, d) if s == "blue" else key(COMMITTED, g, d)
                            for d in duels)
                if any(getattr(a, "behaviour", None) == beh for a in archive[s]):
                    continue
                if not evo.dominated(rows[g.name], [a.win for a in archive[s] if hasattr(a, "win")]):
                    g.win, g.gen, g.behaviour = rows[g.name], gen, beh
                    archive[s].append(g)
        for s in ("blue", "red"):
            if gen:
                bands[s] = narrow(bands[s], champ[s], alphabet, genes[s])
            pop[s] = grid_poll(champ[s], genes[s], bands[s], alleles[s], gen + 1, args.points, s[0])

    print("\n%s\nCIRCLING — the three instruments of doc/doctrine-evolution.md §3.6" % ("=" * 104))
    for s in ("blue", "red"):
        print("\n%s" % s)
        print("  (a) frozen field per generation: %s" % " ".join("%.3f" % x for x in scores[s]))
        nd = all(scores[s][i] >= scores[s][i - 1] - 1e-9 for i in range(1, len(scores[s])))
        print("      non-decreasing over the window: %s" % ("yes" if nd else "NO — §3.6 a"))
        uniq = {g.line(): g for g in champions[s]}
        cw = {}
        if len(uniq) >= 3:
            pool.demand([(a, b) for a in uniq.values() for b in [COMMITTED]] if s == "blue"
                        else [(COMMITTED, b) for b in uniq.values()], "champ-")
            names = list(uniq)
            key = pool.blue if s == "blue" else pool.red
            for a in names:
                cw[uniq[a].name] = {}
                for b in names:
                    if a == b:
                        continue
                    pts = 0.0
                    for d in duels:
                        ka = key(uniq[a], COMMITTED, d) if s == "blue" else key(COMMITTED, uniq[a], d)
                        kb = key(uniq[b], COMMITTED, d) if s == "blue" else key(COMMITTED, uniq[b], d)
                        pts += fit.pair_points(ka, kb)[0]
                    cw[uniq[a].name][uniq[b].name] = pts / len(duels)
            t, dd, total = evo.cyclic_triples(cw)
            print("  (b) champion graph: n=%d  cyclic triples d=%d of %d  T=%.4f  %s"
                  % (len(names), dd, total, t, "ok" if t <= 0.05 else "NO"))
        else:
            print("  (b) champion graph: %d distinct champion(s) — fewer than 3, not computable"
                  % len(uniq))
        print("  (c) doctrine trajectory, min distance to a champion 3+ generations back:")
        any_traj = False
        for i in range(3, len(champions[s])):
            vi = champions[s][i].vector(alphabet)
            best = min(math.dist(vi, champions[s][j].vector(alphabet)) for j in range(i - 2))
            print("      champion %d: %.4f" % (i, best))
            any_traj = True
        if not any_traj:
            print("      (needs at least four generations)")

    print("\nFINAL CHAMPIONS")
    for s in ("blue", "red"):
        print("  %-5s %s" % (s, champions[s][-1].line()))
    print("\nCHAMPION PAIR against the committed world, per cell")
    pool.demand([(champions["blue"][-1], champions["red"][-1]),
                 (champions["blue"][-1], COMMITTED), (COMMITTED, champions["red"][-1]),
                 (COMMITTED, COMMITTED)], "final-")
    cb, cr = champions["blue"][-1], champions["red"][-1]
    print("  %-26s %-22s %-22s %-22s %s" % ("cell", "committed/committed", "champ/committed",
                                            "committed/champ", "champ/champ"))
    for d in duels:
        print("  %-26s %-22s %-22s %-22s %s"
              % (d.mission,
                 "%s|%s" % (fit.outcome_class(pool.blue(COMMITTED, COMMITTED, d)),
                            fit.outcome_class(pool.red(COMMITTED, COMMITTED, d))),
                 "%s|%s" % (fit.outcome_class(pool.blue(cb, COMMITTED, d)),
                            fit.outcome_class(pool.red(cb, COMMITTED, d))),
                 "%s|%s" % (fit.outcome_class(pool.blue(COMMITTED, cr, d)),
                            fit.outcome_class(pool.red(COMMITTED, cr, d))),
                 "%s|%s" % (fit.outcome_class(pool.blue(cb, cr, d)),
                            fit.outcome_class(pool.red(cb, cr, d)))))

    for s in ("blue", "red"):
        p = os.path.join(args.out, "archive-%s.txt" % s)
        with open(p, "w") as f:
            f.write("# FlightBox doctrine archive (%s side) — one genome per line, re-flyable.\n" % s)
            f.write("# arena=%s (%d duels) elev=%s\n"
                    % (os.path.basename(args.cells), len(duels), args.elev))
            for i, g in enumerate(archive[s]):
                f.write("%s   # gen=%d idx=%d\n" % (g.line(), g.gen, i))
        print("archive %s: %d member(s) -> %s" % (s, len(archive[s]), p))
    print("\nruns: %d" % pool.runs)
    if not arena.tree_clean():
        print("VOID: sim/missions or sim/assets moved during the run")
        return 1
    print("mission and model tree clean after the run")
    return 0


if __name__ == "__main__":
    sys.exit(main())
