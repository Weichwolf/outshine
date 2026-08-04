#!/usr/bin/env python3
"""FlightBox — THE TWO-SIDED ARENA GATE. doc/doctrine-evolution.md §4.2, round `E30`.

    tools/fb_duel_arena.py --out build/e30 --cells tools/duels-e30.txt \
        --blue-levers tools/levers-campaign-g5.txt --red-levers tools/levers-red-mig29.txt \
        --chaos-screen

WHY IT EXISTS BESIDE `fb_campaign_arena.py`. That file grades ONE declared side: S1 is the modal share
of one side's outcome class over one side's lever population, S2 counts one side's movers, and S7 flies
one side's spawn grid. `E-30` is the measurement that this is not enough — on three of five cells the
OPPONENT's outcome class is constant over its entire strategy space, so half of every co-evolution on
them is a fixed point, and the one-sided gate cannot see it because it never asks.

WHAT IS DIFFERENT, in three sentences:

  * S1 AND S2 ARE ASKED OF BOTH SIDES, each under the alphabet ITS OWN airframe can express — blue's
    lever file and red's lever file are separate arguments, because a MiG-29 that cannot carry six of
    the nine genes (`E-31`) would otherwise be graded on a population that is 22 identity maps.
  * S7 IS A PROPERTY OF THE PAIR (`X-17`), so it is flown per (cell, OPPONENT ALLELE): the graded
    side's spawn is perturbed over the same 0.8 m grid while the other side carries each lever of its
    own file in turn, and the cell is admitted for a side only if EVERY pair is clean.
  * NOTHING IS COMPARED ACROSS THE TWO KEYS. Blue's class and red's class are read off the same run and
    reported side by side; an F-16 flight and a MiG-29 flight declare different objectives on different
    airframes, so a cross-side comparison measures the seat (`fb_fitness.match_points`).

Stdlib only, no build target, one dependency: build/fb-gym.
"""

import argparse
import collections
import concurrent.futures
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fb_campaign_arena as arena
import fb_campaign_coevolve as coevo
import fb_evolve as evo
import fb_fitness as fit

SIM_DIR = arena.SIM_DIR
COMMITTED = coevo.COMMITTED


def genomes(path):
    """`fb_tournament.load_variants` maps `sort=none` onto the EMPTY sort, i.e. onto "splice nothing",
    which is the identity map — and `none` is a real MiG-29 contract (`FBMig29Module::Set`) and the one
    allele `E19` measured as a mover on both sides. The raw token is therefore read back here, the same
    way `fb_campaign_arena.load_levers` reads `dl=` back."""
    out = [evo.Genome(v.name, v.params, v.dl, v.sort) for v in arena.load_levers(path)]
    for raw in open(path):
        t = raw.split("#", 1)[0].split()
        for g in out:
            if t[:1] == [g.name]:
                for tok in t[1:]:
                    if tok.startswith("sort="):
                        g.sort = tok[5:]
    return out


def s2_ok(movers, nlevers):
    return len(movers) >= arena.kMoversMin and len(movers) >= arena.kMoverFrac * nlevers


def sweep_side(pool, duels, levers, side):
    """One side's lever population against the COMMITTED opponent — S1's population and S2's."""
    base = COMMITTED
    pairs = [(base, base)] + [((g, base) if side == "blue" else (base, g)) for g in levers]
    pool.demand(pairs, "sw-%s-" % side)
    key = pool.blue if side == "blue" else pool.red
    out = {}
    for d in duels:
        cls = {}
        for g in [base] + levers:
            b, r = (g, base) if side == "blue" else (base, g)
            cls[g.name] = fit.outcome_class(key(b, r, d))
        out[d.mission] = cls
    return out


