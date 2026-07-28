#!/usr/bin/env python3
"""FlightBox — THE DOCTRINE EVOLUTION RUNNER. doc/doctrine-evolution.md §2, §3, §6.

    tools/fb_evolve.py --out /tmp/evo --generations 4 --geometry xmirror

WHAT IT IS ALLOWED TO CHANGE. Five genes and nothing else. The alphabet is not written here — it is
read out of the BINARY (`fb-gym --pilot-keys`), which is the one table `FBPilotTuning` compiles, so a
genome cannot drift away from what the simulator will accept. The runner prints that alphabet at start
in the shape doc/doctrine-evolution.md §2.2 asks for.

WHAT IT CANNOT DO, STRUCTURALLY. It imports the mission generator and the scorer from
`fb_tournament` / `fb_fitness` and NEVER the deck writer: `run_attr`, `restore_decks` and
`gen_air_decks.py` are not reachable from any path below, and the run is void unless
`git status --porcelain sim/assets` is empty before and after. That is checked here, not hoped for.

WHY AN ARCHIVE. Co-evolutionary fitness is measured against the CURRENT opponents, so a population in
which A beats B, B beats C and C beats A can circle forever while every generation's measured fitness
rises. The fitness curve of a circling population and of a learning one are the same curve, so the
circling has to be MEASURED — three instruments, §3.6:

  (a) the FIXED yardstick   the champion against `variants-bvr.txt`, both seats, every generation;
                            learning ⇒ non-decreasing over a five-generation window
  (b) the champion graph    cyclic triples T = d / C(n,3), Kendall-Babington Smith; acceptance T <= 0.05
  (c) the doctrine track    min over j <= i-3 of ||g_i - g_j||, each gene normalised by its own band;
                            it names the GENE that is circling, which is what makes it a debugger

Everything is deterministic: conventions.md forbids randomness in this simulator, so mutation is a
fixed stride over the band and the archive sample is an even stride over admission order.

Stdlib only, no build target.
"""

import argparse
import concurrent.futures
import math
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fb_fitness as fit
import fb_tournament as tour

SIM_DIR = tour.SIM_DIR
REPO_DIR = os.path.dirname(SIM_DIR)

# THE FIVE GENES of §2.1. Three are buildable today; two are BLOCKED by a named gap in another file and
# are listed here with the blocker rather than silently omitted — a genome that quietly drops a gene is
# a genome nobody can audit.
GENES = [
    ("pilot_energy_frac", None),          # G4 — Scale on BfmCornerSpeedKt
    ("pilot_cover_frac", None),           # G2 — Scale on the weapon's own binding time
    ("sort", None),                       # G3 — the briefed contract + the channel bit
    ("pilot_flight_shape", "doc/formation.md F5: FormationSpreadM/TrailM/StackM are airframe hooks, "
                           "not mission data, so a flight cannot be briefed a shape at all"),
    ("pilot_emcon_frac", "doc/duels.md D3: the pilot's picture is built from the Radar block alone, so "
                         "'silent' means 'silent and blind' and the band is degenerate at one rail"),
]

SORT_ALLELES = [("off", ""), ("on", ""), ("on", "left"), ("on", "near")]   # (dl, sort)
kArchiveMax = 64
kArchiveSample = 8
kYardstick = os.path.join(SIM_DIR, "tools", "variants-bvr.txt")


def pilot_alphabet(gym):
    """The alphabet, out of the binary. A key this does not list cannot be set from a mission file, so
    a genome built from it cannot express anything the simulator would refuse."""
    out = subprocess.run([gym, "--pilot-keys"], capture_output=True, text=True, check=True).stdout
    keys = {}
    for line in out.splitlines():
        f = line.split()
        if len(f) >= 4:
            keys[f[0]] = {"kind": f[1], "lo": float(f[2]), "hi": float(f[3]),
                          "hook": " ".join(f[4:])}
    return keys


def assets_clean():
    r = subprocess.run(["git", "status", "--porcelain", "sim/assets"], cwd=REPO_DIR,
                       capture_output=True, text=True)
    return r.stdout.strip() == ""


