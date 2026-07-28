#!/usr/bin/env python3
"""FlightBox — THE ARENA GATE. doc/doctrine-evolution.md §4: no evolution run starts on a saturated
arena, and this file is what "saturated" means as a number.

    tools/fb_arena_check.py --out /tmp/arena [--geometry NAME ...] [--timeout 420]

A saturated arena is not a small defect of an attribution test, it is the ABSENCE of the signal
selection would act on: §4.1's measured case is 18 of 19 runs returning the identical outcome, so deck
spread measures nothing, doctrine spread measures nothing, and an evolutionary run on it optimises
noise. The criterion is three per geometry and three per arena:

  S1  the runs do not all land in the same outcome class: >= 2 distinct (V, M), modal class <= 60 %
  S2  the geometry separates DOCTRINES: >= 3 of the 9 levers change the outcome class from baseline
  S3  the result is not a measurement of our own model: band_deck <= 0.25 * band_doctrine
  S4  >= 6 geometries          S5  >= 3 informative          S6  no two informative ones ask the same
                                                                 question (identical class vector)

S3 IS NOT COMPUTABLE ON THIS ARENA, and that is a property of the arena rather than of this file. Its
instrument (doc/modules/air/module.md §Spec 11) perturbs a GENERATED catalogue deck through
tools/gen_air_decks.py; the two aircraft this arena flies are the F-16 and the MiG-29, whose decks are
FlightBox's read-only model copies under CLAUDE.md principle 1 and have no declared-ignorance band to
perturb. The check therefore prints `n/a` with this reason and never a zero — a no-op is not a
measurement. S3 stays live for a catalogue-row arena, where it was defined.

The outcome class is fb_fitness's (V, M) — the two levels that DECIDE. Craft is deliberately not in it:
a criterion built on craft would call an arena informative for the same reason a saturated one produces
a ranking, which is the error the whole file exists to catch.

Stdlib only, no build target, one dependency: build/fb-gym.
"""

import argparse
import collections
import concurrent.futures
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fb_fitness as fit
import fb_tournament as tour

SIM_DIR = tour.SIM_DIR

# The FIXED yardstick of §3.6a — `tools/variants-bvr.txt`'s six lines, which predate all of this and
# never evolve. An arena is judged against a field that cannot have been fitted to it.
YARDSTICK = os.path.join(SIM_DIR, "tools", "variants-bvr.txt")

kModalMax = 0.60      # [SET] a binary question is most informative at 50/50; 60 % admits a real
                      # asymmetry and excludes the measured 18/19 = 94.7 % by 35 points
kMoversMin = 3        # [SET] with three, the band survives losing any one to a fix
kGeometriesMin = 6    # [DERIVED] 50 % measured informative rate x 3 required informative
kInformativeMin = 3


def run_field(gym, outroot, geometry, timeout, variants, jobs, threads):
    """Every unordered pair of the yardstick, both seats. Returns one (V, M) class per SIDE per run —
    60 samples for six variants, which is the population S1 counts a modal share in."""
    jobslist, meta = [], []
    for i in range(len(variants)):
        for j in range(i + 1, len(variants)):
            for w, e in ((variants[i], variants[j]), (variants[j], variants[i])):
                tag = "%s-%s_vs_%s" % (geometry, w.name, e.name)
                jobslist.append((gym, outroot, tag,
                                 tour.mission_text(tag, timeout, geometry, w, e, 1), threads))
                meta.append((tag, w, e))
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        list(ex.map(tour.run_one, jobslist))
    classes = []
    for tag, w, e in meta:
        wm, em, dur = fit.read_pair(os.path.join(outroot, tag), ["west"], ["east"], w.name, e.name)
        wk, _ = fit.side_key(wm, fit.FlightView(em), dur)
        ek, _ = fit.side_key(em, fit.FlightView(wm), dur)
        classes.append((tag, w.name, fit.outcome_class(wk)))
        classes.append((tag, e.name, fit.outcome_class(ek)))
    return classes


def run_levers(gym, outroot, geometry, timeout, jobs, threads, levers=None):
    """The doctrine levers on the WEST seat against a baseline east, on this geometry. By default the
    nine the attribution instrument uses (fb_tournament.DOCTRINE_VARIANTS), so a geometry is tested
    with the levers the tree already declared.

    S2 MEASURES SENSITIVITY TO THE LEVER SET IT SWEEPS, and that is not automatically the GENOME:
    [MESS] on `xmirror` at --flight 2 the four gene alleles pilot_cover_frac 0/3 and
    pilot_energy_frac 0.7/1.2 produce the IDENTICAL key in 12 of 12 runs, on a geometry that passes S2
    with 5 of 9. Pass `--levers <variants file>` to sweep the alleles that are actually being evolved;
    doc/doctrine-evolution.md books the asymmetry as a gap rather than hiding it."""
    base = tour.Variant("baseline", {})
    jobslist, meta = [], []
    for name, params in (levers if levers is not None else tour.DOCTRINE_VARIANTS):
        v = params if isinstance(params, tour.Variant) else tour.Variant(name, dict(params))
        tag = "%s-lever-%s" % (geometry, name)
        jobslist.append((gym, outroot, tag, tour.mission_text(tag, timeout, geometry, v, base, 1),
                         threads))
        meta.append((tag, name, v))
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        list(ex.map(tour.run_one, jobslist))
    out = []
    for tag, name, v in meta:
        wm, em, dur = fit.read_pair(os.path.join(outroot, tag), ["west"], ["east"], v.name, "baseline")
        wk, _ = fit.side_key(wm, fit.FlightView(em), dur)
        out.append((name, fit.outcome_class(wk)))
    return out


