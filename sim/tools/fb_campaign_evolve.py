#!/usr/bin/env python3
"""FlightBox — DOCTRINE EVOLUTION OVER THE CAMPAIGN BREADTH. doc/doctrine-evolution.md §7, round `E5`.

    tools/fb_campaign_evolve.py --out /tmp/evo --cells tools/arena-informative.txt --generations 6

WHAT IS DIFFERENT FROM `fb_evolve.py`, and both differences are properties of the ARENA rather than
choices of this file:

  1. THE OPPONENT IS COMMITTED MISSION TEXT. A campaign rung's other side is a script; it cannot
     answer. So this is not a co-evolution and the Red Queen is excluded by construction — a rise
     against the fixed yardstick is a rise against a fixed world. Instrument (a) of §3.6 is therefore
     EXACT here, not a proxy. (b) and (c) stay live: a pairwise order summed over many cells can be
     intransitive even when every cell is deterministic.
  2. A GENOME'S KEY ON A CELL NEVER CHANGES. The environment does not move, so a genome flown once is
     known forever — the archive costs no runs at all, and a generation costs exactly
     (new genomes) x (cells). The cache is keyed by the genome's own TEXT LINE, which is what a
     genome is (doc/pilot.md §9).

The comparison is like against like, cell by cell (`fb_fitness.pair_points` per cell, averaged), which
is D6's rule with the CELL in the seat's place: comparing two units inside one run measures the seat,
and on a campaign rung the two sides are not even the same aircraft.

Stdlib only, no build target, one dependency: build/fb-gym.
"""

import argparse
import concurrent.futures
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fb_campaign_arena as arena
import fb_evolve as evo
import fb_fitness as fit
import fb_tournament as tour

SIM_DIR = arena.SIM_DIR
kYardstick = os.path.join(SIM_DIR, "tools", "variants-bvr.txt")


class Pool:
    """Every (genome line, cell) ever flown, and the key it produced. The one place a run is spent."""

    def __init__(self, gym, out, cells, jobs, threads, elev, keep):
        self.gym, self.out, self.cells = gym, out, cells
        self.jobs, self.threads, self.elev, self.keep = jobs, threads, elev, keep
        self.keys = {}
        self.runs = 0

    def demand(self, genomes, tagpfx):
        want = [g for g in genomes if (g.line(), self.cells[0].name) not in self.keys]
        if not want:
            return
        variants = [g.variant() for g in want]
        by_name = {v.name: g for v, g in zip(variants, want)}
        got = arena.fly(self.gym, self.out, self.cells, variants, self.jobs, self.threads,
                        self.elev, self.keep, tagpfx)
        for (cellname, vname), (key, _, _, chan) in got.items():
            self.keys[(by_name[vname].line(), cellname)] = (key, chan)
        self.runs += len(want) * len(self.cells)

    def key(self, genome, cell):
        return self.keys[(genome.line(), cell.name)][0]

    def chan(self, genome, cell):
        return self.keys[(genome.line(), cell.name)][1]


# ---------------------------------------------------------------------------------------------
# THE SEARCH OPERATOR — a coordinate-wise GRID poll whose grid halves its width per generation, and
# the reason it is not `fb_evolve.mutate`'s +-step poll is MEASURED (doc/doctrine-evolution.md §State
# E5, D8): the ground half's fitness is a PLATEAU with a knife edge in it. `pilot_attack_bias_s` is a
# compiled +-10 s band whose useful neighbourhood is +-0.08 s (half a decision tick, 18 m of track
# against a Mk-84's 17.7 m hardened radius), and every value outside it produces the identical outcome
# class. A local +-step poll that shrinks on stagnation therefore shrinks around wherever the first
# champion happened to sit and never reaches the edge — measured: 94 runs, 8 generations, champion
# `bias=-7.2` unmoved, poll step down to 0.0039 of the band.
#
# A grid that always spans the CURRENT BRACKET cannot have that failure: the bracket contains the
# optimum at every level, and halving it per generation reaches 0.08 s of a 20 s band in six
# generations (20 / 2^6 = 0.31 s of bracket, 0.078 s of spacing at 5 points). Deterministic, no
# tolerance constant, no restart — conventions.md's rule on randomness is untouched.
def grid_poll(champ, alphabet, keys, bands, generation, points):
    """The champion, one gene at a time, sampled over its current bracket; plus the three sort alleles
    it is not carrying. Everything else stays at the champion's value, so one comparison is one gene."""
    pop, seen = [champ], {champ.line()}
    for gi, key in enumerate(keys):
        lo, hi = bands[key]
        for i in range(points):
            v = round(lo + (hi - lo) * i / float(points - 1), 4)
            child = evo.Genome("g%d_%d%d" % (generation, gi, i), champ.params, champ.dl, champ.sort)
            child.params[key] = v
            if child.line() in seen:
                continue
            seen.add(child.line())
            pop.append(child)
    for ai, (dl, sort) in enumerate(evo.SORT_ALLELES):
        child = evo.Genome("g%d_s%d" % (generation, ai), champ.params, dl, sort)
        if child.line() in seen:
            continue
        seen.add(child.line())
        pop.append(child)
    return pop


