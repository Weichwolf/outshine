#!/usr/bin/env python3
"""fb_delivery_stats — read every `stores DELIVERY` out of two regression snapshots and compare the two
distributions of ALONG-TRACK error.

Why a distribution and not a per-cell verdict: the release clock is quantised at the pilot's decision
tick (doc/doctrine-evolution.md X-3), so the reachable release points sit on a lattice ~23 m apart at
strike speed. A correction to the release LEAD moves every delivery by one lattice step, which makes
some cells better and some worse for reasons that have nothing to do with the correction being right.
The claim a lead fix can honestly make is about the MEAN (the bias it removes) and about the SPREAD
(which it must NOT change, because the lattice is untouched).

Pairs by (mission, ordinal of the delivery within the mission) so the two snapshots are compared
delivery for delivery and an added or dropped release is reported rather than silently realigned.

Usage: tools/fb_delivery_stats.py <snapshotA> <snapshotB>
Stdlib only; reads `events.norm`, which fb_regress.sh keeps in full.
"""
import os
import re
import sys

kField = re.compile(r"(\w+)=(-?[0-9.eE+]+)")


def deliveries(root):
    """{(mission, i) -> {field: float}} for every `stores DELIVERY` line, in file order."""
    out = {}
    for name in sorted(os.listdir(root)):
        path = os.path.join(root, name, "events.norm")
        if not os.path.isfile(path):
            continue
        i = 0
        with open(path, errors="replace") as f:
            for line in f:
                if " stores DELIVERY " not in line:
                    continue
                out[(name, i)] = {k: float(v) for k, v in kField.findall(line)}
                i += 1
    return out


def stats(xs):
    n = len(xs)
    if not n:
        return 0, 0.0, 0.0, 0.0, 0.0
    m = sum(xs) / n
    sd = (sum((x - m) ** 2 for x in xs) / n) ** 0.5 if n > 1 else 0.0
    s = sorted(xs)
    return n, m, sd, s[0], s[-1]


def line(tag, xs):
    n, m, sd, lo, hi = stats(xs)
    return "%-10s n=%-4d Mittel %+8.2f m   Streuung %7.2f m   Spanne %+8.2f .. %+8.2f m" % (
        tag, n, m, sd, lo, hi)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__.strip().splitlines()[-2])
    a, b = deliveries(sys.argv[1]), deliveries(sys.argv[2])
    only_a = sorted(set(a) - set(b))
    only_b = sorted(set(b) - set(a))
    both = sorted(set(a) & set(b))
    if only_a or only_b:
        print("ABWEICHENDE ABWURFZAHL — nicht paarbar, deshalb zuerst genannt:")
        for k in only_a:
            print("   nur in A: %s #%d" % k)
        for k in only_b:
            print("   nur in B: %s #%d" % k)
        print()

    for field, what in (("aimLongM", "LAENGS zum Zielpunkt (+ = zu lang)"),
                        ("aimAcrossM", "QUER zum Zielpunkt"),
                        ("aimErrM", "Betrag des Zielfehlers"),
                        ("predErrM", "Fehler des RECHNERS (C28)")):
        xa = [a[k][field] for k in both if field in a[k]]
        xb = [b[k][field] for k in both if field in b[k]]
        if not xa:
            continue
        print("== %s (%s)" % (field, what))
        print("   " + line("A", xa))
        print("   " + line("B", xb))
        na, ma, sda, _, _ = stats(xa)
        nb, mb, sdb, _, _ = stats(xb)
        print("   %-10s Mittel %+8.2f m      Streuung %+7.2f m" % ("DIFFERENZ", mb - ma, sdb - sda))
        print()

    moved = [k for k in both if abs(a[k].get("aimLongM", 0) - b[k].get("aimLongM", 0)) > 1e-6]
    better = sum(1 for k in moved if abs(b[k]["aimErrM"]) < abs(a[k]["aimErrM"]))
    print("Bewegte Abwuerfe: %d von %d   davon naeher am Ziel: %d, weiter weg: %d"
          % (len(moved), len(both), better, len(moved) - better))
    return 0


if __name__ == "__main__":
    sys.exit(main())
