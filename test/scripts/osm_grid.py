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

    held, apart, drew, absent = 0, [], {}, 0
    for cell in grid["cells"]:
        name = f"{cell['structure']}-{cell['terrain']}"
        if "absent" in cell:
            absent += 1
            print(f"ABSENT {name:26s} {cell['absent']}")
            continue
        if told.only and told.only != name:
            continue
        wrote = scratch / f"{name}.scn"
        wrote.write_text(scenario_for(lands[cell["terrain"]], builds[cell["structure"]]))
        ran = subprocess.run([str(CLIENT), "run", "--rows", "--into", "osm", str(wrote), name],
                             capture_output=True, text=True, timeout=900)
        digest = ""
        for line in ran.stdout.splitlines():
            if line.startswith("ROW"):
                digest = line.split("\t")[2]
        if not digest:
            apart.append((name, "the client drew nothing"))
            print(f"APART {name:26s} the client drew nothing")
            continue
        drew[name] = digest
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
        return 0

    print(f"\n{held} of {len(grid['cells']) - absent} cell(s) held, {len(apart)} apart, "
          f"{absent} absent. A digest is agreement across time and never correctness.")
    return 1 if apart else 0


if __name__ == "__main__":
    sys.exit(main())
