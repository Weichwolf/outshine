#!/usr/bin/env python3
"""Flatten the A9 OpenDRIVE corpus into a STATION TABLE, once.

ASAM OpenDRIVE states a road as four profiles over the arc length `s`: the reference line in plan
(line, arc, spiral or a parametric cubic), the elevation, the superelevation, and the lane widths.
Each is a piecewise polynomial, so asking "what does the surveyed road do at this point" means
evaluating four polynomial sequences -- and doing that inside a scoring run would make the score a
program nobody wants to run twice.

So it is done ONCE. Every road is walked at a fixed step and written as plain rows, and everything
downstream reads rows. A row is what the MEASUREMENT says at that station; what this tree derives
from OpenStreetMap is scored against it in `score_opendrive_a9.py`.

The A9 files are `paramPoly3` throughout with `pRange="arcLength"`, so the plan view evaluates as
two cubics in the arc length itself rather than in a normalised parameter -- the one case where the
parameter and `s` agree, which is why no arc-length reparameterisation appears here. A file that
used `normalized` would need one, and this refuses rather than guessing.

WHAT IT DOES NOT CARRY: `laneOffset` (2 299 records), lane markings, signals, objects and junction
connections. The first shifts the reference line inside the carriageway and matters for a lateral
comparison at the decimetre; the rest are their own items. Stated here because a table that is
silent about what it dropped gets read as complete.
"""
import json
import math
import os
import pathlib
import re
import sys

kStepM = 1.0
kDriving = ("driving", "entry", "exit")


def prepared_root():
    return pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-prepared" / "opendrive-a9"


def cubics(text, tag, at="s"):
    out = []
    for m in re.finditer(rf'<{tag} {at}="([^"]*)" a="([^"]*)" b="([^"]*)" c="([^"]*)" d="([^"]*)"',
                         text):
        out.append(tuple(float(m.group(i)) for i in range(1, 6)))
    out.sort(key=lambda row: row[0])
    return out


def poly(rows, s, fallback=0.0):
    if not rows:
        return fallback
    picked = rows[0]
    for row in rows:
        if row[0] <= s:
            picked = row
        else:
            break
    d = s - picked[0]
    return picked[1] + picked[2] * d + picked[3] * d * d + picked[4] * d * d * d


def plan(text):
    laid = []
    for m in re.finditer(
            r'<geometry s="([^"]*)" x="([^"]*)" y="([^"]*)" hdg="([^"]*)" length="([^"]*)">\s*'
            r'<(\w+)([^/]*)/>', text):
        shape = m.group(6)
        if shape != "paramPoly3":
            return None, shape
        args = dict(re.findall(r'(\w+)="([^"]*)"', m.group(7)))
        if args.get("pRange") != "arcLength":
            return None, "pRange=" + str(args.get("pRange"))
        laid.append((float(m.group(1)), float(m.group(2)), float(m.group(3)), float(m.group(4)),
                     float(m.group(5)),
                     tuple(float(args[k]) for k in ("aU", "bU", "cU", "dU")),
                     tuple(float(args[k]) for k in ("aV", "bV", "cV", "dV"))))
    laid.sort(key=lambda row: row[0])
    return laid, None


def at(laid, s):
    picked = laid[0]
    for row in laid:
        if row[0] <= s:
            picked = row
        else:
            break
    p = s - picked[0]
    u = picked[5][0] + picked[5][1] * p + picked[5][2] * p * p + picked[5][3] * p * p * p
    v = picked[6][0] + picked[6][1] * p + picked[6][2] * p * p + picked[6][3] * p * p * p
    cos, sin = math.cos(picked[3]), math.sin(picked[3])
    return picked[1] + u * cos - v * sin, picked[2] + u * sin + v * cos


def widths(text):
    """Every laneSection as (s, {'left': [(type, cubics)], 'right': [...]})."""
    out = []
    for m in re.finditer(r'<laneSection s="([^"]*)"[^>]*>(.*?)</laneSection>', text, re.S):
        sides = {}
        for side in ("left", "right"):
            block = re.search(rf'<{side}>(.*?)</{side}>', m.group(2), re.S)
            lanes = []
            if block:
                for lane in re.finditer(r'<lane id="[^"]*" type="([^"]*)"[^>]*>(.*?)</lane>',
                                        block.group(1), re.S):
                    lanes.append((lane.group(1), cubics(lane.group(2), "width", "sOffset")))
            sides[side] = lanes
        out.append((float(m.group(1)), sides))
    out.sort(key=lambda row: row[0])
    return out


def carriageway(sections, s):
    if not sections:
        return 0.0, 0.0
    picked = sections[0]
    for row in sections:
        if row[0] <= s:
            picked = row
        else:
            break
    wide = {}
    for side in ("left", "right"):
        total = 0.0
        for kind, rows in picked[1][side]:
            if kind in kDriving:
                total += max(0.0, poly(rows, s - picked[0]))
        wide[side] = total
    return wide["left"], wide["right"]


def flatten(path, into):
    text = path.read_text(encoding="utf-8", errors="replace")
    rows, roads, refused = 0, 0, []
    with into.open("w") as out:
        out.write("# road s x y z bank leftM rightM\n")
        for m in re.finditer(r'<road name="[^"]*" length="([^"]*)" id="([^"]*)"[^>]*>(.*?)</road>',
                             text, re.S):
            length, road, body = float(m.group(1)), m.group(2), m.group(3)
            laid, why = plan(body)
            if laid is None:
                refused.append((road, why))
                continue
            rise = cubics(body, "elevation")
            bank = cubics(body, "superelevation")
            sections = widths(body)
            roads += 1
            s = 0.0
            while s <= length:
                x, y = at(laid, s)
                left, right = carriageway(sections, s)
                out.write(f"{road} {s:.1f} {x:.3f} {y:.3f} {poly(rise, s):.3f} "
                          f"{poly(bank, s):.6f} {left:.3f} {right:.3f}\n")
                rows += 1
                s += kStepM
    return roads, rows, refused


def main():
    into = prepared_root()
    if not into.exists():
        print("UNPREPARED: run test/scripts/fetch_opendrive_a9.py")
        return 1
    total = 0
    for name in sorted(p.name for p in into.glob("*.xodr")):
        table = into / (name.replace(".xodr", ".stations"))
        roads, rows, refused = flatten(into / name, table)
        print(f"{name}: {roads} road(s), {rows} station(s) at {kStepM:.1f} m -> {table.name}")
        for road, why in refused:
            print(f"  REFUSED road {road}: {why}")
        total += rows
    print(f"{total} station(s) in all")
    return 0


if __name__ == "__main__":
    sys.exit(main())