def check_geometry(gym, outroot, geometry, timeout, variants, jobs, threads, leverset=None):
    classes = run_field(gym, outroot, geometry, timeout, variants, jobs, threads)
    levers = run_levers(gym, outroot, geometry, timeout, jobs, threads, leverset)

    counts = collections.Counter(c for _, _, c in classes)
    modal_class, modal_n = counts.most_common(1)[0]
    modal = modal_n / float(len(classes))
    s1 = len(counts) >= 2 and modal <= kModalMax

    base_class = dict(levers)["baseline"]
    movers = [n for n, c in levers if n != "baseline" and c != base_class]
    s2 = len(movers) >= kMoversMin

    return {
        "name": geometry, "classes": classes, "counts": counts, "modal": modal,
        "modal_class": modal_class, "distinct": len(counts), "levers": levers,
        "movers": movers, "s1": s1, "s2": s2, "s3": None,
        "informative": s1 and s2,
        "vector": tuple(c for _, _, c in classes),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", required=True)
    ap.add_argument("--gym", default=os.path.join(SIM_DIR, "build", "fb-gym"))
    ap.add_argument("--geometry", action="append", default=[],
                    help="restrict to these geometries (repeatable; default all of fb_tournament's)")
    ap.add_argument("--variants", default=YARDSTICK, help="the FIXED yardstick field")
    ap.add_argument("--levers", default="",
                    help="variants file to sweep for S2 instead of the nine declared doctrine levers "
                         "— use it to test the geometry against the GENOME actually being evolved")
    ap.add_argument("--timeout", type=int, default=420)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--threads", type=int, default=2)
    args = ap.parse_args()

    variants = tour.load_variants(args.variants)
    geoms = args.geometry or sorted(tour.GEOMETRIES)
    os.makedirs(args.out, exist_ok=True)

    print("arena check: %d geometries x (%d yardstick runs + %d lever runs), timeout %ds"
          % (len(geoms), len(variants) * (len(variants) - 1), len(tour.DOCTRINE_VARIANTS), args.timeout))
    leverset = None
    if args.levers:
        lv = tour.load_variants(args.levers)
        leverset = [("baseline", tour.Variant("baseline", {}))] + [(v.name, v) for v in lv]
    res = [check_geometry(args.gym, args.out, g, args.timeout, variants, args.jobs, args.threads,
                          leverset) for g in geoms]

    print("\n" + "=" * 104)
    print("PER GEOMETRY — the outcome class is (V, M): the two levels of the fitness that DECIDE")
    print("=" * 104)
    print("%-10s %8s %9s %-12s %8s %-34s %s" %
          ("geometry", "distinct", "modal", "modal class", "movers", "of 9", "S1 S2 S3"))
    for r in res:
        print("%-10s %8d %8.1f%% %-12s %8d %-34s %s %s %s" %
              (r["name"], r["distinct"], 100 * r["modal"], str(r["modal_class"]), len(r["movers"]),
               ",".join(r["movers"])[:34] or "-",
               "ok" if r["s1"] else "NO", "ok" if r["s2"] else "NO", "n/a"))

    informative = [r for r in res if r["informative"]]
    seen, dupes = {}, []
    for r in informative:
        if r["vector"] in seen:
            dupes.append((seen[r["vector"]], r["name"]))
        seen[r["vector"]] = r["name"]

    print("\nS4 size        : %d geometries          (>= %d) %s"
          % (len(res), kGeometriesMin, "ok" if len(res) >= kGeometriesMin else "NO"))
    print("S5 yield       : %d informative          (>= %d) %s   [%s]"
          % (len(informative), kInformativeMin,
             "ok" if len(informative) >= kInformativeMin else "NO",
             ", ".join(r["name"] for r in informative) or "-"))
    print("S6 distinctness: %d identical pair(s)                  %s   %s"
          % (len(dupes), "ok" if not dupes else "NO", dupes or ""))
    print("S3             : n/a — this arena flies the F-16 and the MiG-29, whose decks are")
    print("                 FlightBox's read-only model copies (CLAUDE.md principle 1). There is no")
    print("                 declared-ignorance band to perturb, so no number is produced for it.")

    ok = (len(res) >= kGeometriesMin and len(informative) >= kInformativeMin and not dupes)
    print("\nARENA: %s   (%d geometries, %d informative, modal <= %.0f%% on every informative one)"
          % ("PASSED" if ok else "REFUSED", len(res), len(informative), 100 * kModalMax))
    if not ok:
        print("       No evolution run starts on it. doc/doctrine-evolution.md §4, criterion E4.")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
