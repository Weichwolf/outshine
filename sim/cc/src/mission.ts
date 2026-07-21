// Declarative mission -> resolved geometry. A mission is data (JSON/YAML): takeoff + waypoints (AGL) +
// land + speeds. Resolving it means: runway threshold lat/lon + heading from the airport DB, and each
// waypoint's home-relative altitude from the DEM (ground elevation) — "Terrain lebt im CC". The DEM/DB
// lookups already exist in test/mission.py (one source of truth for E2E and CC); we call it as a pure
// resolver here rather than re-porting the tile decode. The mission stays declarative; this is compute.

import { execSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

export interface Waypoint { lat: number; lon: number; altAgl: number; altRel: number; }
export interface Runway { icao: string; runway: string; lat: number; lon: number; elevM: number; headingDeg: number; }
export interface Speeds { cruiseMs: number; climbMs: number; approachMs: number; stallMs: number; }
export interface Mission {
  name: string; aircraft: string;
  takeoff: Runway; land: Runway; waypoints: Waypoint[];
  speeds: Speeds;
  raw: any;                                    // the declarative spec, verbatim (success/abort criteria etc.)
}

const SIM = resolve(import.meta.dirname, '../../..');   // cc/dist/src -> sim/

/** Resolve a declarative mission file (path relative to sim/, e.g. "missions/c172.json") to geometry. */
export function resolveMission(specPath: string): Mission {
  // single line (semicolons): a `python3 -c` argument can carry no real newlines through execSync
  const py = `import sys,json; sys.path.insert(0,'test'); import mission; ` +
    `m=json.load(open(sys.argv[1])); R=mission.resolve(m,tiles_url='http://localhost:8081'); ` +
    `print(json.dumps({"name":m.get("name",""),"aircraft":R["aircraft"],"takeoff":R["takeoff"],"land":R["land"],"waypoints":R["waypoints"],"raw":m}))`;
  const out = execSync(`python3 -c ${JSON.stringify(py)} ${JSON.stringify(specPath)}`,
                       { cwd: SIM, encoding: 'utf8', maxBuffer: 8 << 20 });
  const R = JSON.parse(out);
  const rw = (o: any): Runway => ({ icao: o.icao, runway: o.runway, lat: o.lat, lon: o.lon, elevM: o.elev_m, headingDeg: o.heading_deg });
  const sp = R.raw.speeds ?? {};
  return {
    name: R.name, aircraft: R.aircraft,
    takeoff: rw(R.takeoff), land: rw(R.land),
    waypoints: R.waypoints.map((w: any) => ({ lat: w.lat, lon: w.lon, altAgl: w.alt_agl, altRel: w.alt_rel })),
    speeds: { cruiseMs: sp.cruise_ms ?? 20, climbMs: sp.climb_ms ?? 18, approachMs: sp.approach_ms ?? 16, stallMs: sp.stall_ms ?? 11 },
    raw: R.raw,
  };
}

export function readJson(path: string): any { return JSON.parse(readFileSync(path, 'utf8')); }
