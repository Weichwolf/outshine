#!/usr/bin/env python3
"""fb_arena_movers — read a campaign-arena channels CSV and report WHICH LEVER FAMILY moves the
outcome class, per cell and in total.

The arena's own report answers "how many levers moved this cell". This answers the question that
decides whether the FIXED FIELD of S1 is stale rather than wrong (doc/doctrine-evolution.md D9): a
lever family that moves the class on N cells is a family a debrief-defensible frozen doctrine could be
written in; a family that moves nothing cannot be rescued by any yardstick.

It reads only what the arena already published. It runs nothing, so it cannot select anything.

    tools/fb_arena_movers.py build/arena-e17-channels.csv [--baseline-of OTHER.csv]

Stdlib only.
"""
import argparse
import collections
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fb_fitness as fit

# The family a lever belongs to is its own name's stem, which is how both lever files are written.
FAMILY = {"cover": "G2 cover", "energy": "G4 energy", "net": "G3 net", "sort": "G3 sort",
          "bias": "G6 bias", "ccip": "G7 ccip"}


def family_of(lever):
    return FAMILY.get(lever.split("-")[0], "?" + lever)


def read(path):
    """(cell, lever) -> outcome class, straight out of the published columns."""
    out = {}
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            key = (int(row["V"]), int(row["M"]), row["C_air"])   # C is never read here: §4.2's
            out[(row["cell"], row["lever"])] = (fit.outcome_class(key), row)  # class is (V, M) alone
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("channels")
    ap.add_argument("--per-cell", action="store_true")
    a = ap.parse_args()

    rows = read(a.channels)
    cells = sorted({c for c, _ in rows})
    levers = sorted({l for _, l in rows if l != "baseline"})

    fam_cells = collections.defaultdict(set)     # family -> cells it moves
    lever_cells = collections.defaultdict(set)   # lever  -> cells it moves
    movers_hist = collections.Counter()
    complete = 0
    for cell in cells:
        base = rows.get((cell, "baseline"))
        have = [l for l in levers if (cell, l) in rows]
        if base is None or len(have) < len(levers):
            continue
        complete += 1
        n = 0
        for l in have:
            if rows[(cell, l)][0] != base[0]:
                n += 1
                fam_cells[family_of(l)].add(cell)
                lever_cells[l].add(cell)
        movers_hist[n] += 1

    print("%d complete cells of %d seen, %d levers\n" % (complete, len(cells), len(levers)))
    print("movers of %d : %s" % (len(levers),
          "  ".join("%d×%d" % (movers_hist[k], k) for k in sorted(movers_hist))))
    print("\n%-12s %8s   %s" % ("family", "cells", "the levers in it, with their own cell count"))
    for fam in sorted(fam_cells, key=lambda f: -len(fam_cells[f])):
        inner = sorted((l for l in lever_cells if family_of(l) == fam),
                       key=lambda l: -len(lever_cells[l]))
        print("%-12s %8d   %s" % (fam, len(fam_cells[fam]),
              ", ".join("%s %d" % (l, len(lever_cells[l])) for l in inner)))
    for fam in sorted(set(FAMILY.values()) - set(fam_cells)):
        print("%-12s %8d   -" % (fam, 0))

    if a.per_cell:
        print()
        for cell in cells:
            base = rows.get((cell, "baseline"))
            if base is None:
                continue
            mv = [l for l in levers if (cell, l) in rows and rows[(cell, l)][0] != base[0]]
            print("%-34s base=%-10s %2d  %s" % (cell, str(base[0]), len(mv), ",".join(mv)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