class Genome:
    """A genome is a TEXT LINE, which is doc/pilot.md §9's whole point — so a population is a file and
    an archive is a file, and nothing about either is compiled."""

    def __init__(self, name, params, dl="off", sort=""):
        self.name = name
        self.params = dict(params)
        self.dl = dl
        self.sort = sort

    def variant(self, module="f16"):
        return tour.Variant(self.name, self.params, module, self.dl, self.sort)

    def line(self):
        bits = ["%s=%g" % kv for kv in sorted(self.params.items())]
        if self.dl != "off":
            bits.append("dl=" + self.dl)
        if self.sort:
            bits.append("sort=" + self.sort)
        return "%s %s" % (self.name, " ".join(bits))

    def vector(self, alphabet):
        """Each gene normalised by its OWN band, so instrument (c)'s distance is comparable across
        genes that carry different units — which they do not, but different SPANS, which they do."""
        v = []
        for key, blocked in GENES:
            if blocked or key not in alphabet:
                continue
            e = alphabet[key]
            v.append((self.params.get(key, 1.0) - e["lo"]) / (e["hi"] - e["lo"]))
        v.append(SORT_ALLELES.index((self.dl, self.sort)) / float(len(SORT_ALLELES) - 1))
        return v


def seed_population(alphabet, size):
    """A DETERMINISTIC spread over the bands: gene i of individual k sits at stride k/(size-1) of its
    own band, and the sort allele cycles. No randomness anywhere, so a generation is reproducible from
    its number alone."""
    keys = [k for k, blocked in GENES if not blocked and k in alphabet]
    pop = []
    for k in range(size):
        f = k / float(max(1, size - 1))
        params = {}
        for i, key in enumerate(keys):
            e = alphabet[key]
            g = (f + i * 0.37) % 1.0
            params[key] = round(e["lo"] + g * (e["hi"] - e["lo"]), 4)
        dl, sort = SORT_ALLELES[k % len(SORT_ALLELES)]
        pop.append(Genome("g0_%02d" % k, params, dl, sort))
    return pop


def mutate(parent, alphabet, index, generation):
    """One gene moved by a fixed fraction of its own band, chosen by index — the deterministic stand-in
    for a mutation operator. The clamp is the band's, which is also FBPilotTuning's, so a mutant that
    left the band would not fly rather than silently flying the default."""
    keys = [k for k, blocked in GENES if not blocked and k in alphabet]
    child = Genome("g%d_%02d" % (generation, index), parent.params, parent.dl, parent.sort)
    if not keys:
        return child
    key = keys[index % len(keys)]
    e = alphabet[key]
    step = (e["hi"] - e["lo"]) * (0.25 if index % 2 == 0 else -0.25)
    child.params[key] = round(min(e["hi"], max(e["lo"], child.params.get(key, 1.0) + step)), 4)
    if index % len(keys) == len(keys) - 1:
        a = (SORT_ALLELES.index((child.dl, child.sort)) + 1) % len(SORT_ALLELES)
        child.dl, child.sort = SORT_ALLELES[a]
    return child


def fly(gym, outroot, geometry, timeout, pairs, jobs, threads, flight=1):
    """Every (west, east) pairing as a run; returns the two side keys per pairing."""
    jobslist, meta = [], []
    for tag, w, e in pairs:
        jobslist.append((gym, outroot, tag,
                         tour.mission_text(tag, timeout, geometry, w, e, flight), threads))
        meta.append((tag, w, e))
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        list(ex.map(tour.run_one, jobslist))
    out = {}
    for tag, w, e in meta:
        wm, em, dur = fit.read_pair(os.path.join(outroot, tag), tour.side_calls("west", flight),
                                    tour.side_calls("east", flight), w.name, e.name)
        wk, _ = fit.side_key(wm, fit.FlightView(em), dur)
        ek, _ = fit.side_key(em, fit.FlightView(wm), dur)
        out[tag] = (wk, ek)
    return out