def narrow(bands, champ, alphabet, keys):
    """Halve every bracket around the champion's own value, clamped to the key's compiled band. The
    clamp is FBPilotTuning's, so a bracket can never leave what the simulator would accept."""
    out = {}
    for key in keys:
        lo, hi = bands[key]
        e = alphabet[key]
        w = (hi - lo) / 4.0
        c = champ.params.get(key, 0.5 * (lo + hi))
        out[key] = (max(e["lo"], c - w), min(e["hi"], c + w))
    return out


def match(pool, a, b):
    """A MATCH is the whole arena: the two genomes' keys compared CELL BY CELL and averaged. Nothing
    is summed across cells before the comparison — a side key is a componentwise sum over one rung's
    members and is comparable only within that rung (doc/campaigns/w1-red-flag.md)."""
    pa = 0.0
    for c in pool.cells:
        pa += fit.pair_points(pool.key(a, c), pool.key(b, c))[0]
    return pa / len(pool.cells)


def round_robin(pool, contenders, opponents):
    win = {}
    for c in contenders:
        win[c.name] = {o.name: match(pool, c, o) for o in opponents if o.name != c.name}
    return win


def levels(pool, contenders, opponents):
    """Which LEVEL decided, over every comparison of the round robin — the report E-11 made mandatory:
    a full ranking out of level C alone is a saturated arena wearing a ranking."""
    n = {"V": 0, "M": 0, "C": 0, "tie": 0}
    for c in contenders:
        for o in opponents:
            if c.name == o.name:
                continue
            for cell in pool.cells:
                a, b = pool.key(c, cell), pool.key(o, cell)
                if a[0] != b[0]:
                    n["V"] += 1
                elif a[1] != b[1]:
                    n["M"] += 1
                elif fit.compare_craft(a[2], b[2]):
                    n["C"] += 1
                else:
                    n["tie"] += 1
    return n


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", required=True)
    ap.add_argument("--gym", default=os.path.join(SIM_DIR, "build", "fb-gym"))
    ap.add_argument("--cells", default=os.path.join(SIM_DIR, "tools", "arena-informative.txt"))
    ap.add_argument("--generations", type=int, default=6)
    ap.add_argument("--points", type=int, default=5,
                    help="grid points per gene per generation — the population is 1 + genes x points "
                         "+ the sort alleles, minus the duplicates the cache already knows")
    ap.add_argument("--elev", default="const")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--threads", type=int, default=2)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--archive", default="")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    archive_path = args.archive or os.path.join(args.out, "archive.txt")
    alphabet = evo.pilot_alphabet(args.gym)
    cells = arena.load_cells(args.cells)

    live = [k for k, b in evo.GENES if not b and (k in alphabet or k == "sort")]
    blocked = [(k, b) for k, b in evo.GENES if b]
    reachable = [k for k in alphabet if k in [g for g, _ in evo.GENES]]
    print("evolving %d genes of %d pilot keys; %d keys not in the genome; 0 non-pilot keys reachable"
          % (len(live), len(alphabet), len(alphabet) - len(reachable)))
    for k in live:
        e = alphabet.get(k)
        print("   gene %-22s %s" % (k, "the briefed sort contract + the channel bit (mission text)"
              if e is None else "%s %g..%g%s" % (e["kind"], e["lo"], e["hi"],
                                                " x " + e["hook"] if e["hook"] else "")))
    for k, why in blocked:
        print("   BLOCKED %-19s %s" % (k, why))
    if not arena.tree_clean():
        sys.exit("sim/missions or sim/assets is dirty BEFORE the run")
    print("mission and model tree clean before the run")
    print("\narena: %d cells from %s" % (len(cells), os.path.basename(args.cells)))
    for c in cells:
        print("   %-26s %-9s %s" % (c.mission, c.team, c.module))

    pool = Pool(args.gym, args.out, cells, args.jobs, args.threads, args.elev, args.keep)
    yard = [evo.Genome(v.name, v.params, v.dl, v.sort) for v in tour.load_variants(kYardstick)]
    for g in yard:
        g.dl = ""                      # the yardstick is a PILOT doctrine; it briefs no channel bit
    pool.demand(yard, "yard-")

    keys = [k for k, b in evo.GENES if not b and k in alphabet]
    bands = {k: (alphabet[k]["lo"], alphabet[k]["hi"]) for k in keys}
    seed = evo.Genome("g0_seed", {k: round(0.5 * (bands[k][0] + bands[k][1]), 4) for k in keys})
    pop = grid_poll(seed, alphabet, keys, bands, 0, args.points)
    archive, champions, champ_scores = [], [], []
    for gen in range(args.generations):
        pool.demand(pop, "g%d-" % gen)
        sample = evo.archive_sample(archive, evo.kArchiveSample)
        opponents = pop + sample
        win = round_robin(pool, pop, opponents)
        fitness = {n: sum(v.values()) / max(1, len(v)) for n, v in win.items()}
        order = sorted(pop, key=lambda g: -fitness[g.name])
        champ = order[0]
        champions.append(champ)
        yw = round_robin(pool, [champ], yard)
        ys = sum(yw[champ.name].values()) / max(1, len(yw[champ.name]))
        champ_scores.append(ys)
        lv = levels(pool, pop, opponents)

        print("\ngeneration %d — population %d, archive %d (sampled %d), runs so far %d"
              % (gen, len(pop), len(archive), len(sample), pool.runs))
        for g in order:
            print("   %-10s fitness %.3f   %s" % (g.name, fitness[g.name], g.line()))
        print("   brackets: %s" % "  ".join("%s [%.4g, %.4g]" % (k.replace("pilot_", ""),
              bands[k][0], bands[k][1]) for k in keys))
        print("   champion %s   yardstick %.3f" % (champ.name, ys))
        print("   decided at level:  V %d   M %d   C %d   exact tie %d"
              % (lv["V"], lv["M"], lv["C"], lv["tie"]))
        if lv["V"] == 0 and lv["M"] == 0:
            print("   SATURATED — no comparison turned on the judge's verdict or on an objective")

        for g in order:
            if len(archive) >= evo.kArchiveMax:
                break
            # §3.4 B admits "genuinely new behaviour". On a fixed arena the behaviour a fitness can see
            # IS the vector of per-cell keys, and admitting a genome that produces an existing member's
            # vector fills the archive with clones that then tie against everything.
            behaviour = tuple(pool.key(g, c) for c in cells)
            if any(getattr(a, "behaviour", None) == behaviour for a in archive):
                continue
            if not evo.dominated(win[g.name], [a.win for a in archive if hasattr(a, "win")]):
                g.win, g.gen, g.behaviour = win[g.name], gen, behaviour
                archive.append(g)
        bands = narrow(bands, champ, alphabet, keys) if gen else bands
        pop = grid_poll(champ, alphabet, keys, bands, gen + 1, args.points)

    cw = round_robin(pool, champions, champions)
    t, d, total = evo.cyclic_triples(cw)
    traj = []
    for i in range(3, len(champions)):
        vi = champions[i].vector(alphabet)
        traj.append((i, min(math.dist(vi, champions[j].vector(alphabet)) for j in range(i - 2))))

    with open(archive_path, "w") as f:
        f.write("# FlightBox doctrine archive — one genome per line, verbatim and re-flyable.\n")
        f.write("# arena=%s (%d campaign cells) elev=%s\n"
                % (os.path.basename(args.cells), len(cells), args.elev))
        for i, g in enumerate(archive):
            f.write("%s   # gen=%d idx=%d wins=%s\n" % (g.line(), g.gen, i,
                    ",".join("%s:%.2f" % kv for kv in sorted(g.win.items()))))

    print("\n" + "=" * 96)
    print("CIRCLING — the three instruments of doc/doctrine-evolution.md §3.6")
    print("=" * 96)
    print("(a) fixed yardstick per generation: %s" % " ".join("%.3f" % s for s in champ_scores))
    nondec = all(champ_scores[i] >= champ_scores[i - 1] - 1e-9 for i in range(1, len(champ_scores)))
    print("    non-decreasing over the window: %s" % ("yes" if nondec else "NO — see §3.6a"))
    print("    (EXACT on this arena: the opponent is committed mission text and cannot answer)")
    print("(b) champion graph: n=%d  cyclic triples d=%d of %d  T=%.4f  (acceptance T <= 0.05) %s"
          % (len(champions), d, total, t, "ok" if t <= 0.05 else "NO"))
    print("(c) doctrine trajectory, min distance to a champion 3+ generations back:")
    for i, best in traj:
        print("    champion %d: %.4f" % (i, best))
    if not traj:
        print("    (needs at least four generations)")

    print("\nCHAMPION, per cell, against the yardstick's baseline")
    base = [g for g in yard if g.name == "baseline"][0]
    for c in cells:
        kc, kb = pool.key(champions[-1], c), pool.key(base, c)
        print("   %-26s champion %-24s baseline %-24s %s"
              % (c.mission, fit.key_str(kc), fit.key_str(kb),
                 "better" if fit.compare(kc, kb) > 0 else
                 "worse" if fit.compare(kc, kb) < 0 else "same"))
    print("\nruns: %d   archive: %d member(s) -> %s" % (pool.runs, len(archive), archive_path))
    if not arena.tree_clean():
        print("VOID: sim/missions or sim/assets moved during the run")
        return 1
    print("mission and model tree clean after the run")
    return 0


if __name__ == "__main__":
    sys.exit(main())
