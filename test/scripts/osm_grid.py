#!/usr/bin/env python3
"""Render the synthetic OSM grid and score each cell against a pinned digest.

The Khronos render corpus grades a picture against a picture somebody else computed. A synthetic
grid has no such neighbour, so it grades against ITSELF ACROSS TIME: a cell is HELD when its digest
matches the one pinned beside it, and the count HELD may only rise. That is agreement rather than
correctness, and it is stated here so nobody reads it as more -- the CORRECTNESS in this corpus
comes from the arithmetic oracles, which check the geometry against the terrain function and the
design standards rather than against a previous run.

The terrain is DECLARED (`<world><relief/></world>`), so a cell needs no network and no tile cache
to be right: the ground is a function of place and the correct height is known everywhere.

WHAT THIS DOES NOT COVER: a digest is agreement with a previous run and NEVER correctness. It says
a cell moved, not that it is right. What makes a cell right is a person looking at its picture, and
the arithmetic oracles that are still to come.
The ORACLES below are arithmetic and none of them is a digest. A digest says a cell moved; an oracle
says a cell is wrong. Their thresholds carry their derivation where they stand.
"""
import argparse
import json
import os
import pathlib
import math
import subprocess
import sys

TREE = pathlib.Path(__file__).resolve().parents[2]
CLIENT = TREE / "build" / "outshine-client"
kAtLat = 49.0
kAtLon = 9.0


kMetresPerDegLat = 111320.0


def _at(eastM, northM):
    lat = kAtLat + northM / kMetresPerDegLat
    lon = kAtLon + eastM / (kMetresPerDegLat * math.cos(math.radians(kAtLat)))
    return f"{lat:.7f},{lon:.7f}"


def _part(one):
    said = "      <area" if one["family"] == "area" else "      <way"
    said += f" kind=\"{one['kind']}\""
    if one.get("widthM"):
        said += f" widthM=\"{one['widthM']}\""
    if one.get("heightM"):
        said += f" heightM=\"{one['heightM']}\""
    if one.get("bridge"):
        said += " bridge=\"yes\""
    if one.get("tunnel"):
        said += " tunnel=\"yes\""
    if one.get("level"):
        said += f" level=\"{one['level']}\""
    points = " ".join(_at(e, n) for e, n in one["points"])
    return said + f" points=\"{points}\"/>"


def scenario_for(land, build=None):
    osm = ""
    if build and build.get("parts"):
        osm = "\n    <osm>\n" + "\n".join(_part(one) for one in build["parts"]) + "\n    </osm>"
    return f"""<scenario>
  <world lat="{kAtLat}" lon="{kAtLon}" patienceS="6" sightM="4000">
    <relief kind="{land['kind']}" amplitudeM="{land['amplitudeM']}" \
wavelengthM="{land['wavelengthM']}" gradient="{land['gradient']}" bearingDeg="0" seed="1"/>{osm}
  </world>
  <render widthPx="1280" heightPx="720" fps="60" fill="0.6" audits="no"/>
  <clock start="2026-06-21T11:34:00Z" rate="1" live="no"/>
  <lighting>
    <key lux="0" elevationDeg="0" bearingDeg="0"/>
  </lighting>
  <views>
    <view id="station" person="first" fovDeg="62">
    <at lat="{kAtLat - 0.00108}" lon="{kAtLon - 0.00164}" heightM="105" samplesHeight="yes" \
bearingDeg="45" pitchDeg="-32"/>
    </view>
  </views>
</scenario>
"""


CEILINGS = [
    ("streets: the deepest the ground stands over one", 3.0,
     "no ground stands over a carriageway. The measure grids at 4 m, so on the steepest declared "
     "terrain (30 %) up to 1.2 m of any reading is the grid rather than burial; 3.0 m leaves that "
     "room and still fails the 43.5 m this corpus found on its first pass."),
    ("streets: ways it refused", 0.0, "a declared way is laid or the refusal is a defect"),
    ("streets: pieces the sweep could not lay", 0.0, "every piece of every laid way is swept"),
    ("streets: features no rule named", 0.0, "the corpus declares only kinds the table names"),
    ("streets: ends STILL crossing, the cap bit", 0.0, "no way end is left crossing another"),
    ("ground: yields the budget REFUSED", 0.0,
     "a cell is small enough that the frame budget never has to refuse a corridor"),
    ("ground: footprint corners NO ground vertex shares", 0.0,
     "the ground and the infrastructure are ONE body where they touch. Every corner of a "
     "carriageway that RESTS on the ground is a vertex the ground mesh also carries, so there is "
     "no sliver to fall through and nothing for a triangle to cross. A bridge rests only at its "
     "abutments and only those are counted for it."),
]

