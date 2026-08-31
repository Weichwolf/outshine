#!/usr/bin/env python3
"""Score one station table against another, in the shape the Khronos render corpus already uses.

`render_corpus.py` paid for the hard half of this and wrote down what it cost: a completely black
frame scored 57.6729 per cent against an oracle of six lit boxes -- BETTER than the 11.3325 per cent
it scored when it drew the subject in the wrong place. A metric that rewards doing nothing is a
metric being read without its picture, and the fix there was the denominator: every pixel lit in
EITHER image.

The same trap, translated: a case is one ROAD, a pixel is a STATION one metre apart, and the
denominator is every station where EITHER side carries a carriageway. Score only where WE laid road
and laying none scores a hundred per cent.

WHAT IS COMPARED, and every tolerance is declared with an origin rather than tuned until the number
looks good. Only DATUM-FREE quantities: the A9 is surveyed in ETRS89/UTM32 on WGS84 and this tree's
ground comes from its own tile source, so an absolute height difference would measure the geoid
rather than us.

WHAT THIS DOES NOT COVER: lanes, markings, signals and junction connections, all of which the oracle
carries and this tree does not derive; and anywhere that is not this motorway.
"""
import argparse
import json
import math
import os
import pathlib
import sys

TREE = pathlib.Path(__file__).resolve().parents[2]

kNearM = 8.0
kWidthM = 1.875
kGradePerCent = 0.5
kBankPerCent = 0.5
kLeastAgreeing = 0.90
kCellM = 25.0


def prepared_root():
    return pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-prepared" / "opendrive-a9"


def stations(path):
    """road -> [(s, x, y, z, bank, left, right)], and a grid from cell to those rows."""
    by, grid = {}, {}
    for line in path.read_text().splitlines():
        if line.startswith("#"):
            continue
        part = line.split()
        row = (float(part[1]), float(part[2]), float(part[3]), float(part[4]), float(part[5]),
               float(part[6]), float(part[7]))
        by.setdefault(part[0], []).append(row)
        grid.setdefault((int(row[1] // kCellM), int(row[2] // kCellM)), []).append(row)
    return by, grid


def nearest(grid, x, y):
    best, near = None, kNearM
    cx, cy = int(x // kCellM), int(y // kCellM)
    for ox in (-1, 0, 1):
        for oy in (-1, 0, 1):
            for row in grid.get((cx + ox, cy + oy), ()):
                apart = math.hypot(row[1] - x, row[2] - y)
                if apart < near:
                    near, best = apart, row
    return best


def gradient(rows, at):
    if at + 1 >= len(rows):
        return 0.0
    return (rows[at + 1][3] - rows[at][3]) / max(1.0e-6, rows[at + 1][0] - rows[at][0]) * 100.0


def agrees(mine, theirs, atMine, atTheirs, minesRows, theirsRows):
    if abs((mine[5] + mine[6]) - (theirs[5] + theirs[6])) > kWidthM:
        return False
    if abs(gradient(minesRows, atMine) - gradient(theirsRows, atTheirs)) > kGradePerCent:
        return False
    return abs(math.tan(mine[4]) - math.tan(theirs[4])) * 100.0 <= kBankPerCent


def score(oursPath, theirsPath):
    ours, ourGrid = stations(oursPath)
    theirs, theirGrid = stations(theirsPath)
    theirIndex = {}
    for road, rows in theirs.items():
        for at, row in enumerate(rows):
            theirIndex[(round(row[1], 3), round(row[2], 3))] = (road, at, rows)

    held, apart, scored = 0, [], 0
    for road, rows in sorted(ours.items()):
        lit, agree, worst, worstAt = 0, 0, 0.0, 0.0
        for at, row in enumerate(rows):
            found = nearest(theirGrid, row[1], row[2])
            lit += 1
            if found is None:
                continue
            key = (round(found[1], 3), round(found[2], 3))
            theirRoad, theirAt, theirRows = theirIndex[key]
            if agrees(row, found, at, theirAt, rows, theirRows):
                agree += 1
            else:
                off = abs((row[5] + row[6]) - (found[5] + found[6]))
                if off > worst:
                    worst, worstAt = off, row[0]
        if lit == 0:
            continue
        scored += 1
        share = agree / lit
        if share >= kLeastAgreeing:
            held += 1
        else:
            apart.append((road, share, worst, worstAt))
    return held, apart, scored


def main():
    ask = argparse.ArgumentParser()
    ask.add_argument("--ours")
    ask.add_argument("--theirs")
    told = ask.parse_args()

    root = prepared_root()
    if not (root / "2017-04-04_Testfeld_A9_Nord.stations").exists():
        print("UNPREPARED: run test/scripts/fetch_opendrive_a9.py then opendrive_oracle.py")
        return 1
    stated = json.loads((TREE / "test" / "opendrive" / "a9" / "manifest.json").read_text())
    print(f"{stated['title']} -- grade {stated['grade']}, {stated['source']['licence']}")

    ours = pathlib.Path(told.ours) if told.ours else root / "2017-04-04_Testfeld_A9_Nord.stations"
    them = pathlib.Path(told.theirs) if told.theirs else root / "2017-04-04_Testfeld_A9_Nord.stations"

    held, apart, scored = score(ours, them)
    for road, share, worst, at in apart[:8]:
        print(f"APART road {road:>10s} {share * 100:8.4f}%  widest {worst:6.2f} m at s={at:.0f}")
    print(f"\n{held} of {scored} road(s) held at {kLeastAgreeing * 100:.0f}% of the stations "
          f"EITHER side carries")
    print(f"tolerances: width {kWidthM} m (half a lane of 3.75), gradient {kGradePerCent} per cent, "
          f"superelevation {kBankPerCent} per cent")
    print("the denominator is the UNION: a station one side carries and the other does not counts "
          "AGAINST, because a metric that rewards laying nothing is one being read without its "
          "picture (render_corpus.py, PointLightIntensityTest)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