def chaos_pairs(pool, gym, out, duels, side, opponents, jobs, threads, elev):
    """S7 per (cell, opponent). The perturbed side is the GRADED one; the opponent carries its lever."""
    base = COMMITTED
    pool.demand([((base, o) if side == "blue" else (o, base)) for o in opponents], "s7base-%s-" % side)
    jobs_l, meta = [], []
    for d in duels:
        text = open(d.path).read()
        team, module = d.blue if side == "blue" else d.red
        for o in opponents:
            for i, m in enumerate(arena.kChaosSteps):
                b, r = (base, o) if side == "blue" else (o, base)
                t, bc, rc = coevo.splice_both(coevo.perturbed(text, team, module, m), d, b, r)
                tag = "s7-%s-%s-%s-%d" % (side, d.mission, o.name, i)
                jobs_l.append((gym, out, tag, d, t, threads, elev, bc, rc, False))
                meta.append((tag, d, o))
    done = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        for tag, bk, rk, _ in ex.map(coevo.run_one, jobs_l):
            done[tag] = bk if side == "blue" else rk
    flips = collections.Counter()
    for tag, d, o in meta:
        b, r = (base, o) if side == "blue" else (o, base)
        ref = pool.blue(b, r, d) if side == "blue" else pool.red(b, r, d)
        if fit.outcome_class(done[tag]) != fit.outcome_class(ref):
            flips[(d.mission, o.name)] += 1
    return flips, len(jobs_l)