FLOORS = [
    ("streets: ways the GROUND carries instead", 1.0, "rests",
     "a carriageway AT GRADE is ground that has been classified and pulled flat. If a cell that "
     "declares one carries none, the ground is not doing the job and something else is."),
    ("streets: vertices compared", 1.0, "floats",
     "a cell that declares a BRIDGE must draw one. Only what floats keeps a body of its own, so "
     "this is the floor that stops an empty span from passing every ceiling for the wrong reason."),
]


def lays_a_road(build):
    for one in build.get("parts", []):
        if one["family"] != "way" or one.get("tunnel") or one["kind"] == "water":
            continue
        if not one.get("bridge"):
            return True
    return False


def flies_a_span(build):
    return any(one.get("bridge") for one in build.get("parts", []))


ORACLES = CEILINGS + FLOORS


def measures_of(lines):
    out = {}
    for line in lines:
        held = line.strip()
        for what, _, _ in ORACLES:
            if held.startswith(what):
                tail = held[len(what):].split()
                if tail:
                    try:
                        out[what] = float(tail[0])
                    except ValueError:
                        pass
    return out


def main():
    ask = argparse.ArgumentParser()
    ask.add_argument("--pin", action="store_true", help="write the digests instead of scoring them")
    ask.add_argument("--only", default="", help="render one cell, named structure-terrain")
    told = ask.parse_args()

    grid = json.loads((TREE / "test" / "outshine" / "osm" / "grid.json").read_text())
    pinned_at = TREE / "test" / "outshine" / "osm" / "digests.json"
    pinned = json.loads(pinned_at.read_text()) if pinned_at.exists() else {}
    lands = {one["id"]: one for one in grid["terrains"]}
    builds = {one["id"]: one for one in grid["structures"]}

    scratch = pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-osm-grid"
    scratch.mkdir(parents=True, exist_ok=True)

    held, apart, drew, absent, wrong = 0, [], {}, 0, []
    for cell in grid["cells"]:
        name = f"{cell['structure']}-{cell['terrain']}"
        if "absent" in cell:
            absent += 1
            continue
        if told.only and told.only != name:
            continue
        wrote = scratch / f"{name}.scn"
        wrote.write_text(scenario_for(lands[cell["terrain"]], builds[cell["structure"]]))
        ran = subprocess.run([str(CLIENT), "measures", "--rows", "--into", "osm", str(wrote), name],
                             capture_output=True, text=True, timeout=900)
        lines = ran.stdout.splitlines()
        digest = ""
        for line in lines:
            if line.startswith("ROW"):
                digest = line.split("\t")[2]
        if not digest:
            apart.append((name, "the client drew nothing"))
            print(f"APART {name:26s} the client drew nothing")
            continue
        drew[name] = digest

        read = measures_of(lines)
        broke = [f"{what} = {read[what]:.3f} > {most:.3f}"
                 for what, most, _ in CEILINGS if what in read and read[what] > most]
        build = builds[cell["structure"]]
        asked = {"rests": lays_a_road(build), "floats": flies_a_span(build)}
        broke += [f"{what} = {read.get(what, 0.0):.3f} < {least:.3f}"
                  for what, least, when, _ in FLOORS
                  if asked[when] and read.get(what, 0.0) < least]
        if broke:
            wrong.append((name, broke))
            print(f"WRONG {name:26s} {digest}   {'; '.join(broke)}")
            continue

        was = pinned.get(name)
        if told.pin or was is None:
            print(f"PIN   {name:26s} {digest}")
        elif was == digest:
            held += 1
            print(f"HELD  {name:26s} {digest}")
        else:
            apart.append((name, f"{was} -> {digest}"))
            print(f"APART {name:26s} {was} -> {digest}")

    if told.pin:
        pinned.update(drew)
        pinned_at.write_text(json.dumps(pinned, indent=1, sort_keys=True) + "\n")
        print(f"\n{len(drew)} cell(s) pinned into test/outshine/osm/digests.json")

    total = len(grid["cells"]) - absent
    print(f"\n{held} of {total} cell(s) held, {len(apart)} apart, {len(wrong)} WRONG, "
          f"{absent} absent.")
    print("A digest is agreement across time and never correctness. The oracles below it are "
          "arithmetic and they are the ones that say WRONG:")
    for what, most, why in CEILINGS:
        print(f"  <= {most:<7.3f} {what}\n      {why}")
    for what, least, when, why in FLOORS:
        print(f"  >= {least:<7.3f} {what}   [asked where a way {when}]\n      {why}")
    return 1 if (apart or wrong) else 0


if __name__ == "__main__":
    sys.exit(main())
