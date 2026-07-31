#!/usr/bin/env python3
"""fb_champion_audit — X4 of doc/doctrine-evolution.md §5, applied to a champion before anything about
it is published. Two of the three detectors need runs and this file drives them:

  A LUCKY TRAJECTORY dressed as a doctrine   the spawn longitude of every unit of the graded side is
                                             perturbed in 0.8 m steps over +-3 m, 8 samples. The
                                             champion's OUTCOME CLASS must survive. [MESS, pilot.md]
                                             the yardstick itself flips in 2 of 8 in a chaotic
                                             geometry — and if it does, no claim may be made there.
  JUDGE EVASION                              the same cell re-flown with `timeout` x 1.5. An advantage
                                             that evaporates was an advantage against the CLOCK.

The third detector (a partition exploit) needs no run: §1.3 contains no such count, and the report
below prints which counts moved so a reader can check rather than trust.

The committed missions are never written — every splice goes into --out, and the tree is checked clean
before and after, exactly as the arena does it.

Stdlib only, one dependency: build/fb-gym.
"""
import argparse
import collections
import csv
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fb_campaign_arena as arena
import fb_fitness as fit
import fb_tournament as tour

kSteps = [-3.0, -2.2, -1.4, -0.6, 0.6, 1.4, 2.2, 3.0]   # metres, the 0.8 m grid of pilot.md


def shift_spawn(text, team, module, metres):
    """Every spawn of the graded side moved EAST by `metres`, in the file's own units. 1 deg lon at
    the mission's own latitude is read per line, so the shift is metres and not degrees."""
    out, blk_team, blk_mod = [], "", ""
    for raw in text.splitlines(True):
        t = raw.split("#", 1)[0].split()
        if t[:1] == ["unit"]:
            blk_team = blk_mod = ""
        elif len(t) > 1 and t[0] == "team":
            blk_team = t[1]
        elif len(t) > 1 and t[0] == "module":
            blk_mod = t[1]
        elif len(t) > 2 and t[0] == "spawn" and blk_team == team and blk_mod == module:
            lat, lon = float(t[1]), float(t[2])
            import math
            dlon = metres / (111320.0 * max(1e-6, math.cos(math.radians(lat))))
            t[2] = "%.8f" % (lon + dlon)
            raw = "  " + " ".join(t) + "\n"
        out.append(raw)
    return "".join(out)


def scale_timeout(text, factor):
    out = []
    for raw in text.splitlines(True):
        t = raw.split("#", 1)[0].split()
        if t[:1] == ["timeout"] and len(t) > 1:
            raw = "timeout %g\n" % (float(t[1]) * factor)
        out.append(raw)
    return "".join(out)


def variant_from_line(line):
    """The champion's own genome line, parsed by the SAME reader the arena uses — a second parser
    would be a second dialect."""
    tmp = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".champion.tmp")
    with open(tmp, "w") as f:
        f.write("baseline\nchampion " + line + "\n")   # der Leser verlangt zwei Zeilen
    v = arena.load_levers(tmp)[1]
    os.remove(tmp)
    return v


def fly_variants(gym, out, cell, variant, texts):
    """texts: {tag -> mission text}. Each becomes a cell of its own pointing at a written copy, so the
    committed file is never touched and `fly` needs no change."""
    md = os.path.join(out, "missions")
    os.makedirs(md, exist_ok=True)
    cells = []
    for tag, text in texts.items():
        path = os.path.join(md, tag + ".fbm")
        with open(path, "w") as f:
            f.write(text)
        c = arena.Cell(cell.mission, cell.team, cell.module)
        c.mission, c.path = tag, path
        cells.append(c)
    got = arena.fly(gym, out, cells, [variant], 6, 1, "const")
    return {c.mission: got[(c.name, variant.name)][0] for c in cells}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--cells", required=True)
    ap.add_argument("--genome", required=True, help="the champion's own line, verbatim")
    ap.add_argument("--out", default="build/champion-audit")
    ap.add_argument("--gym", default="build/fb-gym")
    a = ap.parse_args()

    if not arena.tree_clean():
        sys.exit("sim/missions or sim/assets is dirty BEFORE the run")
    cells = arena.load_cells(a.cells)
    variant = variant_from_line(a.genome)
    os.makedirs(a.out, exist_ok=True)

    print("X4.1 A LUCKY TRAJECTORY — spawn longitude of the graded side, 8 samples over +-3 m\n")
    verdict = True
    for c in cells:
        text = open(c.path).read()
        texts = {"%s-base" % c.mission: text}
        for i, m in enumerate(kSteps):
            texts["%s-p%d" % (c.mission, i)] = shift_spawn(text, c.team, c.module, m)
        keys = fly_variants(a.gym, a.out, c, variant, texts)
        bcls = fit.outcome_class(keys["%s-base" % c.mission])
        flips = []
        for i, m in enumerate(kSteps):
            k = keys["%s-p%d" % (c.mission, i)]
            if fit.outcome_class(k) != bcls:
                flips.append("%+.1f m -> %s" % (m, fit.outcome_class(k)))
        ok = not flips
        verdict = verdict and ok
        print("   %-26s base %-9s  %d of 8 flipped  %s   %s"
              % (c.mission, str(bcls), len(flips), "ok" if ok else "NO", "; ".join(flips)))

    print("\nX4.2 JUDGE EVASION — the same cells at timeout x 1.5\n")
    for c in cells:
        text = open(c.path).read()
        keys = fly_variants(a.gym, a.out, c, variant,
                            {"%s-t1" % c.mission: text,
                             "%s-t15" % c.mission: scale_timeout(text, 1.5)})
        base, long = keys["%s-t1" % c.mission], keys["%s-t15" % c.mission]
        same = fit.outcome_class(base) == fit.outcome_class(long)
        verdict = verdict and same
        print("   %-26s %-9s -> %-9s  %s"
              % (c.mission, str(fit.outcome_class(base)), str(fit.outcome_class(long)),
                 "held" if same else "MOVED — the advantage was against the CLOCK"))

    if not arena.tree_clean():
        print("\nVOID: sim/missions or sim/assets moved during the run")
        return 1
    print("\nX4: %s   (tree clean before and after)" % ("PASSED" if verdict else "FAILED"))
    return 0 if verdict else 1


if __name__ == "__main__":
    sys.exit(main())