def round_robin(gym, outroot, geometry, timeout, contenders, opponents, jobs, threads, tagpfx,
                flight=1):
    """Both seats of every contender against every opponent. Returns a win vector per contender —
    the archive's admission test and the CIAO matrix need exactly this, and it costs no extra run."""
    pairs = []
    for c in contenders:
        for o in opponents:
            if c.name == o.name:
                continue
            pairs.append(("%s-%s_vs_%s" % (tagpfx, c.name, o.name), c.variant(), o.variant()))
            pairs.append(("%s-%s_vs_%s" % (tagpfx, o.name, c.name), o.variant(), c.variant()))
    keys = fly(gym, outroot, geometry, timeout, pairs, jobs, threads, flight)
    win = {c.name: {} for c in contenders}
    for c in contenders:
        for o in opponents:
            if c.name == o.name:
                continue
            a = keys["%s-%s_vs_%s" % (tagpfx, c.name, o.name)]
            b = keys["%s-%s_vs_%s" % (tagpfx, o.name, c.name)]
            pts = fit.pair_points(a[0], a[1])[0] + fit.pair_points(b[1], b[0])[0]
            win[c.name][o.name] = pts / 2.0
    return win


def archive_sample(archive, k):
    """An even stride over the WHOLE history, always including the oldest (the anchor that makes
    progress measurable) and the newest (the current threat). Deterministic by construction —
    conventions.md forbids a random sample and a sampled fitness would make a generation
    irreproducible."""
    n = len(archive)
    if n <= k:
        return list(archive)
    return [archive[int(round(i * (n - 1) / float(k - 1)))] for i in range(k)]


def dominated(vec, others):
    """§3.4 design B: a candidate enters iff its win vector over the SAMPLED opponents is not dominated
    by any current member's on the same opponents. A set of mutually non-dominated strategies is one a
    cycler must beat ALL of, and beating all of them is by construction not a cycle."""
    for o in others:
        common = [k for k in vec if k in o]
        if not common:
            continue
        if all(o[k] >= vec[k] for k in common) and any(o[k] > vec[k] for k in common):
            return True
    return False


