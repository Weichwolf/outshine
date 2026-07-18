#!/usr/bin/env python3
"""Headless End-to-End mission harness — no rendering, reads the bridge telemetry directly.

Per aircraft/mission: spawn the aircraft container at the takeoff airport (ORIGIN = threshold),
let real iNav SITL fly, assert against the bridge telemetry. Runs all three aircraft from their
own airports (missions/*.json).

Phases asserted TODAY (what the current native-NAV stack supports):
  - ARM     : leaves DISARM
  - TAKEOFF : climbs clear of the ground (agl > TAKEOFF_AGL)
  - CLIMB   : gains altitude and does NOT diverge (no NaN)
Pending the MSP-WP command layer (iNav gets no waypoints yet, only launch->NAV): WAYPOINT hits and
TOUCHDOWN assertions are stubbed (reported SKIP), so the harness is honest about coverage.

Usage: e2e.py [mission ...]   (default: all missions).  Needs fb-tiles running + the fb-aircraft image.
"""
import subprocess, sys, time, re
from mission import load, resolve

TAKEOFF_AGL = 50.0        # m — "clear of the ground"
ARM_TIMEOUT = 40          # s
CLIMB_TARGET = 120.0      # m — reached the NAV handoff / airborne regime
FLY_TIMEOUT = 150         # s

LINE = re.compile(r"\[xp_bridge\] (\w+) alt=([-\d.]+|nan) .*?home=([-\d.]+|nan)")


def sh(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True)


def spawn(aircraft, lat, lon):
    sh("podman rm -f fb-aircraft")
    sh(f"podman run -d --name fb-aircraft --network flightboxnet "
       f"-e AIRCRAFT={aircraft} -e FLIGHTBOX_ADDR=fb-flightbox -e TILES_ADDR=fb-tiles:8081 "
       f"-e ORIGIN_LAT={lat} -e ORIGIN_LON={lon} -e FDM_ENGINE=jsbsim fb-aircraft")


def latest():
    """(mode, alt, home) from the last bridge telemetry line, or None."""
    out = sh("podman logs --tail 30 fb-aircraft 2>&1").stdout
    m = None
    for ln in out.splitlines():
        g = LINE.search(ln)
        if g:
            m = g
    if not m:
        return None
    mode = m.group(1)
    alt = float("nan") if m.group(2) == "nan" else float(m.group(2))
    home = float("nan") if m.group(3) == "nan" else float(m.group(3))
    return mode, alt, home


def run(mf):
    r = resolve(load(mf))
    t = r["takeoff"]
    name = f"{r['aircraft']} {t['icao']}/{t['runway']}"
    print(f"\n=== {name}  (mission {mf}) ===")
    spawn(r["aircraft"], t["lat"], t["lon"])
    res = {"arm": None, "takeoff": None, "climb": None, "nan": False}
    t0 = time.time()
    peak = 0.0
    while time.time() - t0 < FLY_TIMEOUT:
        time.sleep(5)
        s = latest()
        if not s:
            continue
        mode, alt, home = s
        if alt != alt:                       # NaN
            res["nan"] = True
            break
        if res["arm"] is None and mode != "DISARM":
            res["arm"] = round(time.time() - t0, 1)
        if res["takeoff"] is None and alt > TAKEOFF_AGL:
            res["takeoff"] = round(time.time() - t0, 1)
        peak = max(peak, alt)
        if res["climb"] is None and alt > CLIMB_TARGET:
            res["climb"] = round(time.time() - t0, 1)
            break
    ok = res["arm"] and res["takeoff"] and res["climb"] and not res["nan"]
    print(f"  ARM      {'@%ss'%res['arm'] if res['arm'] else 'FAIL'}")
    print(f"  TAKEOFF  {'@%ss (agl>%g)'%(res['takeoff'],TAKEOFF_AGL) if res['takeoff'] else 'FAIL'}")
    print(f"  CLIMB    {'@%ss (alt>%g, peak %.0fm)'%(res['climb'],CLIMB_TARGET,peak) if res['climb'] else 'FAIL (peak %.0fm)'%peak}"
          + ("  [NaN/diverged]" if res["nan"] else ""))
    print(f"  WAYPOINT SKIP (MSP-WP command layer not built)")
    print(f"  TOUCHDOWN SKIP (needs waypoints + autoland)")
    print(f"  => {name}: {'PASS (takeoff+climb)' if ok else 'FAIL'}")
    return ok


def main():
    missions = sys.argv[1:]
    if not missions:
        from pathlib import Path
        missions = sorted(str(p) for p in (Path(__file__).resolve().parents[1] / "missions").glob("*.json"))
    results = {mf: run(mf) for mf in missions}
    print("\n==== E2E SUMMARY ====")
    for mf, ok in results.items():
        print(f"  {'PASS' if ok else 'FAIL'}  {mf.split('/')[-1]}")
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
