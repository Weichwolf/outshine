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

WHAT THIS DOES NOT COVER YET: the OSM structures. Each cell today renders its terrain alone, which
makes the ten terrain columns real and the twenty-five structure rows pending. Said plainly so the
count is not read as more than it is.
"""
import argparse
import json
import os
import pathlib
import subprocess
import sys

TREE = pathlib.Path(__file__).resolve().parents[2]
CLIENT = TREE / "build" / "outshine-client"
kAtLat = 49.0
kAtLon = 9.0


def scenario_for(land):
    return f"""<scenario>
  <world lat="{kAtLat}" lon="{kAtLon}" patienceS="6" sightM="12000">
    <relief kind="{land['kind']}" amplitudeM="{land['amplitudeM']}" \
wavelengthM="{land['wavelengthM']}" gradient="{land['gradient']}" bearingDeg="0" seed="1"/>
  </world>
  <render widthPx="1280" heightPx="720" fps="60" fill="0.6" audits="no"/>
  <clock start="2026-06-21T11:34:00Z" rate="1" live="no"/>
  <lighting>
    <key lux="0" elevationDeg="0" bearingDeg="0"/>
  </lighting>
  <views>
    <view id="station" person="first" fovDeg="55">
    <at lat="{kAtLat}" lon="{kAtLon}" heightM="120" samplesHeight="yes" bearingDeg="90" \
pitchDeg="-18"/>
    </view>
  </views>
</scenario>
"""


def main():
    ask = argparse.ArgumentParser()
    ask.add_argument("--pin", action="store_true", help="write the digests instead of scoring them")
    told = ask.parse_args()

    grid = json.loads((TREE / "test" / "outshine" / "osm" / "grid.json").read_text())
    pinned_at = TREE / "test" / "outshine" / "osm" / "digests.json"
    pinned = json.loads(pinned_at.read_text()) if pinned_at.exists() else {}

    scratch = pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-osm-grid"
    scratch.mkdir(parents=True, exist_ok=True)

    held, apart, drew = 0, [], {}
    for land in grid["terrains"]:
        wrote = scratch / f"{land['id']}.scn"
        wrote.write_text(scenario_for(land))
        ran = subprocess.run([str(CLIENT), "run", "--rows", "--into", "osm", str(wrote),
                              land["id"]], capture_output=True, text=True, timeout=900)
        digest = ""
        for line in ran.stdout.splitlines():
            if line.startswith("ROW"):
                digest = line.split("\t")[2]
        if not digest:
            apart.append((land["id"], "the client drew nothing"))
            continue
        drew[land["id"]] = digest
        was = pinned.get(land["id"])
        if told.pin or was is None:
            print(f"PIN   {land['id']:14s} {digest}   {land['why']}")
        elif was == digest:
            held += 1
            print(f"HELD  {land['id']:14s} {digest}")
        else:
            apart.append((land["id"], f"{was} -> {digest}"))
            print(f"APART {land['id']:14s} {was} -> {digest}")

    if told.pin or not pinned:
        pinned_at.write_text(json.dumps(drew, indent=1) + "\n")
        print(f"\n{len(drew)} terrain(s) pinned into test/outshine/osm/digests.json")
        return 0

    print(f"\n{held} of {len(grid['terrains'])} terrain(s) held; "
          f"{len(grid['structures'])} structure(s) and "
          f"{sum(1 for c in grid['cells'] if 'absent' not in c)} cells still pending")
    return 1 if apart else 0


if __name__ == "__main__":
    sys.exit(main())
