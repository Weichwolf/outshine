// Declarative mission -> resolved geometry, NATIVE TypeScript (no Python). A mission is data: takeoff +
// waypoints (AGL) + land. Resolving it: runway threshold lat/lon + heading from the OurAirports world DB
// (geo/airports.json), and each waypoint's home-relative altitude from the DEM (ground elevation via the
// fb-tiles /elev endpoint) — "Terrain lebt im CC". iNav only ever gets home-relative GPS numbers. The
// aircraft contributes its Vs/Vc (profile.env); the scenario speed envelope comes from speeds.ts.

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { scenarioSpeeds, vne as vneOfVc, type Scenario, type Band } from './speeds.js';

const SIM = resolve(import.meta.dirname, '../../..');   // cc/dist/src -> sim/

interface RunwayRec { ident: string; heading_deg: number; lat: number; lon: number; elev_m: number; length_m: number | null; surface: string; }
const AIRPORTS: Record<string, { icao: string; runways: RunwayRec[] }> =
  JSON.parse(readFileSync(`${SIM}/geo/airports.json`, 'utf8'));

export interface Waypoint { lat: number; lon: number; altAgl: number; altRel: number | null; }
export interface Runway { icao: string; runway: string; lat: number; lon: number; elevM: number; headingDeg: number; }
export interface Mission {
  name: string; aircraft: string;
  takeoff: Runway; land: Runway; waypoints: Waypoint[];
  vs: number; vc: number; vne: number; vmin: number; speeds: Record<Scenario, Band>;   // aircraft speed envelope
  raw: any;                                                                             // the declarative spec, verbatim
}

/** Runway threshold record by airport ICAO + runway ident, from the world DB. */
export function runway(icao: string, ident: string): RunwayRec {
  const ap = AIRPORTS[icao];
  if (!ap) throw new Error(`airport ${icao} not in DB`);
  const rw = ap.runways.find((r) => r.ident === ident);
  if (!rw) throw new Error(`runway ${ident} not at ${icao} (have ${ap.runways.map((r) => r.ident).join(',')})`);
  return rw;
}

/** DEM ground elevation (m ASL) at lat/lon via fb-tiles /elev, or null if unreachable. */
export async function groundElev(lat: number, lon: number, tilesUrl: string): Promise<number | null> {
  if (!tilesUrl) return null;
  try {
    const r = await fetch(`${tilesUrl}/elev?lat=${lat}&lon=${lon}&block=1`);
    if (!r.ok) return null;
    const v = Number((await r.text()).trim());
    return Number.isFinite(v) && v > -1e8 ? v : null;
  } catch { return null; }
}

// Vs/Vc from the aircraft; Vne is the structural never-exceed — usually derived (≈1.9·Vc) but a per-aircraft
// FB_VNE override wins where that ratio misfits (a fast jet whose FB_CRUISE is a nav reference, not its
// throttle cruise). One honest number, not a re-materialised table (see speeds.ts).
function vsVc(ac: string): { vs: number; vc: number; vne: number; vmin: number } {
  const txt = readFileSync(`${SIM}/aircraft/models/${ac}/profile.env`, 'utf8');
  const g = (k: string, d: number) => { const m = txt.match(new RegExp(`^\\s*${k}\\s*=\\s*([0-9.]+)`, 'm')); return m ? Number(m[1]) : d; };
  const vs = g('FB_STALL', 11), vc = g('FB_CRUISE', 20);
  // vmin = minimum safe maneuvering speed. Most airframes fly to ~1.2·Vs; a relaxed-stability jet departs
  // far above the 1g stall, so FB_VMIN overrides it (energy management holds the aircraft above this).
  return { vs, vc, vne: g('FB_VNE', vneOfVc(vc)), vmin: g('FB_VMIN', 1.2 * vs) };
}

/** Resolve a declarative mission (name or path under sim/) to a flyable GPS/ASL plan. */
export async function resolveMission(spec: string, tilesUrl = process.env.TILES_URL ?? 'http://localhost:8081'): Promise<Mission> {
  const path = spec.endsWith('.json') || spec.includes('/') ? spec : `missions/${spec}.json`;
  const m = JSON.parse(readFileSync(resolve(SIM, path), 'utf8'));
  const to = runway(m.takeoff.airport, m.takeoff.runway);
  const ld = runway(m.land.airport, m.land.runway);
  const home = to.elev_m;
  const waypoints: Waypoint[] = [];
  for (const w of m.waypoints ?? []) {
    const g = await groundElev(w.lat, w.lon, tilesUrl);
    const asl = g != null ? w.alt_agl + g : null;
    waypoints.push({ lat: w.lat, lon: w.lon, altAgl: w.alt_agl, altRel: asl != null ? asl - home : null });
  }
  const { vs, vc, vne, vmin } = vsVc(m.aircraft);
  const rw = (o: RunwayRec, icao: string): Runway => ({ icao, runway: o.ident, lat: o.lat, lon: o.lon, elevM: o.elev_m, headingDeg: o.heading_deg });
  return {
    name: m.name ?? '', aircraft: m.aircraft,
    takeoff: rw(to, m.takeoff.airport), land: rw(ld, m.land.airport),
    waypoints, vs, vc, vne, vmin, speeds: scenarioSpeeds(vs, vc), raw: m,
  };
}