def report(side, duels, cls, levers, flips, verdicts):
    n = len(levers)
    print("\n%s\n%s SIDE — %d levers + baseline, opponent = the committed mission text"
          % ("=" * 110, side.upper(), n))
    print("%-28s %8s %7s %-10s %7s %s"
          % ("cell", "distinct", "modal", "modal cls", "movers", "S1 S2"))
    for d in duels:
        c = cls[d.mission]
        classes = [c[g.name] for g in [COMMITTED] + levers]
        counts = collections.Counter(classes)
        modal_class, modal_n = counts.most_common(1)[0]
        modal = modal_n / float(len(classes))
        movers = [g.name for g in levers if c[g.name] != c[COMMITTED.name]]
        s1 = len(counts) >= 2 and modal <= arena.kModalMax
        s2 = s2_ok(movers, n)
        verdicts[(side, d.mission)] = {"s1": s1, "s2": s2, "modal": modal, "movers": movers,
                                       "classes": classes, "base": c[COMMITTED.name]}
        print("%-28s %8d %6.1f%% %-10s %7d %s %s"
              % (d.mission, len(counts), 100 * modal, str(modal_class), len(movers),
                 "ok" if s1 else "NO", "ok" if s2 else "NO"))
        print("      movers: %s" % (", ".join(movers) or "-"))
    if flips is not None:
        print("\n  S7, per (cell, opponent) — the %s spawn over the %d-point 0.8 m grid"
              % (side, len(arena.kChaosSteps)))
        opps = sorted({k[1] for k in flips.keys()}) if flips else []
        for d in duels:
            dirty = [(o, n) for (m, o), n in sorted(flips.items())
                     if m == d.mission and n > arena.kChaosMaxFlips]
            verdicts[(side, d.mission)]["s7dirty"] = dirty
            print("      %-28s %s" % (d.mission,
                  "all pairs 0 of %d" % len(arena.kChaosSteps) if not dirty
                  else "DIRTY: " + ", ".join("%s %d of %d" % (o, n, len(arena.kChaosSteps))
                                             for o, n in dirty)))
        del opps


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", required=True)
    ap.add_argument("--gym", default=os.path.join(SIM_DIR, "build", "fb-gym"))
    ap.add_argument("--cells", default=os.path.join(SIM_DIR, "tools", "duels-e30.txt"))
    ap.add_argument("--blue-levers", default=os.path.join(SIM_DIR, "tools",
                                                          "levers-campaign-g5.txt"))
    ap.add_argument("--red-levers", default=os.path.join(SIM_DIR, "tools",
                                                         "levers-red-mig29.txt"))
    ap.add_argument("--chaos-screen", action="store_true")
    ap.add_argument("--s7-sides", default="blue,red",
                    help="which side's S7 to FLY. A side whose objectives did not change since the last screen keeps its numbers by conservation — an objective is read by the judge alone, so re-laddering one side cannot move the other side's class — and re-flying 400 runs to reprint them is not a measurement")
    ap.add_argument("--emit-informative", default="")
    ap.add_argument("--elev", default="const")
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--threads", type=int, default=1)
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    if not arena.tree_clean():
        sys.exit("sim/missions or sim/assets is dirty BEFORE the run")
    duels = coevo.load_duels(args.cells)
    lev = {"blue": genomes(args.blue_levers), "red": genomes(args.red_levers)}
    print("two-sided arena: %d cells, blue %d levers, red %d levers"
          % (len(duels), len(lev["blue"]), len(lev["red"])))
    print("simulator %s" % arena.gym_identity(args.gym))
    sink = coevo.Sink(os.path.join(args.out, "duel-arena-channels.csv"))
    pool = coevo.Pool(args.gym, args.out, duels, args.jobs, args.threads, args.elev, False, sink)

    cls = {s: sweep_side(pool, duels, lev[s], s) for s in ("blue", "red")}
    flips = {"blue": None, "red": None}
    if args.chaos_screen:
        for s in [x for x in ("blue", "red") if x in args.s7_sides.split(",")]:
            other = "red" if s == "blue" else "blue"
            f, n = chaos_pairs(pool, args.gym, args.out, duels, s,
                               [COMMITTED] + lev[other], args.jobs, args.threads, args.elev)
            flips[s] = f
            print("S7 %s: %d runs" % (s, n))

    verdicts = {}
    for s in ("blue", "red"):
        report(s, duels, cls[s], lev[s], flips[s], verdicts)

    print("\n%s\nTHE GATE — a cell is TWO-SIDED INFORMATIVE only if BOTH sides pass S1, S2 and S7"
          % ("=" * 110))
    print("%-28s %-24s %-24s %s" % ("cell", "blue S1/S2/S7", "red S1/S2/S7", "two-sided"))
    good = []
    for d in duels:
        row, ok = [], True
        for s in ("blue", "red"):
            v = verdicts[(s, d.mission)]
            s7 = "-" if flips[s] is None else ("ok" if not v.get("s7dirty") else
                                               "NO(%d)" % len(v["s7dirty"]))
            row.append("%s %s %s" % ("ok" if v["s1"] else "NO", "ok" if v["s2"] else "NO", s7))
            ok = ok and v["s1"] and v["s2"] and (flips[s] is None or not v.get("s7dirty"))
        if ok:
            good.append(d)
        print("%-28s %-24s %-24s %s" % (d.mission, row[0], row[1], "YES" if ok else "no"))
    print("\nTWO-SIDED INFORMATIVE: %d of %d   [%s]"
          % (len(good), len(duels), ", ".join(d.mission for d in good) or "-"))

    with open(os.path.join(args.out, "duel-arena.csv"), "w") as f:
        w = csv.writer(f)
        w.writerow(["cell", "side", "lever", "V", "M"])
        for s in ("blue", "red"):
            for d in duels:
                for g in [COMMITTED] + lev[s]:
                    c = cls[s][d.mission][g.name]
                    w.writerow([d.mission, s, g.name, c[0], c[1]])
    if args.emit_informative:
        with open(args.emit_informative, "w") as f:
            f.write("# GENERATED by tools/fb_duel_arena.py — cells where BOTH sides pass S1, S2, S7.\n")
            for d in good:
                f.write("%-28s %-9s %-6s %-9s %s\n"
                        % (d.mission, d.blue[0], d.blue[1], d.red[0], d.red[1]))
    print("\nruns: %d" % pool.runs)
    if not arena.tree_clean():
        print("VOID: sim/missions or sim/assets moved during the run")
        return 1
    print("mission and model tree clean after the run")
    return 0 if len(good) >= arena.kInformativeMin else 1


if __name__ == "__main__":
    sys.exit(main())