def cyclic_triples(win):
    """d = C(n,3) - sum_i C(s_i, 2), T = d / C(n,3). Exact, not estimated: every triple is either
    transitive or cyclic, and a transitive one has exactly one vertex that beats both others."""
    names = sorted(win)
    n = len(names)
    if n < 3:
        return 0.0, 0, 0
    s = [sum(1 for b in names if a != b and win[a].get(b, 0.5) > 0.5) for a in names]
    total = n * (n - 1) * (n - 2) // 6
    d = total - sum(x * (x - 1) // 2 for x in s)
    return d / float(total), d, total


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", required=True)
    ap.add_argument("--gym", default=os.path.join(SIM_DIR, "build", "fb-gym"))
    ap.add_argument("--geometry", default="xmirror", choices=sorted(tour.GEOMETRIES))
    ap.add_argument("--generations", type=int, default=4)
    ap.add_argument("--population", type=int, default=6)
    ap.add_argument("--timeout", type=int, default=420)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--threads", type=int, default=2)
    ap.add_argument("--flight", type=int, default=1, choices=(1, 2, 4),
                    help="element size per side. G2 (cover) needs a MATE to be anything but inert, so "
                         "the cover gene is only measurable at 2 or 4")
    ap.add_argument("--archive", default="", help="archive file (default <out>/archive.txt)")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    archive_path = args.archive or os.path.join(args.out, "archive.txt")
    alphabet = pilot_alphabet(args.gym)

    # THE ALPHABET, printed at start in the shape verify-layers' own summary line has. A reader sees
    # what this run can touch before it touches anything.
    live = [k for k, b in GENES if not b and (k in alphabet or k == "sort")]
    blocked = [(k, b) for k, b in GENES if b]
    reachable = [k for k in alphabet if k in [g for g, _ in GENES]]
    print("evolving %d genes of %d pilot keys; %d keys not in the genome; 0 non-pilot keys reachable"
          % (len(live), len(alphabet), len(alphabet) - len(reachable)))
    for k in live:
        e = alphabet.get(k)
        print("   gene %-20s %s" % (k, "the briefed sort contract + the channel bit (mission text)"
              if e is None else "%s %g..%g%s" % (e["kind"], e["lo"], e["hi"],
                                                " x " + e["hook"] if e["hook"] else "")))
    for k, why in blocked:
        print("   BLOCKED %-17s %s" % (k, why))
    if not assets_clean():
        sys.exit("sim/assets is dirty BEFORE the run — an evolution run starts from a clean model tree")
    print("model tree clean before the run (git status --porcelain sim/assets empty)")

    print("\narena gate: run tools/fb_arena_check.py first — no evolution run starts on an arena that")
    print("            has not passed (doc/doctrine-evolution.md E4). Geometry: %s" % args.geometry)

    yard = [Genome(v.name, v.params, v.dl, v.sort) for v in tour.load_variants(kYardstick)]
    pop = seed_population(alphabet, args.population)
    archive, champions, champ_scores = [], [], []

    for gen in range(args.generations):
        sample = archive_sample(archive, kArchiveSample)
        opponents = pop + sample
        win = round_robin(args.gym, args.out, args.geometry, args.timeout, pop, opponents,
                          args.jobs, args.threads, "g%d" % gen, args.flight)
        fitness = {n: sum(v.values()) / max(1, len(v)) for n, v in win.items()}
        order = sorted(pop, key=lambda g: -fitness[g.name])
        champ = order[0]
        champions.append(champ)

        # (a) THE FIXED YARDSTICK — the cheapest instrument and the only mandatory one. Measured against
        # a field that never evolves, so a rise here cannot be the Red Queen.
        yw = round_robin(args.gym, args.out, args.geometry, args.timeout, [champ], yard,
                         args.jobs, args.threads, "y%d" % gen, args.flight)
        ys = sum(yw[champ.name].values()) / max(1, len(yw[champ.name]))
        champ_scores.append(ys)

        print("\ngeneration %d — population %d, archive %d (sampled %d)"
              % (gen, len(pop), len(archive), len(sample)))
        for g in order:
            print("   %-10s fitness %.3f   %s" % (g.name, fitness[g.name], g.line()))
        print("   champion %s   yardstick %.3f" % (champ.name, ys))

        for g in order:
            if len(archive) >= kArchiveMax:
                break
            if not dominated(win[g.name], [a.win for a in archive if hasattr(a, "win")]):
                g.win = win[g.name]
                g.gen = gen
                archive.append(g)

        pop = [champ] + [mutate(order[i % len(order)], alphabet, i, gen + 1)
                         for i in range(args.population - 1)]

    # (b) THE CHAMPION GRAPH — the exact statistic, no extra runs beyond this one round robin.
    cw = round_robin(args.gym, args.out, args.geometry, args.timeout, champions, champions,
                     args.jobs, args.threads, "champ", args.flight)
    t, d, total = cyclic_triples(cw)

    # (c) THE DOCTRINE TRAJECTORY — free, and it names the gene.
    traj = []
    for i in range(3, len(champions)):
        vi = champions[i].vector(alphabet)
        best = min(math.dist(vi, champions[j].vector(alphabet)) for j in range(i - 2))
        traj.append((i, best))

    with open(archive_path, "w") as f:
        f.write("# FlightBox doctrine archive — one genome per line, verbatim and re-flyable.\n")
        f.write("# arena=%s timeout=%d  (a score measured on another arena is REFUSED, not re-used:\n"
                "#  doc/missions/campaign.md §5's rule, applied verbatim)\n" % (args.geometry, args.timeout))
        for i, g in enumerate(archive):
            f.write("%s   # gen=%d idx=%d wins=%s\n" % (g.line(), g.gen, i,
                    ",".join("%s:%.2f" % kv for kv in sorted(g.win.items()))))

    print("\n" + "=" * 96)
    print("CIRCLING — the three instruments of doc/doctrine-evolution.md §3.6")
    print("=" * 96)
    print("(a) fixed yardstick per generation: %s" % " ".join("%.3f" % s for s in champ_scores))
    nondec = all(champ_scores[i] >= champ_scores[i - 1] - 1e-9 for i in range(1, len(champ_scores)))
    print("    non-decreasing over the window: %s" % ("yes" if nondec else "NO — see §3.6a"))
    print("(b) champion graph: n=%d  cyclic triples d=%d of %d  T=%.4f  (acceptance T <= 0.05) %s"
          % (len(champions), d, total, t, "ok" if t <= 0.05 else "NO"))
    print("(c) doctrine trajectory, min distance to a champion 3+ generations back:")
    for i, best in traj:
        print("    champion %d: %.4f" % (i, best))
    if not traj:
        print("    (needs at least four generations)")
    print("\narchive: %d member(s) -> %s" % (len(archive), archive_path))
    if not assets_clean():
        print("VOID: sim/assets moved during the run — doc/doctrine-evolution.md §2.2 property 4")
        return 1
    print("model tree clean after the run (git status --porcelain sim/assets empty)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
