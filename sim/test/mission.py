#!/usr/bin/env python3
"""Mission resolution layer — shared by the headless E2E harness and (mirrored) the command center.

A mission (missions/*.json) names airports+runways and gives waypoints as AGL. This resolves it into
pure GPS/ASL commands for iNav:
  - runway ident -> threshold lat/lon + heading (from the embedded OurAirports DB, geo/airports.json)
  - waypoint alt_agl -> alt_asl = alt_agl + ground_elev(lat,lon)   [DEM lives here, iNav gets ASL only]

Ground elevation comes from fb-tiles /elev (the same DEM the renderer uses). Without a reachable
tiles URL the ASL fields stay None (structure still resolves) so the resolver is unit-testable offline.
"""
import json, os, sys, urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]          # sim/
AIRPORTS = json.loads((ROOT / "geo" / "airports.json").read_text())


def runway(icao, ident):
    ap = AIRPORTS.get(icao)
    if not ap:
        raise KeyError(f"airport {icao} not in DB")
    for rw in ap["runways"]:
        if rw["ident"] == ident:
            return rw
    raise KeyError(f"runway {ident} not at {icao} (have {[r['ident'] for r in ap['runways']]})")


def ground_elev_m(lat, lon, tiles_url):
    """DEM ground elevation (m ASL) at lat/lon via fb-tiles /elev, or None if unreachable."""
    if not tiles_url:
        return None
    try:
        u = f"{tiles_url}/elev?lat={lat}&lon={lon}&block=1"
        with urllib.request.urlopen(u, timeout=10) as r:
            v = float(r.read().decode().strip())
            return v if v > -1e8 else None
    except Exception:
        return None


def resolve(mission, tiles_url=None):
    """Mission dict -> resolved GPS/ASL plan. iNav never sees AGL: every altitude is ASL here."""
    to = runway(mission["takeoff"]["airport"], mission["takeoff"]["runway"])
    ld = runway(mission["land"]["airport"], mission["land"]["runway"])
    wps = []
    for w in mission["waypoints"]:
        g = ground_elev_m(w["lat"], w["lon"], tiles_url)
        wps.append({"lat": w["lat"], "lon": w["lon"], "alt_agl": w["alt_agl"],
                    "ground_m": g, "alt_asl": (w["alt_agl"] + g) if g is not None else None})
    return {
        "aircraft": mission["aircraft"],
        "takeoff": {"icao": mission["takeoff"]["airport"], "runway": to["ident"],
                    "lat": to["lat"], "lon": to["lon"], "elev_m": to["elev_m"], "heading_deg": to["heading_deg"]},
        "waypoints": wps,
        "land": {"icao": mission["land"]["airport"], "runway": ld["ident"],
                 "lat": ld["lat"], "lon": ld["lon"], "elev_m": ld["elev_m"], "heading_deg": ld["heading_deg"]},
    }


def load(name):
    """Load a mission by name (missions/<name>.json) or path."""
    p = Path(name)
    if not p.exists():
        p = ROOT / "missions" / (name if name.endswith(".json") else name + ".json")
    return json.loads(p.read_text())


def _selftest():
    tiles = os.environ.get("TILES_URL")           # optional: http://localhost:8081
    for mf in sorted((ROOT / "missions").glob("*.json")):
        m = load(str(mf))
        r = resolve(m, tiles)
        t, l = r["takeoff"], r["land"]
        assert t["heading_deg"] is not None and l["heading_deg"] is not None
        assert len(r["waypoints"]) == len(m["waypoints"])
        print(f"OK {mf.name}: {r['aircraft']}  {t['icao']}/{t['runway']}@{t['heading_deg']}° "
              f"-> {len(r['waypoints'])} WP -> {l['icao']}/{l['runway']}@{l['heading_deg']}°")
        for w in r["waypoints"]:
            asl = f"{w['alt_asl']:.0f}m ASL" if w["alt_asl"] is not None else "ASL=?(no tiles)"
            print(f"    WP {w['lat']:.4f},{w['lon']:.4f}  {w['alt_agl']}m AGL -> {asl}")
    print("mission.py selftest: all resolved")


if __name__ == "__main__":
    if len(sys.argv) > 1:
        print(json.dumps(resolve(load(sys.argv[1]), os.environ.get("TILES_URL")), indent=2))
    else:
        _selftest()
