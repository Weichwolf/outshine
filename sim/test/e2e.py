#!/usr/bin/env python3
"""Headless End-to-End mission harness — no rendering, reads the bridge telemetry directly.

Per aircraft/mission: resolve the mission (airports + AGL->ASL via the DEM), spawn the aircraft
container at the takeoff airport, feed iNav the waypoints (MISSION_WPS -> bridge -> MSP_SET_WP), let
real iNav SITL fly NAV WP, and assert against the bridge telemetry. Runs all three aircraft from
their own airports (missions/*.json).

Assertions:
  - ARM      : leaves DISARM
  - TAKEOFF  : climbs clear of the ground (agl > TAKEOFF_AGL)
  - WAYPOINT : each mission waypoint captured within CAPTURE_RADIUS
  - TOUCHDOWN: SKIP (iNav FW-Autoland not wired yet)

Usage: e2e.py [mission ...]   Needs fb-tiles running (for AGL->ASL) + the fb-aircraft image.
"""
import subprocess, sys, time, re, math, os
from mission import load, resolve

TAKEOFF_AGL = 50.0
CAPTURE_RADIUS = 200.0     # m — nav_wp_radius (~100m default) + margin
FLY_TIMEOUT = 300          # s per aircraft
TILES = os.environ.get("TILES_URL", "http://localhost:8081")

LINE = re.compile(r"\[xp_bridge\] (\w+) alt=([-\d.]+|nan) pos=([-\d.]+),([-\d.]+) ")


def sh(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True)


def dist_m(lat1, lon1, lat2, lon2):
    return math.hypot((lat2 - lat1) * 111320.0,
                      (lon2 - lon1) * 111320.0 * math.cos(math.radians(lat1)))


def spawn(aircraft, lat, lon, wps):
    mw = ";".join(f"{w['lat']},{w['lon']},{w['alt_asl']}" for w in wps if w["alt_asl"] is not None)
    sh("podman rm -f fb-aircraft")
    sh(f"podman run -d --name fb-aircraft --network flightboxnet "
       f"-e AIRCRAFT={aircraft} -e FLIGHTBOX_ADDR=fb-flightbox -e TILES_ADDR=fb-tiles:8081 "
       f"-e ORIGIN_LAT={lat} -e ORIGIN_LON={lon} -e FDM_ENGINE=jsbsim -e MISSION_WPS='{mw}' fb-aircraft")


def latest():
    for ln in reversed(sh("podman logs --tail 30 fb-aircraft 2>&1").stdout.splitlines()):
        g = LINE.search(ln)
        if g:
            alt = float("nan") if g.group(2) == "nan" else float(g.group(2))
            return g.group(1), alt, float(g.group(3)), float(g.group(4))
    return None


def run(mf):
    r = resolve(load(mf), TILES)
    t = r["takeoff"]
    wps = r["waypoints"]
    name = f"{r['aircraft']} {t['icao']}/{t['runway']}"
    print(f"\n=== {name}  ({len(wps)} WP)  mission {mf.split('/')[-1]} ===")
    spawn(r["aircraft"], t["lat"], t["lon"], wps)
    armed = took = False
    hit = [False] * len(wps)
    nan = False
    t0 = time.time()
    peak = 0.0
    while time.time() - t0 < FLY_TIMEOUT:
        time.sleep(5)
        s = latest()
        if not s:
            continue
        mode, alt, lat, lon = s
        if alt != alt:
            nan = True
            break
        if mode != "DISARM":
            armed = True
        if alt > TAKEOFF_AGL:
            took = True
        peak = max(peak, alt)
        for i, w in enumerate(wps):
            if not hit[i] and dist_m(lat, lon, w["lat"], w["lon"]) < CAPTURE_RADIUS:
                hit[i] = True
        if all(hit):
            break
    nhit = sum(hit)
    ok = armed and took and all(hit) and not nan
    print(f"  ARM       {'OK' if armed else 'FAIL'}")
    print(f"  TAKEOFF   {'OK (agl>%g)'%TAKEOFF_AGL if took else 'FAIL (peak %.0fm)'%peak}")
    wp_line = f"  WAYPOINT  {nhit}/{len(wps)} captured (<{CAPTURE_RADIUS:g}m)"
    if nan:
        wp_line += "  [NaN/diverged]"
    if nhit != len(wps):
        wp_line += "  FAIL"
    print(wp_line)
    print(f"  TOUCHDOWN SKIP (iNav FW-autoland not wired)")
    print(f"  => {name}: {'PASS (takeoff+waypoints)' if ok else 'FAIL'}")
    return ok


def main():
    missions = sys.argv[1:]
    if not missions:
        from pathlib import Path
        missions = sorted(str(p) for p in (Path(__file__).resolve().parents[1] / "missions").glob("*.json"))
    results = {mf.split('/')[-1]: run(mf) for mf in missions}
    print("\n==== E2E SUMMARY ====")
    for mf, ok in results.items():
        print(f"  {'PASS' if ok else 'FAIL'}  {mf}")
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
